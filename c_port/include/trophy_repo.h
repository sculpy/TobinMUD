/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TROPHY_REPO_H
#define TOBIN_TROPHY_REPO_H

#include <stdbool.h>

/* DB access for the `player_trophy` table (db/tobin/tobin_migrations.sql)
 * -- one row per player per mob vnum they've killed, tracking a decaying
 * "trophy count" that trophy.c's exp-modifier formula reads. Ported from
 * SneezyMUD's TTrophy class (cmd/cmd_trophy.cc) -- see trophy.h for the
 * formula itself and the disclosed scope-downs from the original. */

typedef struct {
    int mob_vnum;
    double count;
} trophy_entry_t;

/* Reads `player_id`'s trophy count for `mob_vnum`. Returns false (with
 * *out_count left at 0.0) if no row exists yet, i.e. never killed. */
bool trophy_repo_get_count(long player_id, int mob_vnum, double *out_count);

/* Adds `delta` (may be negative) to `player_id`'s count for `mob_vnum`,
 * creating the row (starting from 0) if none exists yet. */
bool trophy_repo_add_count(long player_id, int mob_vnum, double delta);

/* Fills `out[0..max)` with every (mob_vnum, count) row `player_id` has,
 * ordered by mob_vnum ascending (same order Sneezy's own `trophy` command
 * read them in, for a zone-grouped listing -- see the `trophy` command's
 * own doc comment for why this port skips the zone grouping). Returns the
 * number of rows written (capped at `max`). */
int trophy_repo_list_for_player(long player_id, trophy_entry_t *out, int max);

/* Decays EVERY player's EVERY trophy count by `amount` in one query (only
 * rows already above `amount` are touched, matching Sneezy's own `where
 * count > dec` floor-at-zero guard) -- trophy_pulse_tick() (trophy.c).
 * Deliberately a single global UPDATE rather than Sneezy's per-online-
 * player query loop (procTrophyDecay walks character_list and issues one
 * UPDATE per connected PC): decaying every row regardless of whether that
 * player is currently online is both simpler and cheaper on this box's
 * memory-constrained MariaDB (see combat.c's own "found live-testing"
 * comment on the same server's OOM history). Returns the number of rows
 * touched, or -1 on DB error. */
int trophy_repo_decay_all(double amount);

#endif
