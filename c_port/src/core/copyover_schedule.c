/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "copyover.h"

#include <stdio.h>

#include "descriptor.h"
#include "log.h"

/* -1 = no copyover pending; otherwise seconds remaining in the countdown.
 * Same pattern as shutdown.c's g_remaining_seconds -- see copyover.h. */
static long g_remaining_seconds = -1;

/* Same curated milestone list as shutdown.c (descending, most granular
 * near zero) -- kept as its own copy rather than a shared array since the
 * two countdowns are independent features that happen to want the same
 * cadence, not a single concept split across two files. */
static const long MILESTONES[] = {3600, 1800, 900, 600, 300, 120, 60, 30, 20, 15, 10, 5, 4, 3, 2, 1};

static bool is_milestone(long s) {
    for (size_t i = 0; i < sizeof(MILESTONES) / sizeof(MILESTONES[0]); i++)
        if (MILESTONES[i] == s)
            return true;
    return false;
}

/* Same rendering as shutdown.c's own format_seconds() -- see that copy's
 * doc comment for why (user, 2026-08-08: player-facing messages only,
 * "not everyone will know what 300 second means"). Kept as its own copy,
 * same reasoning as MILESTONES above. */
static void format_seconds(long s, char *buf, size_t bufsz) {
    if (s < 60) {
        snprintf(buf, bufsz, "%ld second%s", s, s == 1 ? "" : "s");
        return;
    }
    long m = s / 60;
    long r = s % 60;
    if (r == 0)
        snprintf(buf, bufsz, "%ld minute%s", m, m == 1 ? "" : "s");
    else
        snprintf(buf, bufsz, "%ld minute%s %ld second%s", m, m == 1 ? "" : "s", r, r == 1 ? "" : "s");
}

static void broadcast(const char *msg) {
    for (descriptor_t *it = g_descriptors; it; it = it->next)
        descriptor_send(it, msg);
}

void copyover_schedule(int seconds, const char *initiator) {
    if (seconds < 0)
        seconds = 0;
    g_remaining_seconds = seconds;

    char msg[256];
    if (seconds == 0) {
        snprintf(msg, sizeof(msg), "\r\n<c>*** %s is copyover-ing the MUD now. ***<z>\r\n", initiator);
    } else {
        char timebuf[48];
        format_seconds(seconds, timebuf, sizeof(timebuf));
        snprintf(msg, sizeof(msg), "\r\n<c>*** %s has scheduled a copyover in %s. ***<z>\r\n",
                 initiator, timebuf);
    }
    broadcast(msg);
    log_info("Copyover scheduled by %s: %d second(s).", initiator, seconds);

    if (seconds == 0) {
        copyover_execute();
        /* Only reached if the exec itself failed -- copyover_execute()
         * already logged and broadcast why. Clear the pending flag so a
         * later `copyover` can be tried again. */
        g_remaining_seconds = -1;
    }
}

bool copyover_cancel(const char *initiator) {
    if (g_remaining_seconds < 0)
        return false;
    g_remaining_seconds = -1;
    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n<g>*** %s has cancelled the scheduled copyover. ***<z>\r\n", initiator);
    broadcast(msg);
    log_info("Copyover cancelled by %s.", initiator);
    return true;
}

bool copyover_is_pending(void) {
    return g_remaining_seconds >= 0;
}

void copyover_pulse_tick(long pulse_num) {
    (void)pulse_num;
    if (g_remaining_seconds < 0)
        return;

    if (g_remaining_seconds > 0) {
        g_remaining_seconds--;
        if (g_remaining_seconds > 0 && is_milestone(g_remaining_seconds)) {
            char timebuf[48];
            format_seconds(g_remaining_seconds, timebuf, sizeof(timebuf));
            char msg[128];
            snprintf(msg, sizeof(msg), "\r\n<c>*** TobinMUD will copyover in %s. ***<z>\r\n", timebuf);
            broadcast(msg);
        }
    }

    if (g_remaining_seconds == 0) {
        copyover_execute();
        /* Same "only reached on failure" reasoning as copyover_schedule()'s
         * own 0-second branch above. */
        g_remaining_seconds = -1;
    }
}
