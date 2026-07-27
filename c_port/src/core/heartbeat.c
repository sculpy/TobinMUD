/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "heartbeat.h"

#include <time.h>

#include "descriptor.h"

/* Tracks the last hour-bucket (see below) this fired for, so repeated
 * pulses within the same half-hour window don't re-fire. -1 (never fired)
 * guarantees the very first pulse after boot doesn't skip a beat if it
 * happens to land past a boundary. */
static long g_last_bucket = -1;

/* Runs once every half hour (real wall-clock time) and sends everyone
 * a blank line so a "tick" is visible, without any actual message --
 * see heartbeat.h for the full reasoning. */
void heartbeat_tick(long pulse_num) {
    (void)pulse_num;

    /* Shifting the epoch back 30 minutes before dividing into hour-sized
     * buckets makes the bucket boundary land on the half hour (:30)
     * instead of the top of the hour (:00) -- e.g. at exactly HH:30:00,
     * (now - 1800) is exactly HH:00:00, a clean multiple of 3600.
     * time_t is UTC-based, but the server's own local zone (America/
     * New_York) is a whole-hour UTC offset, so this lines up with real
     * local wall-clock half-hour marks. */
    time_t now = time(NULL);
    long bucket = (long)((now - 1800) / 3600);
    if (bucket == g_last_bucket)
        return;
    g_last_bucket = bucket;

    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        /* Skip anyone mid-editor -- there's no real content here, so
         * holding it for catchup just leaves a blank, pointless entry
         * (user 2026-07-12: caught via catchup showing "-- What you
         * missed --" / "-- end of held messages --" with nothing
         * between them). No content to miss, so nothing to hold. */
        if (!it->character || descriptor_in_editor(it))
            continue;
        /* Empty, not "\r\n" -- the game loop's prompter already opens
         * every fresh prompt with its own "\r\n\r\n" (user 2026-07-12:
         * "remove the \r\n from the end of the prompt"), so sending our
         * own newline here just doubled the blank line. This only needs
         * to mark needs_prompt so the next prompter pass fires. */
        descriptor_send(it, "");
    }
}
