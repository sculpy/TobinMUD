/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_FLY_H
#define TOBIN_FLY_H

/* Dragon flight's in-progress task (`travel` command, cmd_travel.c,
 * renamed from `fly` 2026-08-25 -- collided with an existing flight
 * spell/skill; this internal machinery kept its original name) -- see
 * being.h's fly_ticks_left/fly_dest_vnum doc comment for the fields this
 * drives. Same "one-off countdown, no general task engine" shape as
 * planting.h/planting_tick_run() (this file's own doc comments lean on
 * that precedent rather than repeating it). */

struct descriptor;
struct being;

/* Kicks off a flight already fee-checked/charged and destination-
 * validated by cmd_travel.c: arms the countdown, sets the wait-lockout
 * for the whole trip (same convention as spellcast_start()'s
 * being_set_wait() call), and moves `ch` into the first shared sky
 * waypoint immediately with its own departure flavor. `dest_vnum` is the
 * real roost fly_tick_run() lands them in on the final tick. */
void fly_start(struct descriptor *d, struct being *ch, int dest_vnum);

/* Advances every connected character's in-progress flight by one leg
 * (shared sky waypoint -> next waypoint -> ... -> real destination roost
 * on the final tick), aborting nothing mid-flight -- once paid for and
 * airborne over open sky there's nothing to interrupt it with (see
 * cmd_travel.c's doc comment). Pulse-registered in main.c; also forced
 * by `aitick` for deterministic testing. */
void fly_tick_run(long pulse_num);

#endif
