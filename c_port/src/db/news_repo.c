/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "news_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

/* Formats the most recent news (or wiznews, when wiz is true) items,
 * newest first, into a single ready-to-send text block, up to limit items.
 * Returns false if there are none. */
bool news_repo_recent(bool wiz, bool archived, char *out, size_t size, int limit) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    size_t n = 0;
    out[0] = '\0';
    bool any = false;

    /* Newest first. No dates rendered -- news items carry no numbers at all
     * (user rule), and the ordering conveys recency. `archived` splits the
     * feed at a three-week (21-day) cutoff: the live feed shows items posted
     * within the window, the archive shows everything older. Four fully
     * literal queries (never a %s table name or interpolated interval) keep
     * the build warning-free, same reason as the original two. */
    bool got;
    if (wiz && archived)
        got = db_query(db, "select author, title, body from wiznews "
                           "where created_at < now() - interval 21 day "
                           "order by created_at desc, id desc limit %i", limit);
    else if (wiz)
        got = db_query(db, "select author, title, body from wiznews "
                           "where created_at >= now() - interval 21 day "
                           "order by created_at desc, id desc limit %i", limit);
    else if (archived)
        got = db_query(db, "select author, title, body from news "
                           "where created_at < now() - interval 21 day "
                           "order by created_at desc, id desc limit %i", limit);
    else
        got = db_query(db, "select author, title, body from news "
                           "where created_at >= now() - interval 21 day "
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

/* Creates a news (or wiznews) item, or edits it in place if title already
 * exists (title is UNIQUE). */
bool news_repo_upsert(bool wiz, const char *author, const char *title, const char *body) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* title is UNIQUE, so re-saving under an existing headline edits that
     * item in place rather than failing -- %s params are escaped by
     * db_query. created_at deliberately isn't touched, so an edit doesn't
     * jump the item back to the top of the newest-first feed. */
    const char *a = author ? author : "", *t = title ? title : "",
               *b = body ? body : "";
    bool ok = wiz
        ? db_query(db, "insert into wiznews (author, title, body) "
                       "values ('%s', '%s', '%s') "
                       "on duplicate key update author=values(author), body=values(body)",
                       a, t, b)
        : db_query(db, "insert into news (author, title, body) "
                       "values ('%s', '%s', '%s') "
                       "on duplicate key update author=values(author), body=values(body)",
                       a, t, b);

    db_close(db);
    return ok;
}

/* Loads a single news (or wiznews) item's body by its exact title. */
bool news_repo_load(bool wiz, const char *title, char *out_body, size_t size) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    out_body[0] = '\0';
    bool found = false;
    bool got = wiz
        ? db_query(db, "select body from wiznews where title='%s'", title ? title : "")
        : db_query(db, "select body from news where title='%s'", title ? title : "");
    if (got && db_fetch_row(db)) {
        const char *body = db_get(db, "body");
        snprintf(out_body, size, "%s", body ? body : "");
        found = true;
    }

    db_close(db);
    return found;
}

/* Removes a news (or wiznews) item by title. Confirms it exists first so
 * the caller can distinguish "removed" from "no such item". */
bool news_repo_delete(bool wiz, const char *title) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    const char *t = title ? title : "";
    bool found = wiz
        ? (db_query(db, "select 1 from wiznews where title='%s'", t) && db_fetch_row(db))
        : (db_query(db, "select 1 from news where title='%s'", t) && db_fetch_row(db));
    bool ok = found && (wiz
        ? db_query(db, "delete from wiznews where title='%s'", t)
        : db_query(db, "delete from news where title='%s'", t));

    db_close(db);
    return ok;
}

/* Highest id currently in the news (or wiznews) table; used to detect
 * whether a player has unread items by comparing against their last-seen
 * id (see player_get_news_last_seen()/player_get_wiznews_last_seen()). */
long news_repo_max_id(bool wiz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    long max_id = 0;
    bool got = wiz
        ? db_query(db, "select max(id) as m from wiznews")
        : db_query(db, "select max(id) as m from news");
    if (got && db_fetch_row(db)) {
        const char *m = db_get(db, "m");
        if (m)
            max_id = atol(m);
    }

    db_close(db);
    return max_id;
}
