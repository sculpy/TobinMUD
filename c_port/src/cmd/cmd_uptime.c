/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <time.h>

#include "game_loop.h"

/* `uptime`: how long this server generation has been running, and since
 * when (TODO.md priority item, 2026-08-02). No SneezyMUD equivalent was
 * found to port verbatim, so this follows the classic DIKU/CircleMUD
 * "Up since <date>, Y days, H hours, M minutes, S seconds" shape.
 * tobin_boot_time() (main.c/game_loop.h) is set unconditionally near the
 * top of main() -- a copyover successor gets its own fresh timestamp
 * exactly like a cold boot, since a copyover doesn't preserve any other
 * in-memory world state either, so "uptime" here means "time since this
 * process last started", the same thing a `copyover` visibly resets. */
bool cmd_uptime(descriptor_t *d, const char *args) {
    (void)args;

    time_t boot = tobin_boot_time();
    time_t now = time(NULL);
    double elapsed = difftime(now, boot);
    if (elapsed < 0)
        elapsed = 0;

    long total_seconds = (long)elapsed;
    long days = total_seconds / 86400;
    long hours = (total_seconds % 86400) / 3600;
    long minutes = (total_seconds % 3600) / 60;
    long seconds = total_seconds % 60;

    char bootbuf[64];
    struct tm boot_tm;
    localtime_r(&boot, &boot_tm);
    strftime(bootbuf, sizeof(bootbuf), "%a %b %d %H:%M:%S %Y", &boot_tm);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Up since %s.\r\n"
             "Uptime: %ld day%s, %ld hour%s, %ld minute%s, %ld second%s.\r\n",
             bootbuf,
             days, days == 1 ? "" : "s",
             hours, hours == 1 ? "" : "s",
             minutes, minutes == 1 ? "" : "s",
             seconds, seconds == 1 ? "" : "s");
    descriptor_send(d, msg);
    return true;
}
