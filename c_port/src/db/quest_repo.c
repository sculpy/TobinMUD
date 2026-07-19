/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "quest_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

int quest_repo_get_stage(long player_id, const char *quest_name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int stage = 0;
    if (db_query(db, "select stage from player_quest where player_id=%i and quest_name='%s'",
                 (int)player_id, quest_name)
        && db_fetch_row(db))
        stage = atoi(db_get(db, "stage"));

    db_close(db);
    return stage;
}

bool quest_repo_set_stage(long player_id, const char *quest_name, int stage) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (stage <= 0) {
        ok = db_query(db, "delete from player_quest where player_id=%i and quest_name='%s'",
                      (int)player_id, quest_name);
    } else {
        ok = db_query(db,
            "insert into player_quest (player_id, quest_name, stage) values (%i, '%s', %i) "
            "on duplicate key update stage=%i",
            (int)player_id, quest_name, stage, stage);
    }

    db_close(db);
    return ok;
}

int quest_repo_list_player(long player_id, quest_entry_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db,
                "select quest_name, stage from player_quest where player_id=%i and stage > 0 "
                "order by quest_name",
                (int)player_id)) {
        while (n < max && db_fetch_row(db)) {
            snprintf(out[n].name, QUEST_NAME_LEN, "%s", db_get(db, "quest_name"));
            out[n].stage = atoi(db_get(db, "stage"));
            n++;
        }
    }

    db_close(db);
    return n;
}

bool quest_repo_def_get(const char *quest_name, int stage, char *buf, size_t bufsz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select description from quest_def where quest_name='%s' and stage=%i",
                 quest_name, stage)
        && db_fetch_row(db)) {
        snprintf(buf, bufsz, "%s", db_get(db, "description"));
        found = true;
    }

    db_close(db);
    return found;
}

bool quest_repo_def_set(const char *quest_name, int stage, const char *description) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into quest_def (quest_name, stage, description) values ('%s', %i, '%s') "
        "on duplicate key update description='%s'",
        quest_name, stage, description, description);

    db_close(db);
    return ok;
}
