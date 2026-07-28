/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "game_loop.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "being.h"
#include "descriptor.h"
#include "hostname_resolve.h"
#include "log.h"
#include "net.h"
#include "pulse.h"

/* 100,000 microseconds = 100ms = 1 pulse, matching the original's literal
 * OPT_USEC pulse unit (sys/socket.h). select()'s timeout doubles as the
 * loop's own wake-up clock, but the pulse scheduler is gated on real
 * elapsed wall-clock time (see now_usec()/next_pulse_due below), NOT on
 * how many times the loop happens to iterate. */
#define OPT_USEC 100000

/* Bug found 2026-07-12 (chasing a flaky trigger-damage smoke test): pulse
 * advancement used to be "once per loop iteration", and select() returns
 * IMMEDIATELY whenever any watched socket already has data ready -- not
 * just on its OPT_USEC timeout. Under concurrent connection traffic (a
 * test sweep, several players active at once) that let the loop iterate,
 * and therefore the pulse counter advance, far faster than real time,
 * so every pulse-gated system (REGEN_PULSES, COMBAT_ROUND_PULSES, the
 * ~60s zone/gametime/mob-AI ticks, etc. -- see main.c's pulse_register()
 * calls) could fire many times more often than its constant implies. Caps
 * how many pulse boundaries get caught up in one go after a genuine stall
 * (a debugger pause, a slow query, the process being suspended) so that
 * doesn't queue an unbounded catch-up burst either -- resyncs to "now"
 * instead once the cap is hit. */
#define MAX_PULSE_CATCHUP 50 /* ~5s worth at 100ms/pulse */

static long long now_usec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

static volatile sig_atomic_t g_shutdown = 0;
static int g_listen_fd = -1;

/* Boot-phase state (see game_loop.h's game_loop_boot_open()/
 * game_loop_boot_poll() doc comments): the listening socket lives here
 * from the moment boot_open() runs, well before game_loop_run() itself
 * starts -- so a connection arriving during main()'s DB/world-load work
 * can be accepted and held immediately instead of sitting silent in the
 * kernel's backlog. */
static main_socket_t g_boot_ms = { .listen_fd = -1 };
static bool g_boot_is_copyover = false;
static char g_boot_copyover_file[512];
static bool g_boot_notified_existing = false;

#define MAX_BOOT_PENDING 32
static int g_boot_pending_fd[MAX_BOOT_PENDING];
static char g_boot_pending_ip[MAX_BOOT_PENDING][46];
static int g_boot_pending_count = 0;

/* Returns the live listening socket's fd (-1 before game_loop_run() sets
 * it up) so cmd_copyover.c can write it into the recovery file for the
 * next exec to inherit. */
int game_loop_listen_fd(void) {
    return g_listen_fd;
}

/* Opens (or, for a copyover, adopts from the recovery file's "listen"
 * line) the listening socket as early as possible in main()'s startup,
 * well before game_loop_run() itself begins -- see game_loop.h for why.
 * Resets all boot-phase state so it's safe to call once per process
 * start. Returns false on a fatal bind/listen error. */
bool game_loop_boot_open(int port, const char *copyover_file) {
    g_boot_ms.listen_fd = -1;
    g_boot_is_copyover = false;
    g_boot_notified_existing = false;
    g_boot_pending_count = 0;

    if (copyover_file) {
        /* Peek the recovery file just for the inherited listen fd -- the
         * full "conn ..." adoption into real descriptor_t's still happens
         * later, in copyover_recover(), once game_loop_run() actually
         * starts. Deliberately does NOT unlink the file (copyover_recover()
         * does that once it's done reading it for real). */
        FILE *f = fopen(copyover_file, "r");
        if (f) {
            char line[512];
            if (fgets(line, sizeof(line), f)
                && sscanf(line, "listen %d", &g_boot_ms.listen_fd) == 1
                && g_boot_ms.listen_fd >= 0) {
                g_boot_is_copyover = true;
                snprintf(g_boot_copyover_file, sizeof(g_boot_copyover_file), "%s", copyover_file);
            } else {
                g_boot_ms.listen_fd = -1;
            }
            fclose(f);
        }
    }

    if (g_boot_ms.listen_fd < 0 && !main_socket_open(&g_boot_ms, port))
        return false;

    return true;
}

/* Called between each slow step of main()'s boot sequence to send
 * `message` to anyone waiting: existing copyover connections (once) and
 * any newly-accepted connection since the last call, which is held (fd
 * only) until game_loop_run() hands it a real descriptor_t. Returns how
 * many distinct sockets were pinged this call, for a boot-status log. */
