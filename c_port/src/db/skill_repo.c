/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "skill_repo.h"

#include <stdlib.h>

#include "db.h"

/* Loads a player's proficiency (percent and last-gain timestamp) in a
 * single named skill. Returns false if the player has no row for it (i.e.
 * they've never practiced/used it). */
bool skill_repo_get(long player_id, const char *skill_name, skill_proficiency_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select pct, last_gain_at from player_skill where player_id=%i and skill_name='%s'",
                 (int)player_id, skill_name)
        && db_fetch_row(db)) {
        out->pct = atoi(db_get(db, "pct"));
        out->last_gain_at = atol(db_get(db, "last_gain_at"));
        found = true;
    }
    db_close(db);
    return found;
}

/* Upserts a player's proficiency in a single named skill. */
bool skill_repo_set(long player_id, const char *skill_name, int pct, long last_gain_at) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into player_skill (player_id, skill_name, pct, last_gain_at) "
        "values (%i, '%s', %i, %i) "
        "on duplicate key update pct=%i, last_gain_at=%i",
        (int)player_id, skill_name, pct, (int)last_gain_at,
        pct, (int)last_gain_at);

    db_close(db);
    return ok;
}
