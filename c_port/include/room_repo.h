#ifndef TOBIN_ROOM_REPO_H
#define TOBIN_ROOM_REPO_H

#include <stdbool.h>

#include "room.h"

/* C replacement for room persistence, backed by the `room` and `roomexit`
 * tables (db/sneezy/room.sql, roomexit.sql). */

/* Loads room `vnum` (and its exits) fresh from the DB. Returns a freshly
 * allocated room_t, or NULL if no such room exists. */
room_t *room_repo_load(int vnum);

/* True if a room row with this vnum exists. */
bool room_repo_exists(int vnum);

/* Upserts the room row: inserts with harmless defaults for the many
 * original columns Tobin doesn't model (coords, flags, river, ...) on
 * first save, updates only name/description/sector after that. Backs the
 * in-game `edit` command (cmd_edit.c) -- edits persist immediately, a
 * deliberate deviation from the original's separate rsave-to-zonefile
 * step, since Tobin's world lives in MariaDB. */
bool room_repo_save(const room_t *r);

/* Upserts one exit (vnum, dir) -> dest. The roomexit FK requires `dest`
 * to already exist as a room row. Door fields are written as
 * none/zero -- Tobin has no doors yet. */
bool room_repo_save_exit(int vnum, int dir, int dest);

/* Deletes one exit row. True even if it didn't exist. */
bool room_repo_delete_exit(int vnum, int dir);

#endif
