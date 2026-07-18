/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
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

/* The lowest unused vnum in [bottom, top] (one query, not a per-vnum
 * exists() loop -- walks the sorted list of vnums already in range and
 * returns the first gap, or the vnum right after the last one used if
 * the range is gapless so far). -1 if the whole range is already full.
 * Used by `dig` (cmd_dig.c) to place a newly created room within the
 * current room's own zone. */
int room_repo_next_free_vnum(int bottom, int top);

/* The `zone` column for room `vnum` -- which zone this room belongs to,
 * for the zone-ownership edit gate (zone.h's zone_can_edit()). -1 if the
 * room doesn't exist or its zone column is NULL (an unzoned room, editable
 * by any builder -- see zone_can_edit()'s doc comment). */
int room_repo_get_zone(int vnum);

/* Upserts the room row: inserts with harmless defaults for the many
 * original columns Tobin doesn't model (coords, flags, river, ...) on
 * first save, updates only name/description/sector after that. Backs the
 * in-game `edit` command (cmd_edit.c) -- edits persist immediately, a
 * deliberate deviation from the original's separate rsave-to-zonefile
 * step, since Tobin's world lives in MariaDB. */
bool room_repo_save(const room_t *r);

/* Upserts one exit (vnum, dir) -> dest, with its door type (doorTypeT) and
 * condition bitmask. The roomexit FK requires `dest` to already exist as a
 * room row. */
bool room_repo_save_exit(int vnum, int dir, int dest, int door_type, int condition);

/* Deletes one exit row. True even if it didn't exist. */
bool room_repo_delete_exit(int vnum, int dir);

#endif
