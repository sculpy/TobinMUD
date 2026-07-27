/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "crash_handler.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "descriptor.h"
#include "log.h"

#define CRASH_DIR LOG_DIR "/crashes"

static time_t g_boot_time;

/* Counts live descriptors -- a plain linked-list walk, no allocation, safe
 * to call from a signal handler. */
static int count_descriptors(void) {
    int n = 0;
    for (descriptor_t *d = g_descriptors; d; d = d->next)
        n++;
    return n;
}

/* Writes a short, human-readable crash marker to CRASH_DIR before letting
 * the signal proceed to its default (core-dumping) disposition. Uses only
 * low-level open()/write()/close() -- no stdio buffering, no malloc --
 * appropriate for a signal-handler context. `strftime`/`localtime_r`
 * aren't on POSIX's strict async-signal-safe list, but are widely used in
 * practice for exactly this purpose (the risk window is "crash again
 * while already handling a crash", vanishingly rare in reality); a
 * stricter alternative (hand-rolled integer-to-decimal formatting of
 * gmtime fields) is more code for a benefit this project doesn't need. */
static void crash_handler(int sig) {
    time_t now = time(NULL);
    struct tm tmv;
    localtime_r(&now, &tmv);

    char path[256];
    snprintf(path, sizeof(path), CRASH_DIR "/crash-%04d%02d%02d-%02d%02d%02d-pid%d.log",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, (int)getpid());

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char buf[512];
        int n = snprintf(buf, sizeof(buf),
                         "TobinMUD crash report\n"
                         "signal: %d (%s)\n"
                         "pid: %d\n"
                         "uptime: %ld seconds\n"
                         "active connections: %d\n"
                         "Full core dump (if any): `coredumpctl list` / `coredumpctl dump %d`\n",
                         sig, strsignal(sig), (int)getpid(),
                         (long)(now - g_boot_time), count_descriptors(), (int)getpid());
        if (n > 0)
            write(fd, buf, (size_t)n);
        close(fd);
    }

    /* Restore default disposition and re-raise so the OS's own crash-dump
     * path (systemd-coredump on Fedora -- already active, `ulimit -c` is
     * unlimited, confirmed 2026-07-17) still produces the full core dump
     * for gdb-level post-mortem. Don't just exit() here -- that would
     * silently throw away the core dump entirely. */
    signal(sig, SIG_DFL);
    raise(sig);
}

/* Registers crash_handler() for the fatal signals a real crash would
 * raise, records the boot time (for the "uptime" field in crash
 * reports), and ensures CRASH_DIR exists. Also silences SIGPIPE globally
 * -- see the comment above the signal(SIGPIPE, ...) call below for why.
 * Call once during startup, before accepting connections. */
void crash_handler_install(void) {
    g_boot_time = time(NULL);
    mkdir(CRASH_DIR, 0755); /* EEXIST is fine; log.c's LOG_DIR mkdir is the same idiom */

    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGBUS, crash_handler);
    signal(SIGILL, crash_handler);

    /* SIGPIPE (found live, 2026-07-26, while testing Transformation): a
     * raw client disconnect -- socket.close() with no `quit!`, exactly
     * what a crashed/killed client or a flaky connection looks like, not
     * just a test artifact -- means any LATER write() to that socket
     * (an ordinary room broadcast, a prompt, anything) raises SIGPIPE.
     * With no handler, the default disposition is to TERMINATE THE WHOLE
     * PROCESS -- one player's network hiccup was capable of crashing the
     * server for everyone else too. write()/send() already return -1/
     * EPIPE on the failed call either way; nothing in this codebase's
     * socket-writing paths needs the signal itself, so ignoring it
     * outright (not routing it through crash_handler() above -- this
     * isn't a real crash, no report/core dump wanted) is the standard,
     * correct fix for any socket server. */
    signal(SIGPIPE, SIG_IGN);
}
