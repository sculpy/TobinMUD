/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "mob_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

bool mob_proto_load(int vnum, mob_proto_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select name, short_desc, description, level, hpbonus, sex, actions, align "
                      "from mob where vnum=%i",
                 vnum)
        && db_fetch_row(db)) {
        snprintf(out->name, sizeof(out->name), "%s", db_get(db, "name"));
        snprintf(out->short_descr, sizeof(out->short_descr), "%s", db_get(db, "short_desc"));
        snprintf(out->description, sizeof(out->description), "%s", db_get(db, "description"));
        out->level = atoi(db_get(db, "level"));
        out->hpbonus = atof(db_get(db, "hpbonus"));
        out->sex = atoi(db_get(db, "sex"));
        out->actions = atoi(db_get(db, "actions"));
        out->align = atoi(db_get(db, "align"));
        found = true;
    }

    db_close(db);
    return found;
}

int mob_find_vnum_by_name(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int vnum = -1;
    if (db_query(db, "select vnum from mob where name like '%%%s%%' order by vnum limit 1", name)
        && db_fetch_row(db)) {
        vnum = atoi(db_get(db, "vnum"));
    }

    db_close(db);
    return vnum;
}
