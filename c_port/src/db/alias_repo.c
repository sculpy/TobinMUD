/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "alias_repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

/* Creates or updates an alias (name -> expansion) for an account within a
 * given tier. Enforces ALIAS_MAX_PER_TIER only when creating a genuinely
 * new alias name; editing an existing one is always allowed even at the
 * cap (see comment below). */
bool alias_repo_set(long account_id, const char *tier, const char *name, const char *expansion) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* The cap only matters for a genuinely NEW name -- overwriting an
     * existing alias never grows the count, so an account already at the
     * cap can still freely edit what it already has. */
    bool exists = db_query(db,
                           "select 1 from account_alias where account_id=%i and tier='%s' and name='%s'",
                           (int)account_id, tier, name)
                  && db_fetch_row(db);
    if (!exists) {
        bool under_cap = db_query(db,
                                  "select count(*) as n from account_alias where account_id=%i and tier='%s'",
                                  (int)account_id, tier)
                          && db_fetch_row(db) && atoi(db_get(db, "n")) < ALIAS_MAX_PER_TIER;
        if (!under_cap) {
            db_close(db);
            return false;
        }
    }

    bool ok = db_query(db,
        "insert into account_alias (account_id, tier, name, expansion) values (%i, '%s', '%s', '%s') "
        "on duplicate key update expansion='%s'",
        (int)account_id, tier, name, expansion, expansion);

    db_close(db);
    return ok;
}

/* Deletes one named alias in a tier. Confirms the row exists first so the
 * caller can distinguish "removed" from "no such alias". */
bool alias_repo_remove(long account_id, const char *tier, const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool existed = db_query(db,
                            "select 1 from account_alias where account_id=%i and tier='%s' and name='%s'",
                            (int)account_id, tier, name)
                   && db_fetch_row(db);

    bool ok = existed && db_query(db,
        "delete from account_alias where account_id=%i and tier='%s' and name='%s'",
        (int)account_id, tier, name);

    db_close(db);
    return ok;
}

/* Looks up a single alias's expansion text by name within a tier. Returns
 * false if no such alias exists. */
bool alias_repo_find(long account_id, const char *tier, const char *name, char *out, size_t outsz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db,
                "select expansion from account_alias where account_id=%i and tier='%s' and name='%s'",
                (int)account_id, tier, name)
        && db_fetch_row(db)) {
        snprintf(out, outsz, "%s", db_get(db, "expansion"));
        found = true;
    }
    db_close(db);
    return found;
}

/* Lists all aliases in a tier, name-sorted, up to max entries -- used by
 * the "alias" command to show a player everything they have defined. */
void alias_repo_list(long account_id, const char *tier, alias_entry_t *out, int max, int *count) {
    *count = 0;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    if (db_query(db,
                "select name, expansion from account_alias where account_id=%i and tier='%s' order by name limit %i",
                (int)account_id, tier, max)) {
        while (*count < max && db_fetch_row(db)) {
            snprintf(out[*count].name, ALIAS_NAME_LEN, "%s", db_get(db, "name"));
            snprintf(out[*count].expansion, ALIAS_EXPANSION_LEN, "%s", db_get(db, "expansion"));
            (*count)++;
        }
    }

    db_close(db);
}
