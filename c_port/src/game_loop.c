/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "game_loop.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
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

int game_loop_listen_fd(void) {
    return g_listen_fd;
}

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

int game_loop_run(int port, const char *copyover_file) {
    main_socket_t ms;
    ms.listen_fd = -1;

    bool recovered = copyover_file && copyover_recover(copyover_file, &ms);
    if (!recovered && !main_socket_open(&ms, port))
        return 1;
    g_listen_fd = ms.listen_fd;

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    if (recovered)
        log_info("Copyover complete -- still listening on port %d.", port);
    else
        log_info("Listening on port %d. Press Ctrl+C to stop.", port);

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
                 * chosen stats ahead of the "> ". */
                /* A blank line separates the previous output from the prompt
                 * (user request: insert a \r\n before each new prompt). */
                if (p->character && (p->character->prompt_flags & PROMPT_FLAG_HP)) {
                    char pbuf[48];
                    int pn = snprintf(pbuf, sizeof(pbuf), "\r\n\r\nHP: %d > ",
                                      p->character->progress.hp);
                    descriptor_write(p, pbuf, (size_t)pn);
                } else {
                    descriptor_write(p, "\r\n\r\n> ", 6);
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
