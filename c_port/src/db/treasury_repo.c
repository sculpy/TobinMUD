/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "treasury_repo.h"

#include <stdlib.h>

#include "db.h"

/* Returns the world treasury's current gold balance (a single singleton
 * row, id=1). */
int treasury_repo_get_gold(void) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int gold = 0;
    if (db_query(db, "select gold from world_treasury where id=1") && db_fetch_row(db))
        gold = atoi(db_get(db, "gold"));

    db_close(db);
    return gold;
}

/* Adjusts the world treasury's gold balance by delta (positive or
 * negative), done as an atomic gold=gold+delta rather than read-then-write. */
bool treasury_repo_add_gold(int delta) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update world_treasury set gold=gold+%i where id=1", delta);

    db_close(db);
    return ok;
}