int game_loop_boot_poll(const char *message) {
    int notified = 0;
    size_t msg_len = strlen(message);

    /* Existing (copyover) connections only need this once -- they're
     * frozen the whole boot window, not repeatedly reconnecting. */
    if (g_boot_is_copyover && !g_boot_notified_existing) {
        g_boot_notified_existing = true;
        FILE *f = fopen(g_boot_copyover_file, "r");
        if (f) {
            char line[512];
            fgets(line, sizeof(line), f); /* skip the "listen %d" header */
            while (fgets(line, sizeof(line), f)) {
                int fd;
                if (sscanf(line, "conn %d", &fd) == 1 && fd >= 0) {
                    socket_write(fd, message, msg_len);
                    notified++;
                }
            }
            fclose(f);
        }
        log_info("Boot: notified %d existing connection(s) riding out a copyover reboot.", notified);
    }

    char ip[46];
    int fd;
    while ((fd = main_socket_accept(&g_boot_ms, ip, sizeof(ip))) >= 0) {
        socket_write(fd, message, msg_len);
        if (g_boot_pending_count < MAX_BOOT_PENDING) {
            g_boot_pending_fd[g_boot_pending_count] = fd;
            snprintf(g_boot_pending_ip[g_boot_pending_count], sizeof(g_boot_pending_ip[0]), "%s", ip);
            g_boot_pending_count++;
        } else {
            /* Backlog cap hit -- extremely unlikely (32 simultaneous new
             * connections mid-boot), but close rather than leak the fd. */
            log_error("Boot: pending-connection cap hit, closing fd %d from %s.", fd, ip);
            close(fd);
        }
        notified++;
        log_info("Boot: new connection (fd %d) from %s arrived during startup -- held.", fd, ip);
    }

    return notified;
}

/* Sets the same shutdown flag SIGINT triggers (see handle_sigint()
 * below), so shutdown.c can request a clean stop from in-game code
 * instead of a signal. The loop finishes its current iteration, then
 * closes everything before game_loop_run() returns. */
void game_loop_request_shutdown(void) {
    g_shutdown = 1;
}

/* SIGINT handler (Ctrl+C): just flips the shutdown flag rather than
 * doing any real work here -- signal-handler context can't safely call
 * most of what a clean shutdown needs, so the main loop notices the flag
 * and does the actual teardown itself. */
static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

/* Adopts the listening socket and every player connection recorded in the
 * copyover recovery file (see cmd_copyover.c for the writer and the file
 * format). Returns false if the file can't be read/parsed -- the caller
 * then opens a fresh listening socket as if this were a cold boot. */
static bool copyover_recover(const char *file, main_socket_t *ms) {
    FILE *f = fopen(file, "r");
    if (!f) {
        log_error("copyover: cannot open recovery file '%s'", file);
        return false;
    }

    char line[512];
    if (!fgets(line, sizeof(line), f)
        || sscanf(line, "listen %d", &ms->listen_fd) != 1
        || ms->listen_fd < 0) {
        log_error("copyover: bad recovery file header");
        fclose(f);
        return false;
    }

    int restored = 0, dropped = 0;
    while (fgets(line, sizeof(line), f)) {
        int fd, room_vnum, color;
        long account_id;
        char peer_ip[46], char_name[64], account_name[80];
        if (sscanf(line, "conn %d %ld %d %d %45s %63s %79[^\r\n]",
                   &fd, &account_id, &room_vnum, &color, peer_ip, char_name,
                   account_name) != 7) {
            /* Unparseable (e.g. an older recovery-file format): CLOSE the
             * inherited fd rather than leaking it -- an unowned open
             * socket looks like a hung MUD to the client on its far end
             * (learned the hard way, Session 21). */
            if (sscanf(line, "conn %d", &fd) == 1 && fd >= 0)
                close(fd);
            dropped++;
            continue;
        }
        if (descriptor_copyover_adopt(fd, account_id, room_vnum, color != 0,
                                      peer_ip, char_name, account_name))
            restored++;
        else
            dropped++;
    }
    fclose(f);
    unlink(file);
    log_info("Copyover recovery: %d connection(s) restored, %d dropped.", restored, dropped);
    return true;
}

