/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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
        if (!it->character)
            continue;
        descriptor_notify(it, "\r\n");
    }
}
