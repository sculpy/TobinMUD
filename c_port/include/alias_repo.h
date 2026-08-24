/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_ALIAS_REPO_H
#define TOBIN_ALIAS_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* DB access for the `account_alias` table (db/tobin/tobin_migrations.sql)
 * -- player-defined command aliases, stored on the account and scoped by
 * tier (mortal vs immortal, see cmd_alias.c). */

#define ALIAS_NAME_LEN 32
#define ALIAS_EXPANSION_LEN 255
#define ALIAS_MAX_PER_TIER 20

typedef struct {
    char name[ALIAS_NAME_LEN];
    char expansion[ALIAS_EXPANSION_LEN];
} alias_entry_t;

/* Writes (creating or overwriting) `account_id`'s `tier`-scoped alias
 * `name` -> `expansion`. Returns false if the account is already at
 * ALIAS_MAX_PER_TIER aliases for that tier AND `name` isn't an existing
 * one being overwritten (caller checks via alias_repo_count() first). */
bool alias_repo_set(long account_id, const char *tier, const char *name, const char *expansion);

/* Removes `account_id`'s `tier`-scoped alias `name`. Returns false if no
 * such alias existed. */
bool alias_repo_remove(long account_id, const char *tier, const char *name);

/* Looks up `account_id`'s `tier`-scoped alias `name`, copying its
 * expansion into `out` (size `outsz`) on a hit. Returns false (out
 * untouched) if no such alias exists -- the common case for most typed
 * commands, called on every cmd_dispatch(). */
bool alias_repo_find(long account_id, const char *tier, const char *name, char *out, size_t outsz);

/* Fills `out` (capacity `max`) with every `tier`-scoped alias on
 * `account_id`, sorted by name; sets *count to how many were found (0..max,
 * clamped -- ALIAS_MAX_PER_TIER already keeps a real account well under
 * any reasonable `max`). */
void alias_repo_list(long account_id, const char *tier, alias_entry_t *out, int max, int *count);

#endif