/* The main select()-driven accept/read/pulse loop -- the heart of the
 * server once boot has finished. Reuses the listening socket
 * game_loop_boot_open() already set up (or adopts a copyover's, via
 * copyover_recover()), hands off anyone who connected during the boot
 * window, then loops: accept new connections, poll finished hostname
 * lookups, service readable/writable descriptors, advance pulses for
 * every boundary actually crossed since the last check (see
 * MAX_PULSE_CATCHUP above), and issue one prompt per descriptor that
 * needs one. Runs until game_loop_request_shutdown()/SIGINT sets the
 * shutdown flag, then tears down every descriptor and the listening
 * socket before returning. Returns 0 on clean shutdown, nonzero if the
 * listening socket couldn't be set up. */
int game_loop_run(int port, const char *copyover_file) {
    /* Reuse the socket game_loop_boot_open() already opened at the very
     * start of main() -- calling main_socket_open() again here (the old
     * behavior) would try to re-bind a port that's already bound and fail.
     * copyover_recover() below still re-parses the recovery file for the
     * real "conn ..." adoption, and harmlessly re-sets the same listen_fd
     * value while doing so. */
    main_socket_t ms = g_boot_ms;

    bool recovered = copyover_file && copyover_recover(copyover_file, &ms);
    if (!recovered && ms.listen_fd < 0 && !main_socket_open(&ms, port))
        return 1;
    g_listen_fd = ms.listen_fd;

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    if (recovered)
        log_info("Copyover complete -- still listening on port %d.", port);
    else
        log_info("Listening on port %d. Press Ctrl+C to stop.", port);

    /* Hand off anyone who connected during main()'s boot window (see
     * game_loop_boot_open()/game_loop_boot_poll()) straight into the game,
     * exactly as if they'd just been accepted in the loop below. */
    for (int i = 0; i < g_boot_pending_count; i++) {
        int fd = g_boot_pending_fd[i];
        descriptor_t *nd = descriptor_create(fd);
        if (nd) {
            snprintf(nd->ip, sizeof(nd->ip), "%s", g_boot_pending_ip[i]);
            hostname_resolve_start(fd, nd->ip);
            log_info("Boot: handing off held connection (fd %d) from %s now that startup is complete.",
                      fd, nd->ip);
        } else {
            close(fd);
        }
    }
    g_boot_pending_count = 0;

    long pulse_count = 0;
    long long next_pulse_due = now_usec() + OPT_USEC;

    while (!g_shutdown) {
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_SET(ms.listen_fd, &readfds);
        int maxfd = ms.listen_fd;

        for (descriptor_t *d = g_descriptors; d; d = d->next) {
            FD_SET(d->fd, &readfds);
            /* Only watch for writability while output is actually backed
             * up (see descriptor_write()/descriptor_flush_output() in
             * descriptor.c) -- the overwhelming majority of descriptors
             * have nothing pending, and a socket is writable almost all
             * the time, so unconditionally watching it would just spin
             * select() uselessly. */
            if (d->out_len > 0)
                FD_SET(d->fd, &writefds);
            if (d->fd > maxfd)
                maxfd = d->fd;
        }

        struct timeval tv = { .tv_sec = 0, .tv_usec = OPT_USEC };
        int ready = select(maxfd + 1, &readfds, &writefds, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            log_error("select() failed");
            break;
        }

        if (FD_ISSET(ms.listen_fd, &readfds)) {
            int fd;
            char peer_ip[46];
            while ((fd = main_socket_accept(&ms, peer_ip, sizeof(peer_ip))) >= 0) {
                log_info("New connection (fd %d) from %s.", fd, peer_ip);
                descriptor_t *nd = descriptor_create(fd);
                if (nd) {
                    snprintf(nd->ip, sizeof(nd->ip), "%s", peer_ip);
                    hostname_resolve_start(fd, peer_ip);
                }
            }
        }

        /* Apply any reverse-DNS lookups that finished since the last tick
         * (see hostname_resolve.h -- lookups run off-thread so a slow one
         * never stalls this loop). */
        hostname_resolve_poll();

        descriptor_t *d = g_descriptors;
        while (d) {
            descriptor_t *next = d->next;
            /* Flush any backed-up output before reading -- a descriptor
             * that's been unwritable for a while (backlog full) is
             * treated as dead, same as a failed read. */
            if (FD_ISSET(d->fd, &writefds) && !descriptor_flush_output(d)) {
                descriptor_destroy(d);
                d = next;
                continue;
            }
            if (FD_ISSET(d->fd, &readfds)) {
                if (!descriptor_process_input(d))
                    descriptor_destroy(d);
            }
            d = next;
        }

        /* Real-elapsed-time gate (see MAX_PULSE_CATCHUP above) -- fires
         * every pulse boundary actually crossed since the last check,
         * instead of exactly once per loop iteration. */
        long long t = now_usec();
        int caught_up = 0;
        while (t >= next_pulse_due && caught_up < MAX_PULSE_CATCHUP) {
            pulse_count++;
            pulse_scheduler_run(pulse_count);
            next_pulse_due += OPT_USEC;
            caught_up++;
        }
        if (caught_up == MAX_PULSE_CATCHUP)
            next_pulse_due = t + OPT_USEC;

        /* The single prompt authority (Session 21): any playing,
         * non-editing connection that received output this iteration --
         * from its own command, someone's say, a combat round, a
         * broadcast -- gets exactly one fresh prompt. Written directly
         * via descriptor_write (queues/retries same as any other output)
         * so it doesn't re-mark needs_prompt. */
        for (descriptor_t *p = g_descriptors; p; p = p->next) {
            if (p->needs_prompt && p->state == CONN_PLAYING && p->edit_kind == EDIT_NONE
                && p->page_len == 0) {
                /* Prompt customization (cmd_prompt.c): render the player's
                 * chosen stats ahead of the "> ", each toggled
                 * independently so any combination renders together
                 * (e.g. "HP: 25 Gold: 40 > "). */
                /* A single \r\n separates the previous output from the prompt
                 * (user request: insert a \r\n before each new prompt -- was
                 * doubled to "\r\n\r\n" here, an extra stray blank line the
                 * user later asked removed "in front and in back" of the
                 * prompt, i.e. both branches below). */
                /* Combat lockout countdown (user 2026-07-28: "when
                 * fighting, add after the prompt <seconds of lockout>")
                 * -- shown whenever actually fighting AND still lagged
                 * from a swing/skill (being_get_wait(), 10 pulses/real
                 * second), regardless of the toggle-able stat prefixes
                 * below; an immortal's being_get_wait() is always a
                 * no-op 0, so this never appears for them. */
                char waitbuf[16] = "";
                if (p->character && p->character->fighting) {
                    int wait = being_get_wait(p->character);
                    if (wait > 0)
                        snprintf(waitbuf, sizeof(waitbuf), "[%d.%ds] ", wait / 10, wait % 10);
                }
                if (p->character && p->character->prompt_flags) {
                    char pbuf[208];
                    size_t pn = (size_t)snprintf(pbuf, sizeof(pbuf), "\r\n");
                    if (p->character->prompt_flags & PROMPT_FLAG_HP)
                        pn += (size_t)snprintf(pbuf + pn, sizeof(pbuf) - pn, "HP: %d ",
                                               p->character->progress.hp);
                    if (p->character->prompt_flags & PROMPT_FLAG_GOLD)
                        pn += (size_t)snprintf(pbuf + pn, sizeof(pbuf) - pn, "Gold: %d ",
                                               p->character->progress.gold);
                    if (p->character->prompt_flags & PROMPT_FLAG_VIT)
                        pn += (size_t)snprintf(pbuf + pn, sizeof(pbuf) - pn, "Vit: %d ",
                                               p->character->progress.vit);
                    if (p->character->prompt_flags & PROMPT_FLAG_EXP)
                        pn += (size_t)snprintf(pbuf + pn, sizeof(pbuf) - pn, "Exp: %ld ",
                                               p->character->progress.experience);
                    if (p->character->prompt_flags & PROMPT_FLAG_EXPNEED) {
                        long need = 0;
                        if (p->character->progress.level < MORTAL_LEVEL_MAX)
                            need = progress_xp_for_level(p->character->progress.level + 1)
                                   - p->character->progress.experience;
                        if (need < 0)
                            need = 0;
                        pn += (size_t)snprintf(pbuf + pn, sizeof(pbuf) - pn, "ExpNeed: %ld ", need);
                    }
                    pn += (size_t)snprintf(pbuf + pn, sizeof(pbuf) - pn, "> %s", waitbuf);
                    descriptor_write(p, pbuf, pn);
                } else {
                    char pbuf[32];
                    size_t pn = (size_t)snprintf(pbuf, sizeof(pbuf), "\r\n> %s", waitbuf);
                    descriptor_write(p, pbuf, pn);
                }
                p->needs_prompt = false;
            }
        }
    }

    log_info("Shutting down.");
    descriptor_t *d = g_descriptors;
    while (d) {
        descriptor_t *next = d->next;
        descriptor_destroy(d);
        d = next;
    }
    main_socket_close(&ms);
    return 0;
}
