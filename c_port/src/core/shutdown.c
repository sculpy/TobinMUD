/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "shutdown.h"

#include <stdio.h>

#include "being.h"
#include "descriptor.h"
#include "game_loop.h"
#include "log.h"
#include "player_repo.h"

/* -1 = no shutdown pending; otherwise seconds remaining in the countdown. */
static long g_remaining_seconds = -1;

/* Curated broadcast points -- descending, most granular near zero -- so a
 * long countdown doesn't print a line every single second. Whatever
 * `seconds` a countdown is scheduled for is always announced immediately
 * by shutdown_schedule() itself, milestone or not. */
static const long MILESTONES[] = {3600, 1800, 900, 600, 300, 120, 60, 30, 20, 15, 10, 5, 4, 3, 2, 1};

/* True if `s` seconds remaining is one of the curated MILESTONES points
 * that should get its own countdown announcement. */
static bool is_milestone(long s) {
    for (size_t i = 0; i < sizeof(MILESTONES) / sizeof(MILESTONES[0]); i++)
        if (MILESTONES[i] == s)
            return true;
    return false;
}

/* Renders `s` seconds as "N minute(s)", "N second(s)", or "N minute(s) M
 * second(s)" -- player-facing broadcast text only (user, 2026-08-08: "not
 * everyone will know what 300 second means" / "in the messages only, we
 * will know issuing the command" -- log_info() calls elsewhere in this
 * file stay in plain seconds, unaffected). */
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

/* Sends `msg` to every connected descriptor -- the shared helper behind
 * every shutdown-countdown announcement in this file. */
static void broadcast(const char *msg) {
    for (descriptor_t *it = g_descriptors; it; it = it->next)
        descriptor_send(it, msg);
}

/* The actual "kindly" part: save every connected character (mirrors
 * cmd_save.c's player_save()), warn everyone, flush, then hand off to
 * game_loop.c's own clean-exit path (same one SIGINT already takes). */
static void shutdown_execute(void) {
    log_info("Shutdown executing -- saving all connected players.");
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->character)
            player_save(it->character->player_id, it->character);
    }
    broadcast("\r\n<r>*** The MUD is shutting down. Farewell! ***<z>\r\n");
    for (descriptor_t *it = g_descriptors; it; it = it->next)
        descriptor_flush_output(it);
    game_loop_request_shutdown();
}

/* Starts (or immediately executes, if seconds == 0) a shutdown countdown,
 * announcing it to everyone connected and logging who initiated it. Called
 * by the `shutdown` immortal command. */
void shutdown_schedule(int seconds, const char *initiator) {
    if (seconds < 0)
        seconds = 0;
    g_remaining_seconds = seconds;

    char msg[256];
    if (seconds == 0) {
        snprintf(msg, sizeof(msg), "\r\n<r>*** %s is shutting down the MUD now. ***<z>\r\n", initiator);
    } else {
        char timebuf[48];
        format_seconds(seconds, timebuf, sizeof(timebuf));
        snprintf(msg, sizeof(msg), "\r\n<r>*** %s has scheduled a shutdown in %s. ***<z>\r\n",
                 initiator, timebuf);
    }
    broadcast(msg);
    log_info("Shutdown scheduled by %s: %d second(s).", initiator, seconds);

    if (seconds == 0) {
        shutdown_execute();
        g_remaining_seconds = -1;
    }
}

/* Cancels a pending shutdown countdown, if any, and announces the
 * cancellation (with who cancelled it -- user, 2026-08-08: "add a name to
 * the message: Jesus has canceled a shutdown"). Returns false (no-op) if
 * nothing was pending. */
bool shutdown_cancel(const char *initiator) {
    if (g_remaining_seconds < 0)
        return false;
    g_remaining_seconds = -1;
    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n<g>*** %s has cancelled the scheduled shutdown. ***<z>\r\n", initiator);
    broadcast(msg);
    log_info("Shutdown cancelled by %s.", initiator);
    return true;
}

/* True while a shutdown countdown is running. */
bool shutdown_is_pending(void) {
    return g_remaining_seconds >= 0;
}

/* Seconds left in the current countdown, or a negative value if none is
 * pending. */
int shutdown_seconds_remaining(void) {
    return (int)g_remaining_seconds;
}

/* Periodic hook (registered with the pulse scheduler, fires once/second)
 * that counts a pending shutdown down, broadcasting at milestone points and
 * executing the shutdown once it reaches zero. */
void shutdown_pulse_tick(long pulse_num) {
    (void)pulse_num;
    if (g_remaining_seconds < 0)
        return;

    if (g_remaining_seconds > 0) {
        g_remaining_seconds--;
        if (g_remaining_seconds > 0 && is_milestone(g_remaining_seconds)) {
            char timebuf[48];
            format_seconds(g_remaining_seconds, timebuf, sizeof(timebuf));
            char msg[128];
            snprintf(msg, sizeof(msg), "\r\n<r>*** TobinMUD will shut down in %s. ***<z>\r\n", timebuf);
            broadcast(msg);
        }
    }

    if (g_remaining_seconds == 0) {
        shutdown_execute();
        g_remaining_seconds = -1;
    }
}
