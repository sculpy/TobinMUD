/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "social_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

/* Shared row-to-struct mapper for social_repo_load_all() and
 * social_repo_get() -- both select the same social columns and need the
 * same copy-out logic. */
static void row_to_social(db_conn_t *db, social_t *s) {
    snprintf(s->name, sizeof(s->name), "%s", db_get(db, "name"));
    s->hide = atoi(db_get(db, "hide")) != 0;
    s->min_position = atoi(db_get(db, "min_position"));
    snprintf(s->self_no_arg, sizeof(s->self_no_arg), "%s", db_get(db, "self_no_arg"));
    snprintf(s->others_no_arg, sizeof(s->others_no_arg), "%s", db_get(db, "others_no_arg"));
    snprintf(s->self_found, sizeof(s->self_found), "%s", db_get(db, "self_found"));
    snprintf(s->others_found, sizeof(s->others_found), "%s", db_get(db, "others_found"));
    snprintf(s->vict_found, sizeof(s->vict_found), "%s", db_get(db, "vict_found"));
    snprintf(s->not_found, sizeof(s->not_found), "%s", db_get(db, "not_found"));
    snprintf(s->self_auto, sizeof(s->self_auto), "%s", db_get(db, "self_auto"));
    snprintf(s->others_auto, sizeof(s->others_auto), "%s", db_get(db, "others_auto"));
}

/* Loads every social command's definition, name-sorted, up to max entries
 * -- used at startup/reload to build the in-memory social table the
 * command parser dispatches against. */
int social_repo_load_all(social_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select name, hide, min_position, self_no_arg, others_no_arg, "
                     "self_found, others_found, vict_found, not_found, self_auto, "
                     "others_auto from social order by name")) {
        while (n < max && db_fetch_row(db))
            row_to_social(db, &out[n++]);
    }

    db_close(db);
    return n;
}

/* Loads a single social's definition by exact name -- used by socedit to
 * fetch one entry for editing. */
bool social_repo_get(const char *name, social_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select name, hide, min_position, self_no_arg, others_no_arg, "
                     "self_found, others_found, vict_found, not_found, self_auto, "
                     "others_auto from social where name='%s'", name)) {
        if (db_fetch_row(db)) {
            row_to_social(db, out);
            found = true;
        }
    }

    db_close(db);
    return found;
}

/* Creates or updates a social's full definition, keyed by name. */
bool social_repo_save(const social_t *s) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into social (name, hide, min_position, self_no_arg, others_no_arg, "
        "self_found, others_found, vict_found, not_found, self_auto, others_auto) "
        "values ('%s', %i, %i, '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s') "
        "on duplicate key update hide=%i, min_position=%i, self_no_arg='%s', "
        "others_no_arg='%s', self_found='%s', others_found='%s', vict_found='%s', "
        "not_found='%s', self_auto='%s', others_auto='%s'",
        s->name, s->hide ? 1 : 0, s->min_position, s->self_no_arg, s->others_no_arg,
        s->self_found, s->others_found, s->vict_found, s->not_found, s->self_auto,
        s->others_auto,
        s->hide ? 1 : 0, s->min_position, s->self_no_arg, s->others_no_arg,
        s->self_found, s->others_found, s->vict_found, s->not_found, s->self_auto,
        s->others_auto);

    db_close(db);
    return ok;
}

/* Renames a social command without touching its message text. */
bool social_repo_rename(const char *old_name, const char *new_name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update social set name='%s' where name='%s'", new_name, old_name);

    db_close(db);
    return ok;
}

/* Deletes a social command by name. */
bool social_repo_delete(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from social where name='%s'", name);

    db_close(db);
    return ok;
}
