/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SOCIAL_REPO_H
#define TOBIN_SOCIAL_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* C replacement for socials (emotes) persistence, backed by the `social`
 * table (db/sneezy/social.sql, generated from the upstream lib/actions
 * file by db/import-socials.py -- see that script for the exact upstream
 * format and the min_position code translation).
 *
 * Unlike room_repo.h, there is no per-verb DB query path: social_try()
 * (socials.c) runs on nearly every unmatched player command -- checked
 * AFTER the whole command table -- so it always reads an in-memory cache
 * built by social_cache_load() (socials.c), never the DB directly. This
 * header is DB access only: load-everything-once (for the cache) and the
 * write path edsocial needs. */

#define SOCIAL_NAME_LEN 32
#define SOCIAL_TEXT_LEN 256 /* generous -- the longest real ported message
                                is well under this */

/* One social's full message set. The 8 text fields mirror the upstream
 * socialMessg exactly (misc/actions.cc) -- see import-socials.py's header
 * comment for what each means and when the last 6 are empty (no targeted
 * form at all). `min_position` is already translated to Tobin's own
 * position_t ordinal (not the upstream file's raw code -- see the
 * importer). Text may contain the upstream $-token grammar ($n/$N/$s/$S/
 * $e/$E/$m/$M), expanded by socials.c's social_expand() at send time, not
 * here. */
typedef struct {
    char name[SOCIAL_NAME_LEN];
    bool hide;
    int min_position;
    char self_no_arg[SOCIAL_TEXT_LEN];
    char others_no_arg[SOCIAL_TEXT_LEN];
    char self_found[SOCIAL_TEXT_LEN];
    char others_found[SOCIAL_TEXT_LEN];
    char vict_found[SOCIAL_TEXT_LEN];
    char not_found[SOCIAL_TEXT_LEN];
    char self_auto[SOCIAL_TEXT_LEN];
    char others_auto[SOCIAL_TEXT_LEN];
} social_t;

/* Fills `out` (up to `max` entries) with every row in `social`, alphabetical
 * by name. Returns the count written. The whole table (~155 rows) always
 * fits comfortably in any reasonable cache size, so this single call is
 * social_cache_load()'s entire DB interaction -- same "array-out,
 * load-everything" convention as room_repo_extra_list(). */
int social_repo_load_all(social_t *out, int max);

/* Loads the exact-name row for `name` (edsocial, to open one entry for
 * editing without needing the whole cache). False if no such row. */
bool social_repo_get(const char *name, social_t *out);

/* Upserts one social (edsocial's Save). */
bool social_repo_save(const social_t *s);

/* Renames a social's verb (its primary key), keeping every other field.
 * False on a genuine SQL error -- including `new_name` colliding with a
 * DIFFERENT already-existing verb (duplicate-key violation) -- but still
 * true (0 rows affected, not an error) if `old_name` didn't exist. */
bool social_repo_rename(const char *old_name, const char *new_name);

/* Deletes one social. True even if it didn't exist. */
bool social_repo_delete(const char *name);

#endif
