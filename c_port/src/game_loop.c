#include "game_loop.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "descriptor.h"
#include "log.h"
#include "net.h"
#include "pulse.h"

/* 100,000 microseconds = 100ms = 1 pulse, matching the original's literal
 * OPT_USEC pulse unit (sys/socket.h). select()'s timeout doubles as the
 * pulse clock: the scheduler runs once per loop iteration regardless of
 * whether select() returned ready sockets or simply timed out. */
#define OPT_USEC 100000

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
                   account_name) != 7)
            continue;
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

    while (!g_shutdown) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(ms.listen_fd, &readfds);
        int maxfd = ms.listen_fd;

        for (descriptor_t *d = g_descriptors; d; d = d->next) {
            FD_SET(d->fd, &readfds);
            if (d->fd > maxfd)
                maxfd = d->fd;
        }

        struct timeval tv = { .tv_sec = 0, .tv_usec = OPT_USEC };
        int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
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
                if (nd)
                    snprintf(nd->ip, sizeof(nd->ip), "%s", peer_ip);
            }
        }

        descriptor_t *d = g_descriptors;
        while (d) {
            descriptor_t *next = d->next;
            if (FD_ISSET(d->fd, &readfds)) {
                if (!descriptor_process_input(d))
                    descriptor_destroy(d);
            }
            d = next;
        }

        pulse_count++;
        pulse_scheduler_run(pulse_count);

        /* The single prompt authority (Session 21): any playing,
         * non-editing connection that received output this iteration --
         * from its own command, someone's say, a combat round, a
         * broadcast -- gets exactly one fresh prompt. Written directly
         * via socket_write so it doesn't re-mark needs_prompt. */
        for (descriptor_t *p = g_descriptors; p; p = p->next) {
            if (p->needs_prompt && p->state == CONN_PLAYING && p->edit_kind == EDIT_NONE) {
                socket_write(p->fd, "\r\n> ", 4);
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
