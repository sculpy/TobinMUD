/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MOB_AI_H
#define TOBIN_MOB_AI_H

#include <stdbool.h>
#include <stddef.h>

struct being;  /* forward decl only -- avoids a being.h<->mob_ai.h include
                  cycle, same idiom used throughout this codebase */
struct room;

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
 *   - Surplus collection (mob_ai.c's mob_try_surplus_collect(), user
 *     2026-07-26): a DIFFERENT mechanic from the above, keyed by name
 *     keyword ("sweeper"/"hauler"/"trash collector") rather than
 *     ACT_SCAVENGER -- picks up ANY loose takeable item (not destroyed)
 *     and periodically teleport-delivers everything it's collected to
 *     the real seeded Surplus donation room (vnum 563), then teleports
 *     back to keep working its usual room.
 *
 * Pulse-driven (main.c), same ~60s cadence as gametime_tick()/
 * zone_process_run(). */
void mob_ai_tick(long pulse_num);

/* Pursuit (Sneezy → Tobin feature audit, "Monster AI & behavior
 * (pursuit)"): called from cmd_flee.c right after a successful flee
 * breaks off combat. If `m` (the mob just left behind) is
 * ACT_AGGRESSIVE, has a flat chance to immediately follow `fled_ch` into
 * `to` and resume the fight -- a single-room, immediate reaction, not a
 * real multi-room hunt (Tobin has no cross-tick hunting-pointer state
 * machine). Returns true iff `m` followed and re-engaged, in which case
 * `to` already reflects the mob's new location and both fighting
 * pointers are set. See mob_ai.c's own doc comment for the full scope-cut
 * rationale. */
bool mob_ai_try_pursue(struct being *m, struct being *fled_ch, struct room *to);

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

/* The original engine's newbie-equipment-giver spec-proc id (spec_mobs.h,
 * SPEC_NEWBIE_EQUIPPER 147) -- already seeded verbatim into mob.spec_proc
 * for the real upstream NPC it was written for (vnum 90, "the Grimhaven
 * social worker", room 570 "The Grimhaven Welfare Department" -- both
 * already present in the seed data, nothing new to add there). Same
 * data-driven lookup-key precedent as SPEC_PROC_LAMPLIGHTER above: no
 * spec-proc EXECUTION from the original C++ engine is ported, just the
 * numeric id, checked against being_t.mob_spec_proc (cached at spawn,
 * mob_repo.h) by cmd_say.c's speech dispatch. User 2026-07-26: "or in
 * room 570 (welfare) they could ask the social worker to receive a new
 * set of newbie gear" -- reissues the speaker's own class's newbie suit
 * (suit.h/suit_repo.h) on request, same suit_grant() the automatic
 * character-creation issue and the `loadsuit` immortal command both
 * already call. */
#define SPEC_PROC_NEWBIE_EQUIPPER 147

/* Greets an arriving PC with a listing of what SPEC_PROC_NEWBIE_EQUIPPER
 * mob(s) in `r` have available for their class, the moment they walk in
 * (user, 2026-08-03: "surplus welfare worker should list whats available
 * for her when walking into the room") -- separate from, and doesn't
 * change, the existing "say gear"/etc request-and-reissue flow
 * (cmd_say.c's try_newbie_equipper()), which stays the only way to
 * actually RECEIVE the gear. Called from cmd_move.c right after a mover's
 * own arrival echo. No-op for anyone who isn't a real connected PC (a mob
 * wandering in, an admin snapshot, ...). */
void mob_ai_greet_newbie_equipper(struct being *arriver, struct room *r);

/* SPEC_BEGGAR (spec_mobs.cc's `beggar`, spec-proc project, SPEC_PROCS.md)
 * -- called from cmd_object.c's `give` right after `m` (the recipient)
 * successfully receives an item or `amount` coins. No-op for any mob
 * without a matching mob_spec_proc, or for a non-mob target (a PC
 * recipient never reaches these). See mob_ai.c for the reaction text. */
void mob_ai_notify_given_item(struct being *m);
void mob_ai_notify_given_coins(struct being *m, int amount);

/* Forces `m` (must be a mob) to attempt its one wander-a-room-over move
 * right now, bypassing the normal per-AI-tick dice roll -- backs
 * egotrip's `wander` subcommand (user, 2026-08-08). All the OTHER
 * legitimate gates (charmed pet, ACT_SENTINEL, fighting/non-standing, no
 * open exit) still apply; a no-op for anything they'd already block. */
void mob_ai_force_wander(struct being *m);

#endif
