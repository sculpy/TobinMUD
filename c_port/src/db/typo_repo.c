/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "typo_repo.h"

#include <stdio.h>

#include "db.h"

/* Records a new player-submitted typo/text report from the in-game "typo"
 * command. */
bool typo_repo_add(const char *submitter, const char *body, int room_vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* %s params are escaped by db_query, so free-form report text is safe. */
    bool ok;
    if (room_vnum > 0)
        ok = db_query(db, "insert into typo (submitter, body, room_vnum) values ('%s', '%s', %i)",
                      submitter ? submitter : "", body ? body : "", room_vnum);
    else
        ok = db_query(db, "insert into typo (submitter, body) values ('%s', '%s')",
                      submitter ? submitter : "", body ? body : "");

    db_close(db);
    return ok;
}

/* Formats the most recent typo reports (newest first, capped at limit)
 * into a single colorized text block for immortals reviewing the queue.
 * Returns false if there are none. */
bool typo_repo_list(char *out, size_t size, int limit) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    size_t n = 0;
    out[0] = '\0';
    bool any = false;

    if (db_query(db, "select id, date(created_at) as day, submitter, body, room_vnum "
                     "from typo order by id desc limit %i", limit)) {
        while (db_fetch_row(db)) {
            /* "#12 [2026-07-05] Testguy (room 1234): 'recieve' should be
             * 'receive'" -- id/date in a dim cyan tag, then the submitter,
             * the room they filed from (if known), and the report text. */
            const char *rv = db_get(db, "room_vnum");
            if (rv && *rv)
                n += (size_t)snprintf(out + n, size > n ? size - n : 0,
                                      "<c>#%s [%s]<z> <w>%s<z> (room %s): %s\r\n",
                                      db_get(db, "id"), db_get(db, "day"),
                                      db_get(db, "submitter"), rv, db_get(db, "body"));
            else
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

/* Permanently removes a typo report by id. */
bool typo_repo_delete(int id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* db_query reports success even when zero rows match, so confirm the
     * report exists first -- that way deltypo can honestly say "no such
     * report". */
    bool found = db_query(db, "select 1 from typo where id=%i", id) && db_fetch_row(db);
    bool ok = found && db_query(db, "delete from typo where id=%i", id);

    db_close(db);
    return ok;
}
