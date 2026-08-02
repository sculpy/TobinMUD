/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_WORLD_H
#define TOBIN_WORLD_H

#include "being.h"
#include "obj.h"
#include "room.h"

/* Minimal in-memory room registry for Phase 1: rooms are loaded from the DB
 * lazily as they're first visited, not boot-loaded in bulk. A full
 * boot-time world load is future work, see STATUS.md. */

/* Takes ownership of `r`. If a room with the same vnum is already
 * registered, the old one is destroyed and replaced. */
void world_register_room(room_t *r);

/* Returns the registered room for `vnum`, or NULL if not yet loaded. */
room_t *world_get_room(int vnum);

/* Count of rooms currently loaded into memory (`stats` command,
 * TODO.md priority item, user 2026-07-30) -- NOT the total seeded room
 * count (that's a live `select count(*) from room`, see cmd_stats.c),
 * just how many of those Tobin has lazily loaded so far this process
 * (see this file's own top comment on lazy room loading). */
int world_count_loaded_rooms(void);

/* Searches every registered room for a linkdead PC (base.kind == THING_PC,
 * desc == NULL -- link-loss detaches rather than destroys, see
 * descriptor_destroy()) whose player_id matches. Returns NULL if that
 * player isn't sitting linkdead anywhere. Used on reconnect to resume the
 * same live being_t instead of loading a fresh one from the DB. */
being_t *world_find_linkdead_pc(long player_id);

/* Searches every registered room for any PC with a matching player_id,
 * whether linkdead or actively connected (desc != NULL). Returns NULL if
 * no such PC exists. Used at login to prevent duplicate character instances. */
being_t *world_find_active_pc(long player_id);

/* Force-removes every linkdead PC (base.kind == THING_PC, desc == NULL) in
 * every registered room -- `purge linkdead` (cmd_purge.c). Deliberately does
 * NOT save first: a linkdead body's in-memory state is never eagerly
 * persisted (see descriptor_destroy()'s comment on why -- it would risk
 * clobbering a fresher DB-side change), so this just discards it the same
 * way a linkdead body already gets discarded on the owning account's next
 * reconnect (see world_find_linkdead_pc()'s caller in descriptor.c) or a
 * plain process restart -- just triggered on demand instead. Returns how
 * many were removed. */
int world_purge_linkdead(void);

/* Read-only count of linkdead PCs (base.kind == THING_PC, desc == NULL) in
 * every registered room -- same scope as world_purge_linkdead() but never
 * removes anything. Used by `who` to report the split between active links
 * and bodies left behind by a lost connection. */
int world_count_linkdead(void);

/* Auto-purge (TODO.md priority item, user 2026-07-20): force-removes every
 * linkdead PC that has been linkdead at least `max_age_seconds` (compares
 * against `being_t.linkdead_since`, stamped by descriptor_destroy() the
 * moment `desc` was cleared). UNLIKE world_purge_linkdead() above, this
 * DOES save first (player_save()) -- user-directed deviation from that
 * function's own discard-only precedent, matching the real Sneezy's
 * nukeLdead() (misc/periodic.cc), which force-saves before stripping/
 * freeing. Called from a pulse (linkdead_purge_tick(), registered in
 * main.c), not on demand. Returns how many were removed. */
int world_purge_stale_linkdead(int max_age_seconds);

/* Pulse callback (main.c: `pulse_register(600, linkdead_purge_tick)`,
 * ~60s -- plenty granular against a 5-minute threshold) -- calls
 * world_purge_stale_linkdead() with `config_get()->linkdead_purge_seconds`
 * (config.h -- TOBIN_LINKDEAD_PURGE_SECONDS env var, default 300/5min),
 * a flat threshold for everyone the user chose, a deliberate
 * simplification of the original's 15min-mortal/60min-immortal split.
 * Runtime-configurable specifically so a smoke test can run it short
 * instead of waiting out the real 5 minutes. */
void linkdead_purge_tick(long pulse_num);

/* Calls `visit(m)` for every mob (base.kind == THING_MOB) in every
 * registered room -- the iteration primitive mob_ai.c's pulse-driven
 * wander/scavenge logic runs on each tick. Saves each room's mob list
 * position before calling `visit` (in case it moves the mob elsewhere via
 * thing_set_room()), so a wander mid-iteration can't corrupt the walk --
 * though a mob that wanders INTO a room this tick hasn't reached yet will
 * still get a second visit that same tick (a mild, harmless double-tick,
 * not worth guarding against at this scale). */
void world_for_each_mob(void (*visit)(being_t *m));

/* Calls `visit(o)` for every object (base.kind == THING_OBJ) in every
 * registered room -- used by obj.c's pool-decay pulse tick (obj_pool_decay_
 * tick()) to age every ground puddle a little each tick. Same safe-next-
 * pointer iteration as world_for_each_mob(), since decaying a pool to 0 size
 * destroys it mid-walk. */
void world_for_each_obj(void (*visit)(obj_t *o));

/* Calls `visit(r)` for every registered room -- used by trigger.c's
 * random-tick pulse to roll each room's own "random" trigger (ambient
 * room flavor with no mob involved, e.g. a dripping cave). */
void world_for_each_room(void (*visit)(room_t *r));

#endif
