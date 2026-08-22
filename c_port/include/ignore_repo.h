/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_IGNORE_REPO_H
#define TOBIN_IGNORE_REPO_H

#include <stdbool.h>

/* DB access for the `player_ignore` table (db/tobin/tobin_migrations.sql)
 * -- a flat per-character name block list, checked by `tell`/`whisper`
 * (cmd_tell.c/cmd_whisper.c) before delivery. See that migration's own
 * comment for why this is name-keyed and scoped down from Sneezy's
 * broader descriptor/account-level ignoreList. */

#define IGNORE_NAME_LEN 32
#define IGNORE_MAX_PER_PLAYER 30

/* Adds `ignored_name` to `player_id`'s ignore list. Returns false if
 * already at IGNORE_MAX_PER_PLAYER and `ignored_name` isn't already on
 * the list (caller checks via ignore_repo_count() first if it wants a
 * specific "list full" message; this just refuses silently). Idempotent
 * (INSERT ... ON DUPLICATE KEY UPDATE) if already ignored. */
bool ignore_repo_add(long player_id, const char *ignored_name);

/* Removes `ignored_name` from `player_id`'s ignore list. Returns false if
 * it wasn't there. */
bool ignore_repo_remove(long player_id, const char *ignored_name);

/* True iff `player_id` has `name` on their ignore list (case-insensitive,
 * matches how every other name lookup in this codebase already works). */
bool ignore_repo_is_ignored(long player_id, const char *name);

/* How many names are currently on `player_id`'s ignore list. */
int ignore_repo_count(long player_id);

/* Fills `out` (each IGNORE_NAME_LEN bytes) with up to `max` ignored names
 * for `player_id`, alphabetically. Returns the count written. */
int ignore_repo_list(long player_id, char out[][IGNORE_NAME_LEN], int max);

#endif
