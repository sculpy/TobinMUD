/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "news_repo.h"

#include <stdio.h>

#include "db.h"

bool news_repo_recent(bool wiz, char *out, size_t size, int limit) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    size_t n = 0;
    out[0] = '\0';
    bool any = false;

    /* Newest first. No dates rendered -- news items carry no numbers at all
     * (user rule), and the ordering conveys recency. Two literal queries
     * (not a %s table name) keep the build warning-free. */
    bool got = wiz
        ? db_query(db, "select author, title, body from wiznews "
                       "order by created_at desc, id desc limit %i", limit)
        : db_query(db, "select author, title, body from news "
                       "order by created_at desc, id desc limit %i", limit);
    if (got) {
        while (db_fetch_row(db)) {
            const char *author = db_get(db, "author");
            const char *title = db_get(db, "title");
            const char *body = db_get(db, "body");
            n += (size_t)snprintf(out + n, size > n ? size - n : 0,
                                  "\r\n<c>%s<z>\r\n%s\r\n", title, body);
            if (author && author[0])
                n += (size_t)snprintf(out + n, size > n ? size - n : 0,
                                      "  -- %s\r\n", author);
            any = true;
            if (n >= size)
                break;
        }
    }

    db_close(db);
    return any;
}

bool news_repo_add(bool wiz, const char *author, const char *title, const char *body) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* title is UNIQUE, so a duplicate headline fails cleanly (reported by
     * the caller). %s params are escaped by db_query. */
    const char *a = author ? author : "", *t = title ? title : "",
               *b = body ? body : "";
    bool ok = wiz
        ? db_query(db, "insert into wiznews (author, title, body) "
                       "values ('%s', '%s', '%s')", a, t, b)
        : db_query(db, "insert into news (author, title, body) "
                       "values ('%s', '%s', '%s')", a, t, b);

    db_close(db);
    return ok;
}
