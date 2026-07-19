/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "ignore_repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

bool ignore_repo_add(long player_id, const char *ignored_name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool exists = db_query(db,
                           "select 1 from player_ignore where player_id=%i and ignored_name='%s'",
                           (int)player_id, ignored_name)
                  && db_fetch_row(db);
    if (!exists) {
        bool under_cap = db_query(db,
                                  "select count(*) as n from player_ignore where player_id=%i",
                                  (int)player_id)
                          && db_fetch_row(db) && atoi(db_get(db, "n")) < IGNORE_MAX_PER_PLAYER;
        if (!under_cap) {
            db_close(db);
            return false;
        }
    }

    bool ok = db_query(db,
        "insert into player_ignore (player_id, ignored_name) values (%i, '%s') "
        "on duplicate key update ignored_name=ignored_name",
        (int)player_id, ignored_name);

    db_close(db);
    return ok;
}

bool ignore_repo_remove(long player_id, const char *ignored_name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool existed = db_query(db,
                            "select 1 from player_ignore where player_id=%i and ignored_name='%s'",
                            (int)player_id, ignored_name)
                   && db_fetch_row(db);

    bool ok = existed && db_query(db,
        "delete from player_ignore where player_id=%i and ignored_name='%s'",
        (int)player_id, ignored_name);

    db_close(db);
    return ok;
}

bool ignore_repo_is_ignored(long player_id, const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ignored = db_query(db,
        "select 1 from player_ignore where player_id=%i and ignored_name='%s'",
        (int)player_id, name)
        && db_fetch_row(db);

    db_close(db);
    return ignored;
}

int ignore_repo_count(long player_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select count(*) as n from player_ignore where player_id=%i", (int)player_id)
        && db_fetch_row(db))
        n = atoi(db_get(db, "n"));

    db_close(db);
    return n;
}

int ignore_repo_list(long player_id, char out[][IGNORE_NAME_LEN], int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db,
                "select ignored_name from player_ignore where player_id=%i order by ignored_name",
                (int)player_id)) {
        while (n < max && db_fetch_row(db))
            snprintf(out[n++], IGNORE_NAME_LEN, "%s", db_get(db, "ignored_name"));
    }

    db_close(db);
    return n;
}
