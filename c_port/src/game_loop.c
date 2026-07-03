#include "game_loop.h"

#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>

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

static void handle_sigint(int sig) {
    (void)sig;
    g_shutdown = 1;
}

int game_loop_run(int port) {
    main_socket_t ms;
    if (!main_socket_open(&ms, port))
        return 1;

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

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
            while ((fd = main_socket_accept(&ms)) >= 0) {
                log_info("New connection (fd %d).", fd);
                descriptor_create(fd);
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
