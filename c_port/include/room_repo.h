#ifndef TOBIN_ROOM_REPO_H
#define TOBIN_ROOM_REPO_H

#include "room.h"

/* C replacement for room persistence, backed by the `room` and `roomexit`
 * tables (db/sneezy/room.sql, roomexit.sql). */

/* Loads room `vnum` (and its exits) fresh from the DB. Returns a freshly
 * allocated room_t, or NULL if no such room exists. */
room_t *room_repo_load(int vnum);

#endif
