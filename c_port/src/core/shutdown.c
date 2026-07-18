/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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

static bool is_milestone(long s) {
    for (size_t i = 0; i < sizeof(MILESTONES) / sizeof(MILESTONES[0]); i++)
        if (MILESTONES[i] == s)
            return true;
    return false;
}

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

void shutdown_schedule(int seconds, const char *initiator) {
    if (seconds < 0)
        seconds = 0;
    g_remaining_seconds = seconds;

    char msg[256];
    if (seconds == 0)
        snprintf(msg, sizeof(msg), "\r\n<r>*** %s is shutting down the MUD now. ***<z>\r\n", initiator);
    else
        snprintf(msg, sizeof(msg), "\r\n<r>*** %s has scheduled a shutdown in %d second%s. ***<z>\r\n",
                 initiator, seconds, seconds == 1 ? "" : "s");
    broadcast(msg);
    log_info("Shutdown scheduled by %s: %d second(s).", initiator, seconds);

    if (seconds == 0) {
        shutdown_execute();
        g_remaining_seconds = -1;
    }
}

bool shutdown_cancel(void) {
    if (g_remaining_seconds < 0)
        return false;
    g_remaining_seconds = -1;
    broadcast("\r\n<g>*** The scheduled shutdown has been cancelled. ***<z>\r\n");
    log_info("Pending shutdown cancelled.");
    return true;
}

bool shutdown_is_pending(void) {
    return g_remaining_seconds >= 0;
}

int shutdown_seconds_remaining(void) {
    return (int)g_remaining_seconds;
}

void shutdown_pulse_tick(long pulse_num) {
    (void)pulse_num;
    if (g_remaining_seconds < 0)
        return;

    if (g_remaining_seconds > 0) {
        g_remaining_seconds--;
        if (g_remaining_seconds > 0 && is_milestone(g_remaining_seconds)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "\r\n<r>*** The MUD will shut down in %ld second%s. ***<z>\r\n",
                     g_remaining_seconds, g_remaining_seconds == 1 ? "" : "s");
            broadcast(msg);
        }
    }

    if (g_remaining_seconds == 0) {
        shutdown_execute();
        g_remaining_seconds = -1;
    }
}
