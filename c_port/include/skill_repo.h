/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SKILL_REPO_H
#define TOBIN_SKILL_REPO_H

#include <stdbool.h>

/* DB access for the `player_skill` table (db/tobin/tobin_migrations.sql)
 * -- one row per player per skill/spell they've actually attempted, used
 * by skill.c's learn-by-doing proficiency system. See skill.h for the
 * gain formula and the higher-level API game commands actually call. */

typedef struct {
    int pct;             /* 0-100 proficiency */
    long last_gain_at;   /* unix timestamp of the last gain-check, for the anti-grind cooldown */
} skill_proficiency_t;

/* Loads `player_id`'s row for `skill_name`. Returns false (with *out
 * untouched) if no row exists yet -- caller treats that as "never
 * attempted". */
bool skill_repo_get(long player_id, const char *skill_name, skill_proficiency_t *out);

/* Writes (creating or overwriting) `player_id`'s row for `skill_name`. */
bool skill_repo_set(long player_id, const char *skill_name, int pct, long last_gain_at);

#endif
