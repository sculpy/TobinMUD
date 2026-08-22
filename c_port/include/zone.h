/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_ZONE_H
#define TOBIN_ZONE_H

#include "being.h"

/* Zones Part 2 (Session 43): executes the ~36k zone_reset instructions
 * already migrated into the DB (Zones Part 1, Session 38) -- populating
 * rooms with mobs/objects and setting door states, instead of leaving
 * that data inert (which is what a player was seeing: no mobs, no items,
 * despite the reset data existing since Session 38).
 *
 * Deliberately covers only the highest-value opcode subset -- M (load
 * mob), O (load object on the ground, boot-time only), E (equip the last-
 * loaded mob), G (give the last-loaded mob a carried item), P (place an
 * item inside the last-loaded container), D (set a door's open/closed/
 * locked state). Together these cover roughly 84% of all real rows. The
 * rest (Y/X/Z object "sets", A random-room, V/H/F/T/L/K/C/R/I/J) are
 * logged once at boot per zone and skipped -- they depend on subsystems
 * Tobin doesn't have yet (mob AI, object sets, loot tables, traps,
 * grouping/charm/mounts). "Wandering" mobs specifically needs a separate
 * mob-movement/AI system (see TODO.md) -- this only POPULATES rooms, it
 * doesn't move mobs around afterward.
 *
 * A copyover does NOT persist room/mob/object state today (only player
 * connection info survives, see cmd_copyover.c) -- the whole in-memory
 * room graph is wiped by the exec() either way. So from the world's
 * perspective a copyover-resumed process starts exactly like a cold boot:
 * zone_boot_all() runs unconditionally at every process start (main.c),
 * not gated on "was this a copyover." */

/* Runs every enabled zone's FULL reset (every opcode in the table fires,
 * unconditionally -- matching the original's "boot time" mode, including
 * the boot-only 'O' ground loads) once. Called from main.c after the DB
 * connection is confirmed, before the game loop starts. */
void zone_boot_all(void);

/* Pulse-driven (main.c): ages every zone by one tick each time it fires;
 * any zone whose age has reached its `lifespan` (minutes) gets a non-boot
 * reset (tops up mobs/objects up to each command's per-room cap; skips
 * boot-only 'O' loads) and its age resets to 0. */
void zone_process_run(long pulse_num);

/* Immortal admin tool (`zonereset`, cmd_zonereset.c) and test hook: force-
 * runs zone_nr's reset right now, exactly like a periodic tick would
 * (boot-only 'O' ground loads are skipped, matching a live top-up, not a
 * fresh boot). *out_mobs and *out_objs report how many mobs/objects were
 * newly loaded (0/0, harmlessly, for a zone_nr with no reset rows). */
void zone_reset_now(int zone_nr, int *out_mobs, int *out_objs);

/* Zone ownership/identity (Session 43, user: "add identity to zones...
 * builder gets assigned a zone then... a 51-54 wants to edit gets rejected
 * except for those assigned to that zone"). A level 55+ immortal can edit
 * any zone's content unconditionally; a builder (51-54) can only edit a
 * zone they've been assigned to via `zoneassign` (also 55+ only, see
 * cmd_zoneassign.c). A room/mob/obj with NO zone assigned (zone_nr < 0,
 * e.g. an unzoned sandbox room) is unrestricted -- the boundary this
 * enforces is per-ZONE, so content outside any zone has no boundary to
 * violate. Currently wired into `edroom` (cmd_edit.c) -- the only content
 * editor that exists yet; apply the same check when edobject/edmobile/
 * zedit are built (TODO.md). */
bool zone_can_edit(const being_t *ch, int zone_nr);

/* `zonefile create <zone>` (cmd_zonefile.c, user: "zonefile create should
 * create a zone file with the current status of the zone and its
 * contents... you should also be able to delete a line from the zone
 * file, rerun zonefile create and it fills in the blanks of whats loaded
 * into the zone"). Snapshots the zone's CURRENT live world state -- every
 * mob and ground object actually sitting in each room in [bottom, top],
 * plus a mob's equipped/held/carried items and a ground container's
 * contents -- into new zone_reset rows, appended after any existing ones
 * so the next boot/periodic reset recreates it.
 *
 * Idempotent against a partially-edited reset table: a (room, vnum) pair
 * already covered by an EXISTING 'M' or 'O' row is left completely alone
 * (no duplicate row, no touching its children) -- so deleting one row and
 * re-running only adds back reset data for whatever that deletion left
 * unrepresented, matching the user's "fills in the blanks" ask. Matching
 * is per room+vnum, not per live instance, so if a room has 3 live copies
 * of the same mob vnum and one already has a covering row, all 3 are
 * still considered covered (the row's own arg2 cap is left untouched --
 * bumping an existing cap is a separate, unrequested feature).
 *
 * A mob/container's contents are only captured one level deep, and only
 * when reachable via a 'G' (carried) or 'O' (ground) row -- the execution
 * engine's own 'E' (equip) opcode never sets "last object", so a 'P'
 * (place-in-container) can't follow an equipped container; this mirrors a
 * real limitation of zone_execute() (zone.c) itself, not a new one. */
void zone_file_create(int zone_nr, int *out_mobs_added, int *out_objs_added);

#endif
