/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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

/* Searches every registered room for a linkdead PC (base.kind == THING_PC,
 * desc == NULL -- link-loss detaches rather than destroys, see
 * descriptor_destroy()) whose player_id matches. Returns NULL if that
 * player isn't sitting linkdead anywhere. Used on reconnect to resume the
 * same live being_t instead of loading a fresh one from the DB. */
being_t *world_find_linkdead_pc(long player_id);

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
