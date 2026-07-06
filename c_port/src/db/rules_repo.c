/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "rules_repo.h"

#include <stdio.h>

#include "db.h"

bool rules_repo_list(char *out, size_t size) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    size_t n = 0;
    out[0] = '\0';
    bool any = false;

    if (db_query(db, "select num, title from rules order by num asc")) {
        while (db_fetch_row(db)) {
            n += (size_t)snprintf(out + n, size > n ? size - n : 0,
                                  "  <c>%s.<z> %s\r\n",
                                  db_get(db, "num"), db_get(db, "title"));
            any = true;
            if (n >= size)
                break;
        }
    }

    db_close(db);
    return any;
}

bool rules_repo_get(int num, char *out, size_t size) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    out[0] = '\0';
    if (db_query(db, "select title, body from rules where num=%i", num)
        && db_fetch_row(db)) {
        /* Header in cyan, body in magenta (dim), single send so the color
         * pair isn't auto-reset early. */
        snprintf(out, size, "\r\n<c>Rule %d: %s<z>\r\n\r\n<m>%s<z>\r\n",
                 num, db_get(db, "title"), db_get(db, "body"));
        found = true;
    }

    db_close(db);
    return found;
}

bool rules_repo_upsert(int num, const char *title, const char *body, const char *who) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* %s params are escaped by db_query. num is the primary key, so this
     * creates the rule or replaces an existing one in place. */
    bool ok = db_query(db,
        "insert into rules (num, title, body, updated_by) values (%i, '%s', '%s', '%s') "
        "on duplicate key update title=values(title), body=values(body), "
        "updated_by=values(updated_by)",
        num, title ? title : "", body ? body : "", who ? who : "");

    db_close(db);
    return ok;
}
