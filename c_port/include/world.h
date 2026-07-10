/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_WORLD_H
#define TOBIN_WORLD_H

#include "being.h"
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

#endif
