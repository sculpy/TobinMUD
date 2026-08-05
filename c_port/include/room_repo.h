/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_ROOM_REPO_H
#define TOBIN_ROOM_REPO_H

#include <stdbool.h>
#include <stddef.h>

#include "room.h"

/* C replacement for room persistence, backed by the `room` and `roomexit`
 * tables (db/tobin/room.sql, roomexit.sql). */

/* Loads room `vnum` (and its exits) fresh from the DB. Returns a freshly
 * allocated room_t, or NULL if no such room exists. */
room_t *room_repo_load(int vnum);

/* Finds the lowest-vnum room whose name contains `name` (case-sensitive
 * SQL LIKE substring match, same "%%%s%%" shape as obj_repo.c's own
 * obj_find_vnum_by_name()) -- for `ethereal gate` (cmd_cast.c, level 48)
 * teleporting to a named location. Returns -1 if nothing matches. */
int room_repo_find_vnum_by_name(const char *name);

/* True if a room row with this vnum exists. */
bool room_repo_exists(int vnum);

/* Picks one random vnum >= 100 from the whole `room` table, excluding
 * anything flagged DEATH/PRIVATE/HAVE-TO-WALK (ROOM_FLAG_* in room.h) --
 * one DB-side `ORDER BY RAND() LIMIT 1` rather than a client-side retry
 * loop over guessed vnums (real upstream's genericTeleport() does the
 * latter, `misc/magicutils.cc`, since it only has an in-memory room
 * table to pick from; Tobin's rooms live in the DB, so a single query
 * is simpler and can't loop forever). Used by `teleport` (Mage 19,
 * cmd_cast.c). Returns -1 if no eligible room exists at all. */
int room_repo_random_teleport_vnum(void);

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

/* Extra descriptions ("look <keyword>" reveals hidden room detail) --
 * classic Diku-family mechanic, backed by the upstream `roomextra` table
 * (vnum, name/keyword, description), which already carries 8,861 real
 * seeded rows across the live DB but had no Tobin code reading it at all
 * until this. `roomextra.name` is a space-separated keyword LIST, same
 * shape as obj/mob names (e.g. "posters poster sports sport" -- verified
 * against the real seed) -- matches with the same case-insensitive
 * per-word prefix rule already used everywhere else object/mob keywords
 * are matched, not a whole-string match. Writes into `buf` (size
 * `bufsz`); returns false if room `vnum` has no extra description
 * matching `keyword`. */
bool room_repo_extra_desc(int vnum, const char *keyword, char *buf, size_t bufsz);

/* Builder-facing counterpart to room_repo_extra_desc() above (redit's Extra
 * Descriptions submenu, descriptor.c's CONN_REDIT_EXTRA_* states). Unlike
 * the rest of redit, these commit to the DB immediately rather than
 * deferring to the working-copy's (S)ave -- extras aren't modeled in room_t
 * at all (room_repo_extra_desc() already hits the DB fresh on every
 * `look <keyword>`, no in-memory cache to keep in sync), so buffering them
 * in a new parallel in-memory structure would be one more source of truth
 * for no real benefit. Same "commits immediately, no working copy"
 * precedent as the account editor (see edaccount_id's comment,
 * descriptor.h). */

#define ROOM_EXTRA_NAME_LEN 256  /* matches roomextra.name varchar(255) + NUL */
#define ROOM_EXTRA_MAX_LIST 64   /* generous cap for redit's extras list menu */

/* Fills `out` (each ROOM_EXTRA_NAME_LEN bytes) with up to `max` extra-
 * description keyword-list names (the roomextra.name primary-key values)
 * for room `vnum`, alphabetically. Returns the count written. */
int room_repo_extra_list(int vnum, char out[][ROOM_EXTRA_NAME_LEN], int max);

/* Loads the description for room `vnum`'s extra description whose keyword-
 * list is EXACTLY `name` (the roomextra.name primary-key string -- an exact
 * match, not the keyword/prefix search room_repo_extra_desc() does).
 * Returns false if no such row. */
bool room_repo_extra_get(int vnum, const char *name, char *buf, size_t bufsz);

/* Upserts one extra description: creates it (if `name` doesn't already
 * exist for room `vnum`) or updates its description (if it does). */
bool room_repo_extra_save(int vnum, const char *name, const char *description);

/* Renames an extra description's keyword-list (its primary key) from
 * `old_name` to `new_name`, keeping its description untouched. False on a
 * genuine SQL error -- including `new_name` colliding with a DIFFERENT
 * already-existing row for this vnum (duplicate-key violation) -- but
 * still true (0 rows affected, not an error) if `old_name` didn't exist;
 * callers that care can check db_row_count() themselves. */
bool room_repo_extra_rename(int vnum, const char *old_name, const char *new_name);

/* Deletes one extra description. True even if it didn't exist (see
 * room_repo_delete_exit()'s identical precedent above). */
bool room_repo_extra_delete(int vnum, const char *name);

/* Deletes EVERY extra description for room `vnum` -- the original's
 * DeleteExtraDesc() bulk action (misc/create_rooms.cc), a separate menu
 * item from single-item delete above (Sneezy redit items 6 & 10, per
 * TODO.md). True even if there were none. */
bool room_repo_extra_delete_all(int vnum);

/* Deletes every room row (and its roomexit/roomextra rows) with vnum in
 * [low, high] -- `zone reclaim` (cmd_zone.c). Returns the count of `room`
 * rows deleted (0 on DB error or an empty range). DB-only: does not touch
 * anything currently loaded in server memory -- see cmd_zone.c's own
 * doc comment on why that's the caller's job, not this function's. */
int room_repo_delete_range(int low, int high);

#endif
