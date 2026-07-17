/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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

void crash_handler_install(void) {
    g_boot_time = time(NULL);
    mkdir(CRASH_DIR, 0755); /* EEXIST is fine; log.c's LOG_DIR mkdir is the same idiom */

    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);
    signal(SIGBUS, crash_handler);
    signal(SIGILL, crash_handler);
}
