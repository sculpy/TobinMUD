/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MOB_AI_H
#define TOBIN_MOB_AI_H

#include <stddef.h>

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

/* Decodes a raw `mob.actions` value into a readable "[ SENTINEL ] [ ... ]"
 * run (user 2026-07-12's `stat` command: "actions should be readable
 * flags, not numbers") -- the ORIGINAL's full ACT_* bit layout (misc/
 * defs.h, bits 0-23), not just the 2 bits (SENTINEL/SCAVENGER, plus
 * AGGRESSIVE) Tobin's own mob_ai_tick() currently acts on above -- a
 * mob's seeded actions value can and does carry bits Tobin doesn't
 * implement behavior for yet, and `stat` should still show them
 * honestly rather than silently dropping them. Same bracket-per-flag
 * convention as room.h's room_flag_names(). */
const char *mob_action_names(int flags, char *buf, size_t size);

/* The original engine's "Lamp-Lighter" spec-proc id (spec/spec_mobs.cc,
 * misc/paths.h's scripted patrol routes) -- seeded verbatim into
 * mob.spec_proc for the two real lamp-lighting NPCs in the import (vnum
 * 99 "a lamp-lighting boy", Grimhaven; vnum 1303 "an eager page",
 * Brightmoon). A data-driven signal, same SPEC_PROC_DOCTOR precedent as
 * shop_repo_is_hospital() -- no spec-proc EXECUTION happens, just a
 * lookup key (cached on being_t.mob_spec_proc at spawn, mob_repo.h).
 *
 * Scoped DOWN from the original: the upstream spec-proc walks a
 * hardcoded, per-zone scripted patrol route between named lampposts
 * (lamp_path_pos[][], misc/paths.h) to reach them; Tobin's version skips
 * the patrol entirely (a mob AI system this small has no path-following
 * primitive to reuse) and just acts on whatever OBJ_CAT_LIGHT object
 * keyworded "lamppost" happens to already be in its OWN current room
 * each AI tick -- lighting it at night, extinguishing it by day, same
 * `gametime_is_daytime()` gate and auto-refuel-to-full the original uses
 * (TLight::lampLightStuff(), obj_light.cc). A lamplighter mob that never
 * wanders into a lamppost's room (most don't share one at boot) simply
 * never does anything -- honest about the scope-down rather than faking
 * patrol coverage. */
#define SPEC_PROC_LAMPLIGHTER 96

#endif
