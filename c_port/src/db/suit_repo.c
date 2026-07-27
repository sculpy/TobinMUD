/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "suit_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

int suit_repo_find_by_name(const char *name, int *out_class, char *out_name, int out_name_sz) {
    if (out_class)
        *out_class = -1;
    if (!name || !*name)
        return -1;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int found = -1;
    if (db_query(db, "select id, class, name from suit where name like '%%%s%%' order by id limit 1", name)
        && db_fetch_row(db)) {
        found = atoi(db_get(db, "id"));
        const char *c = db_get(db, "class");
        if (out_class && *c)
            *out_class = atoi(c);
        if (out_name)
            snprintf(out_name, (size_t)out_name_sz, "%s", db_get(db, "name"));
    }

    db_close(db);
    return found;
}

int suit_repo_find_for_class(int player_class) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int found = -1;
    if (db_query(db, "select id from suit where class=%i limit 1", player_class)
        && db_fetch_row(db))
        found = atoi(db_get(db, "id"));

    db_close(db);
    return found;
}

int suit_repo_load_items(int suit_id, int *vnums, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select obj_vnum from suit_item where suit_id=%i", suit_id)) {
        while (n < max && db_fetch_row(db))
            vnums[n++] = atoi(db_get(db, "obj_vnum"));
    }

    db_close(db);
    return n;
}
