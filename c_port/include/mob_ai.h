/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MOB_AI_H
#define TOBIN_MOB_AI_H

/* Mob AI (Session 43 continued, user: "in pulse, make sure that mob
 * actions click and mobs that can wander will do so, look at mob ai from
 * sneezy" / "i want cleaner mobs to clean up randomly, i believe this is
 * also in mob ai"). Reads `mob.actions` (the original's ACT_* bitmask,
 * misc/defs.h in the bundled sneezymud-master reference tree -- until now
 * loaded-but-unused per mob_repo.h's own comment) and drives two
 * behaviors from it, mirroring the original's misc/mobact.cc at a much
 * simplified scale:
 *
 *   - Wandering: a mob without ACT_SENTINEL (bit 1, "stays put"), not
 *     fighting, and standing, has a small per-tick chance to walk through
 *     a random valid exit (mirrors wanderAround()/mobileWander()).
 *     Simplification vs. the original: no ACT_STAY_ZONE zone-boundary
 *     restriction yet (Tobin has no direct room-to-zone lookup wired up
 *     for this), no terrain/water/flying/riding/secret-door checks (none
 *     of those subsystems exist for mobs yet) -- just "is there an open,
 *     not-closed exit into a room that isn't ROOM_FLAG_NO_MOB".
 *   - Cleaning: a mob with ACT_SCAVENGER (bit 2) has a small per-tick
 *     chance to pick up and destroy one random loose OBJ_CAT_TRASH item
 *     in its room. Scoped down from the original's ACT_SCAVENGER (which
 *     picks up ANY loose object) to just trash specifically, matching the
 *     user's "clean up" framing rather than risking a cleaner mob eating
 *     real loot or a corpse's contents.
 *
 * Pulse-driven (main.c), same ~60s cadence as gametime_tick()/
 * zone_process_run(). */
void mob_ai_tick(long pulse_num);

#endif
