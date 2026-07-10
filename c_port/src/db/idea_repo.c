/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "idea_repo.h"

#include <stdio.h>

#include "db.h"

bool idea_repo_add(const char *submitter, const char *body) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* %s params are escaped by db_query, so free-form idea text is safe. */
    bool ok = db_query(db, "insert into idea (submitter, body) values ('%s', '%s')",
                       submitter ? submitter : "", body ? body : "");

    db_close(db);
    return ok;
}

bool idea_repo_list(char *out, size_t size, int limit) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    size_t n = 0;
    out[0] = '\0';
    bool any = false;

    if (db_query(db, "select id, date(created_at) as day, submitter, body "
                     "from idea order by id desc limit %i", limit)) {
        while (db_fetch_row(db)) {
            /* "#12 [2026-07-05] Testguy: a purge command would help" -- id/date
             * in a dim cyan tag, then the submitter and idea text. */
            n += (size_t)snprintf(out + n, size > n ? size - n : 0,
                                  "<c>#%s [%s]<z> <w>%s<z>: %s\r\n",
                                  db_get(db, "id"), db_get(db, "day"),
                                  db_get(db, "submitter"), db_get(db, "body"));
            any = true;
            if (n >= size)
                break;
        }
    }

    db_close(db);
    return any;
}

bool idea_repo_delete(int id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* db_query reports success even when zero rows match, so confirm the
     * idea exists first -- that way delidea can honestly say "no such idea". */
    bool found = db_query(db, "select 1 from idea where id=%i", id) && db_fetch_row(db);
    bool ok = found && db_query(db, "delete from idea where id=%i", id);

    db_close(db);
    return ok;
}
