/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "trigger_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

bool trigger_repo_add(const char *created_by, const char *target_type, int target_vnum,
                      const char *trigger_type, const char *match_text, int chance_pct,
                      const char *script) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (match_text && match_text[0])
        ok = db_query(db,
            "insert into `trigger` (created_by, target_type, target_vnum, trigger_type, "
            "match_text, chance_pct, script) values ('%s', '%s', %i, '%s', '%s', %i, '%s')",
            created_by ? created_by : "", target_type, target_vnum, trigger_type,
            match_text, chance_pct, script ? script : "");
    else
        ok = db_query(db,
            "insert into `trigger` (created_by, target_type, target_vnum, trigger_type, "
            "match_text, chance_pct, script) values ('%s', '%s', %i, '%s', NULL, %i, '%s')",
            created_by ? created_by : "", target_type, target_vnum, trigger_type,
            chance_pct, script ? script : "");

    db_close(db);
    return ok;
}

/* Shared row-to-struct mapper for every trigger_repo query that selects the
 * standard trigger column set -- avoids duplicating the same field-by-field
 * copy in trigger_repo_load_for(), trigger_repo_list_for(), and
 * trigger_repo_get() below. */
static int fetch_rows(db_conn_t *db, trigger_t *out, int max) {
    int n = 0;
    while (n < max && db_fetch_row(db)) {
        out[n].id = atol(db_get(db, "id"));
        snprintf(out[n].target_type, sizeof(out[n].target_type), "%s", db_get(db, "target_type"));
        out[n].target_vnum = atoi(db_get(db, "target_vnum"));
        snprintf(out[n].trigger_type, sizeof(out[n].trigger_type), "%s", db_get(db, "trigger_type"));
        snprintf(out[n].match_text, sizeof(out[n].match_text), "%s", db_get(db, "match_text"));
        out[n].chance_pct = atoi(db_get(db, "chance_pct"));
        snprintf(out[n].script, sizeof(out[n].script), "%s", db_get(db, "script"));
        n++;
    }
    return n;
}

int trigger_repo_load_for(const char *target_type, int target_vnum,
                          const char *trigger_type, trigger_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select id, target_type, target_vnum, trigger_type, match_text, "
                     "chance_pct, script from `trigger` where target_type='%s' "
                     "and target_vnum=%i and trigger_type='%s'",
                 target_type, target_vnum, trigger_type))
        n = fetch_rows(db, out, max);

    db_close(db);
    return n;
}

int trigger_repo_list_for(const char *target_type, int target_vnum,
                          trigger_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select id, target_type, target_vnum, trigger_type, match_text, "
                     "chance_pct, script from `trigger` where target_type='%s' "
                     "and target_vnum=%i order by id",
                 target_type, target_vnum))
        n = fetch_rows(db, out, max);

    db_close(db);
    return n;
}

/* Lists the distinct target vnums that have at least one 'random'-type
 * trigger attached, for target_type -- used to know which mobs/rooms/objs
 * need to be considered for the random-trigger pulse without scanning
 * every trigger row each time. */
int trigger_repo_random_vnums(const char *target_type, int *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select distinct target_vnum from `trigger` where target_type='%s' "
                     "and trigger_type='random'",
                 target_type)) {
        while (n < max && db_fetch_row(db))
            out[n++] = atoi(db_get(db, "target_vnum"));
    }

    db_close(db);
    return n;
}

/* Removes a trigger by id. Confirms it exists first so the caller can
 * distinguish "removed" from "no such trigger". */
bool trigger_repo_delete(long id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = db_query(db, "select 1 from `trigger` where id=%i", (int)id) && db_fetch_row(db);
    bool ok = found && db_query(db, "delete from `trigger` where id=%i", (int)id);

    db_close(db);
    return ok;
}

/* Loads a single trigger's full definition by id -- used when a trigger
 * editor needs to fetch one specific trigger it already knows the id of. */
bool trigger_repo_get(long id, trigger_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select id, target_type, target_vnum, trigger_type, match_text, "
                     "chance_pct, script from `trigger` where id=%i", (int)id))
        found = fetch_rows(db, out, 1) == 1;

    db_close(db);
    return found;
}

/* Updates just a trigger's script body, leaving its match/chance/target
 * unchanged. Confirms the trigger exists first. */
bool trigger_repo_update_script(long id, const char *script) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = db_query(db, "select 1 from `trigger` where id=%i", (int)id) && db_fetch_row(db);
    bool ok = found && db_query(db, "update `trigger` set script='%s' where id=%i",
                                script ? script : "", (int)id);

    db_close(db);
    return ok;
}

/* Updates just a trigger's match text (or clears it to NULL if empty),
 * leaving its script/chance/target unchanged. Confirms the trigger exists
 * first. */
bool trigger_repo_update_match(long id, const char *match_text) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = db_query(db, "select 1 from `trigger` where id=%i", (int)id) && db_fetch_row(db);
    bool ok;
    if (match_text && match_text[0])
        ok = found && db_query(db, "update `trigger` set match_text='%s' where id=%i",
                               match_text, (int)id);
    else
        ok = found && db_query(db, "update `trigger` set match_text=NULL where id=%i", (int)id);

    db_close(db);
    return ok;
}

/* Updates just a trigger's fire chance percentage, leaving its script/
 * match/target unchanged. Confirms the trigger exists first. */
bool trigger_repo_update_chance(long id, int chance_pct) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = db_query(db, "select 1 from `trigger` where id=%i", (int)id) && db_fetch_row(db);
    bool ok = found && db_query(db, "update `trigger` set chance_pct=%i where id=%i",
                                chance_pct, (int)id);

    db_close(db);
    return ok;
}

int trigger_repo_delete_for_range(int low, int high) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    db_query(db, "delete from `trigger` where target_vnum between %i and %i", low, high);
    long n = db_row_count(db);

    db_close(db);
    return (int)n;
}
