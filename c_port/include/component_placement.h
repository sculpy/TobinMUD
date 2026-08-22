/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_COMPONENT_PLACEMENT_H
#define TOBIN_COMPONENT_PLACEMENT_H

/* Periodic spell-component placement -- the "gather your reagents in the
 * wild" half of SneezyMUD's obj_component.cc (the compPlace /
 * component_placement table + its per-pulse placement loop).  Real Sneezy
 * spawns specific reagent objects into specific rooms under specific
 * time-of-day / weather windows so a caster can forage for components in
 * the world instead of only buying or looting them; the objects age out
 * again via the ordinary floor-decay path (obj_decay_tick), and some
 * entries actively despawn on an hour window (Sneezy's CACT_REMOVE).
 *
 * Ported DATA-DRIVEN rather than as Sneezy's ~100 hand-written C table
 * rows: the placement rules live in the `component_placement` DB table
 * (db/tobin/component_placement.sql), read once at boot into a static
 * array here.  This is deliberate -- the world's rooms are slated to be
 * deleted and rebuilt, and a hardcoded room-vnum table would be throwaway
 * the moment that happens; a DB table just needs its rows re-seeded (no
 * recompile), matching the live-editable pattern the rest of the codebase
 * already uses (race_balance, game_config, ...).
 *
 * Trimmed from the original: Sneezy keys some windows off sunrise/sunset/
 * moonrise and a per-room weather region; Tobin has a single world-wide
 * sky state (weather.h) and a plain 0-23 clock (gametime.h) with no moon
 * or season, so windows are plain hour ranges + the world weather mask.
 * The mob-carried variant (place the reagent onto a specific mob standing
 * in the room) and the sound cues are dropped -- ground placement is the
 * whole point of "foraging". */

/* Loads the placement rules from the `component_placement` DB table into
 * the in-memory array.  Call once at boot, after the DB is up.  A missing
 * table or empty result simply disables placement (logged). */
void component_placement_load(void);

/* Pulse callback: walks every enabled rule, and for each whose hour and
 * weather window currently matches, rolls its chance and (place) spawns
 * the reagent onto the floor of a random room in its range that doesn't
 * already hold max_per_room of it, or (remove) destroys matching reagents
 * found on the floor of its rooms.  Register with pulse_register() from
 * main.c.  Same ~60s cadence as gametime/weather. */
void component_placement_tick(long pulse_num);

/* COMP_PLACEMENT_PULSES = 600 (~60s at 100ms/pulse) -- the shared "once a
 * minute" world cadence (gametime, weather, zone aging). */
#define COMP_PLACEMENT_PULSES 600

#endif
