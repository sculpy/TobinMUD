/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "trophy_repo.h"

#include <stdlib.h>

#include "db.h"

bool trophy_repo_get_count(long player_id, int mob_vnum, double *out_count) {
    *out_count = 0.0;
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select count from player_trophy where player_id=%i and mob_vnum=%i",
                 (int)player_id, mob_vnum)
        && db_fetch_row(db)) {
        *out_count = atof(db_get(db, "count"));
        found = true;
    }
    db_close(db);
    return found;
}

bool trophy_repo_add_count(long player_id, int mob_vnum, double delta) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into player_trophy (player_id, mob_vnum, count) values (%i, %i, %f) "
        "on duplicate key update count=count+%f",
        (int)player_id, mob_vnum, delta, delta);

    db_close(db);
    return ok;
}

int trophy_repo_list_for_player(long player_id, trophy_entry_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select mob_vnum, count from player_trophy where player_id=%i order by mob_vnum",
                 (int)player_id)) {
        while (n < max && db_fetch_row(db)) {
            out[n].mob_vnum = atoi(db_get(db, "mob_vnum"));
            out[n].count = atof(db_get(db, "count"));
            n++;
        }
    }
    db_close(db);
    return n;
}

int trophy_repo_decay_all(double amount) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    bool ok = db_query(db, "update player_trophy set count=count-%f where count > %f", amount, amount);
    int affected = ok ? (int)db_row_count(db) : -1;
    db_close(db);
    return affected;
}
