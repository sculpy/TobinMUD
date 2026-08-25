/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "fly.h"

#include <stdio.h>
#include <stdlib.h>

#include "being.h"
#include "cmd.h"
#include "descriptor.h"
#include "log.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* Shared "in the sky" waypoints every `travel` route passes through, in
 * order -- flavor only, no route-specific waypoints (user asked for "a
 * series of rooms", not a full second world per route). Not reachable by
 * ordinary movement (no walk-in exits seeded anywhere); only fly_start()/
 * fly_tick_run() ever put a character in one. Same zone-63 vnum block as
 * the roosts (dragon_ride.sql and friends), immediately following
 * 7900/7901 -- 7911/7912 (the 2026-08-25 follow-up's two extra legs)
 * come after the destination-roost block (7907-7910) to avoid a clash.
 *
 * fly_tick_run() is pulse_register()'d on a fixed cadence keyed off the
 * ABSOLUTE pulse clock (pulse_num % FLY_TICK_PULSES == 0), not an offset
 * from when a given flight started -- so the first leg after
 * `travel <dest>` fires anywhere from just after takeoff up to a full
 * FLY_TICK_PULSES later, and every leg after that is exactly
 * FLY_TICK_PULSES apart. With FLY_LEG_COUNT legs that puts landing
 * (offset + (FLY_LEG_COUNT-1)*tick) in a real-time window of
 * [(FLY_LEG_COUNT-1)*tick, FLY_LEG_COUNT*tick) -- the user's requested
 * 10-15s range. Went through two revisions to land there: a 4-leg/3s
 * design gave [9s,12s) (could undershoot 10s on a lucky alignment); a
 * follow-up widened it to 5 legs/3s for [12s,15s); the 2026-08-25
 * "extend by two more legs" follow-up then meant 7 legs/3s would have
 * given [18s,21s), well past 15s -- so the per-tick interval was pulled
 * down to ~2s alongside the extra legs, landing 7 legs/2s squarely at
 * [12s,14s). */
#define FLY_LEG_COUNT 7
#define FLY_TICK_PULSES 20 /* ~2s -- see the doc comment above for why this isn't planting_tick_run()'s 30/~3s */
static const int FLY_WAYPOINTS[FLY_LEG_COUNT] = {7902, 7903, 7904, 7906, 7911, 7912, 7905};

/* One line of in-flight flavor, shown to the flying player only -- no
 * other player is ever really in these rooms at the same moment to see a
 * room-echo, so unlike cmd_travel.c's roost departure/arrival there's no
 * room-echo half here. Picked at random each leg so a flight doesn't
 * repeat the same line every time (spellcast.c's gesture-line pool is
 * the same spirit). */
static const char *const FLY_INFLIGHT_LINES[] = {
    "The dragon's wings beat in a slow, steady rhythm as the wind roars past you.\r\n",
    "Far below, the land unrolls like a map, rivers and roads picked out in miniature.\r\n",
    "A bank of cloud swallows you whole for a moment, cold and grey and silent.\r\n",
    "The dragon tilts on an air current, and for a heartbeat you're weightless.\r\n",
    "Sunlight glints off the dragon's scales as it beats higher, chasing the wind.\r\n",
    "You grip tight as the dragon rides a thermal, climbing without a single wingbeat.\r\n",
    "The dragon lets out a low, rumbling call, answered faintly by the wind.\r\n",
    "Your ears pop as the dragon shifts height, chasing a smoother current of air.\r\n",
};
#define FLY_INFLIGHT_LINE_COUNT (int)(sizeof(FLY_INFLIGHT_LINES) / sizeof(FLY_INFLIGHT_LINES[0]))

static room_t *get_or_load_room(int vnum) {
    room_t *r = world_get_room(vnum);
    if (!r) {
        r = room_repo_load(vnum);
        if (r)
            world_register_room(r);
    }
    return r;
}

void fly_start(descriptor_t *d, being_t *ch, int dest_vnum) {
    room_t *wp0 = get_or_load_room(FLY_WAYPOINTS[0]);
    if (!wp0) {
        /* Waypoint rooms are seeded fixtures (dragon_ride.sql) -- this
         * only fires if the DB is missing rows the schema guarantees.
         * Fail safe rather than strand the character mid-flight-setup. */
        log_error("fly_start: waypoint room %d failed to load", FLY_WAYPOINTS[0]);
        descriptor_send(d, "A dragon-keeper tells you, \"Something's wrong with the sky today -- try again later.\"\r\n");
        return;
    }

    ch->fly_ticks_left = FLY_LEG_COUNT;
    ch->fly_dest_vnum = dest_vnum;

    /* Locks out every other command for the whole trip, same convention
     * spellcast_start() uses (being_set_wait()'s own doc comment) --
     * a no-op for an immortal. */
    being_set_wait(ch, FLY_LEG_COUNT * FLY_TICK_PULSES);

    thing_set_room(&ch->base, wp0);
    descriptor_send(d, "The wind roars in your ears as the dragon climbs above the clouds.\r\n");
}

void fly_tick_run(long pulse_num) {
    (void)pulse_num;

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *ch = d->character;
        if (!ch || ch->fly_ticks_left <= 0)
            continue;

        ch->fly_ticks_left--;
        int left = ch->fly_ticks_left;

        if (left > 0) {
            int wp_index = FLY_LEG_COUNT - left;
            room_t *wp = get_or_load_room(FLY_WAYPOINTS[wp_index]);
            if (wp)
                thing_set_room(&ch->base, wp);
            descriptor_send(d, FLY_INFLIGHT_LINES[rand() % FLY_INFLIGHT_LINE_COUNT]);
            continue;
        }

        /* Final leg: land in the real destination roost. */
        room_t *dest = get_or_load_room(ch->fly_dest_vnum);
        int dest_vnum = ch->fly_dest_vnum;
        ch->fly_dest_vnum = 0;
        if (!dest) {
            log_error("fly_tick_run: destination room %d failed to load", dest_vnum);
            descriptor_send(d, "A dragon-keeper tells you, \"That roost seems to have vanished -- try again later.\"\r\n");
            continue;
        }

        thing_set_room(&ch->base, dest);

        /* fly_start()'s wait-lockout was set for the whole trip up front
         * (FLY_LEG_COUNT * FLY_TICK_PULSES), but each leg's real firing
         * time floats on the absolute pulse clock (pulse_register()'s
         * modulus trigger, not an offset from fly_start()) -- the FIRST
         * leg can fire anywhere from 1 pulse to a full FLY_TICK_PULSES
         * after takeoff, so landing can arrive up to that same slack
         * before the fixed wait actually expires. Clearing it here
         * (rather than just waiting it out) means a character who has
         * physically landed is free to act immediately -- and, just as
         * important, unblocks the cmd_dispatch("look") call right below,
         * which would otherwise be swallowed by the still-ticking wait
         * gate (cmd_table.c) and print "You are still recovering!"
         * instead of the room they just arrived in. */
        being_set_wait(ch, 0);

        char cap[128], msg[192];
        being_display_name_cap(ch, cap, sizeof(cap));
        snprintf(msg, sizeof(msg), "%s swoops in on a dragon and dismounts.\r\n", cap);
        descriptor_room_echo(dest, ch, msg);

        descriptor_send(d, "...until the dragon banks and lands, talons gripping the roost.\r\n");
        cmd_dispatch(d, "look");

        game_log(LOG_GAME, "%s flew by dragon into room %d.", ch->base.name, dest->vnum);
    }
}
