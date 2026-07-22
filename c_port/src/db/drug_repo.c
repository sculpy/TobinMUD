/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "drug_repo.h"

#include <stdlib.h>

#include "db.h"

void drug_repo_load_all(long player_id, drug_state_t states[DRUG_COUNT]) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    if (db_query(db, "select drug_type, first_use, last_use, total_consumed "
                      "from player_drug where player_id=%i",
                 (int)player_id)) {
        while (db_fetch_row(db)) {
            int type = atoi(db_get(db, "drug_type"));
            if (type < 0 || type >= DRUG_COUNT)
                continue;
            states[type].first_use = atol(db_get(db, "first_use"));
            states[type].last_use = atol(db_get(db, "last_use"));
            states[type].total_consumed = atol(db_get(db, "total_consumed"));
        }
    }

    db_close(db);
}

bool drug_repo_save(long player_id, drug_type_t type, const drug_state_t *st) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into player_drug (player_id, drug_type, first_use, last_use, total_consumed) "
        "values (%i, %i, %i, %i, %i) "
        "on duplicate key update first_use=%i, last_use=%i, total_consumed=%i",
        (int)player_id, (int)type, (int)st->first_use, (int)st->last_use, (int)st->total_consumed,
        (int)st->first_use, (int)st->last_use, (int)st->total_consumed);

    db_close(db);
    return ok;
}
