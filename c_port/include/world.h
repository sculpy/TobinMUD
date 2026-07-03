#ifndef TOBIN_WORLD_H
#define TOBIN_WORLD_H

#include "room.h"

/* Minimal in-memory room registry for Phase 1: rooms are loaded from the DB
 * lazily as they're first visited, not boot-loaded in bulk. A full
 * boot-time world load is future work, see STATUS.md. */

/* Takes ownership of `r`. If a room with the same vnum is already
 * registered, the old one is destroyed and replaced. */
void world_register_room(room_t *r);

/* Returns the registered room for `vnum`, or NULL if not yet loaded. */
room_t *world_get_room(int vnum);

#endif
