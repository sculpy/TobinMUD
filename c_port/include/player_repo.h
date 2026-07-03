#ifndef TOBIN_PLAYER_REPO_H
#define TOBIN_PLAYER_REPO_H

#include <stdbool.h>

#include "being.h"

/* C replacement for player persistence, backed by the `player`,
 * `player_attrs`, and `player_progress` tables (db/sneezy/player.sql,
 * player_attrs.sql, player_progress.sql). An account can own multiple
 * characters (player.account_id); every lookup by name here is scoped to
 * the owning account_id to enforce that. */

#define PLAYER_NAME_LEN 64 /* matches thing_t.name's size, see thing.h */
#define MAX_CHARS_PER_ACCOUNT 10

/* Loads a player by character name, but ONLY if it's owned by account_id.
 * Also loads its attrs (player_attrs). Returns NULL if no such player
 * exists under that account. Caller owns the result (being_destroy). */
being_t *player_load(const char *name, long account_id);

/* Creates a new player row linked to account_id with a default load_room,
 * plus a player_attrs row seeded from *attrs (or ATTR_BASE defaults if
 * attrs is NULL). Returns a freshly-allocated being_t. Fails if the name
 * is already taken (player.name is globally unique) or the account has
 * already reached MAX_CHARS_PER_ACCOUNT. */
being_t *player_create(const char *name, long account_id, const attrs_t *attrs);

/* Deletes a player (and, via ON DELETE CASCADE, its player_attrs row) --
 * but only if it's owned by account_id. Returns false if not found/not
 * owned, or the delete failed. */
bool player_delete(const char *name, long account_id);

/* Lists up to `max` character names owned by account_id into `names`
 * (each PLAYER_NAME_LEN bytes) and each one's level into `levels` (may be
 * NULL if the caller doesn't want them; a player somehow missing its
 * player_progress row reports level 1), sets *count to how many were
 * found. Returns false only on a DB error. */
bool player_list_by_account(long account_id, char names[][PLAYER_NAME_LEN], int levels[], int max, int *count);

/* The room vnum this player should load into (from `player.load_room`).
 * Returns -1 if the player doesn't exist under that account. */
int player_load_room(const char *name, long account_id);

/* Sets `player.load_room` -- backs the immortal `loadroom` command
 * (cmd_loadroom.c). Account-scoped like the other player mutations. */
bool player_set_load_room(const char *name, long account_id, int vnum);

/* Loads persisted attributes for player_id into *out. Returns false (and
 * leaves *out untouched) if no player_attrs row exists. */
bool player_attrs_load(long player_id, attrs_t *out);

/* Upserts (insert-or-replace) the player_attrs row for player_id. */
bool player_attrs_save(long player_id, const attrs_t *attrs);

/* Loads persisted level/experience/hp/max_hp for player_id into *out.
 * Returns false (and leaves *out untouched) if no player_progress row
 * exists -- being_create_pc()'s freshly-seeded defaults stand in that case. */
bool player_progress_load(long player_id, progress_t *out);

/* Upserts (insert-or-replace) the player_progress row for player_id. */
bool player_progress_save(long player_id, const progress_t *progress);

/* Sets the persisted level of any player by exact name -- deliberately NOT
 * scoped to an account, unlike everything else here: this backs the
 * immortal-only `promote` command, which acts across accounts. Returns
 * false if no such player exists (or on DB error). */
bool player_set_level_by_name(const char *name, int level);

#endif
