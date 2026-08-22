/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "affect_repo.h"

#include <stdlib.h>

#include "db.h"

/* Loads a player's saved active affects (spell effects with rounds
 * remaining) into the fixed-size affects array, in whatever order the DB
 * returns them. Leaves unfilled slots untouched -- callers should start
 * from a zeroed/AFFECT_NONE array. */
void affect_repo_load_all(long player_id, active_affect_t affects[MAX_ACTIVE_AFFECTS]) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    int slot = 0;
    if (db_query(db, "select affect_type, rounds_left, modifier "
                      "from player_active_affect where player_id=%i",
                 (int)player_id)) {
        while (slot < MAX_ACTIVE_AFFECTS && db_fetch_row(db)) {
            affects[slot].type = (affect_type_t)atoi(db_get(db, "affect_type"));
            affects[slot].rounds_left = atoi(db_get(db, "rounds_left"));
            affects[slot].modifier = atoi(db_get(db, "modifier"));
            slot++;
        }
    }

    db_close(db);
}

/* Replaces a player's saved active affects wholesale: deletes the old rows
 * and inserts the current in-memory set, all inside one transaction so a
 * mid-save failure can't leave the player with a half-written affect list.
 * AFFECT_NONE slots and charm/polymorph affects (which belong to a
 * temporary MOB body, not the player) are skipped. */
bool affect_repo_save_all(long player_id, const active_affect_t affects[MAX_ACTIVE_AFFECTS]) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    if (!db_begin(db)) {
        db_close(db);
        return false;
    }

    if (!db_query(db, "delete from player_active_affect where player_id=%i", (int)player_id)) {
        db_rollback(db);
        db_close(db);
        return false;
    }

    bool ok = true;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS && ok; i++) {
        affect_type_t type = affects[i].type;
        if (type == AFFECT_NONE)
            continue;
        /* AFFECT_CHARMED/AFFECT_POLYMORPH always live on a temporary/
         * summoned MOB body, never a real player's own being_t -- see
         * affect_repo.h's doc comment. Skipped defensively rather than
         * assumed impossible. */
        if (type == AFFECT_CHARMED || type == AFFECT_POLYMORPH)
            continue;
        ok = db_query(db, "insert into player_active_affect (player_id, affect_type, rounds_left, modifier) "
                          "values (%i, %i, %i, %i)",
                      (int)player_id, (int)type, affects[i].rounds_left, affects[i].modifier);
    }

    if (!ok) {
        db_rollback(db);
        db_close(db);
        return false;
    }

    ok = db_commit(db);
    db_close(db);
    return ok;
}
