/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "trigger_script.h"

#include <stdio.h>

#include "db.h"

/* Backs the DG Scripts-style `global`/%var% persisted-variable pair
 * (trigger_script.h) -- one flat key/value store, no per-context
 * namespacing (see trigger_script.h's header comment for why). Not a
 * cache: reads/writes are both rare (ambient builder-authored world
 * state, not a hot path), same call trigger_repo_load_for() already made. */

bool trigger_global_get(const char *name, char *out, size_t outsz) {
    out[0] = '\0';
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select var_value from trigger_global_var where var_name='%s'", name) &&
        db_fetch_row(db)) {
        snprintf(out, outsz, "%s", db_get(db, "var_value"));
        found = true;
    }

    db_close(db);
    return found;
}

bool trigger_global_set(const char *name, const char *value) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into trigger_global_var (var_name, var_value) values ('%s', '%s') "
        "on duplicate key update var_value=values(var_value)",
        name, value);

    db_close(db);
    return ok;
}
