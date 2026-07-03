#include "help_repo.h"

#include <stdio.h>

#include "db.h"

bool help_topic_load_exact(const char *name, char *body, size_t body_size) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select body from help_topic where name='%s'", name)
        && db_fetch_row(db)) {
        snprintf(body, body_size, "%s", db_get(db, "body"));
        found = true;
    }

    db_close(db);
    return found;
}

bool help_topic_find(const char *name, char *resolved, size_t resolved_size,
                     char *body, size_t body_size) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select name, body from help_topic where name='%s'", name)
        && db_fetch_row(db)) {
        found = true;
    } else {
        /* Prefix fallback -- mysql_real_escape_string doesn't touch '%',
         * so the wildcard survives the %s escaping intact. */
        char pattern[HELP_TOPIC_NAME_LEN + 2];
        snprintf(pattern, sizeof(pattern), "%s%%", name);
        if (db_query(db, "select name, body from help_topic where name like '%s' "
                          "order by name limit 1", pattern)
            && db_fetch_row(db)) {
            found = true;
        }
    }

    if (found) {
        snprintf(resolved, resolved_size, "%s", db_get(db, "name"));
        snprintf(body, body_size, "%s", db_get(db, "body"));
    }

    db_close(db);
    return found;
}

bool help_topic_save(const char *name, const char *body, const char *updated_by) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into help_topic (name, body, updated_by) values ('%s', '%s', '%s') "
        "on duplicate key update body='%s', updated_by='%s'",
        name, body, updated_by, body, updated_by);

    db_close(db);
    return ok;
}
