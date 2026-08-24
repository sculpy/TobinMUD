/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "tips_repo.h"

#include <stdio.h>

#include "being.h"
#include "db.h"
#include "descriptor.h"

/* Adds a new newbie tip to the pool, e.g. via an immortal's "addtip"
 * command. */
bool tips_repo_add(const char *added_by, const char *body) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "insert into tip (added_by, body) values ('%s', '%s')",
                       added_by ? added_by : "", body ? body : "");

    db_close(db);
    return ok;
}

/* Picks one random tip's body text -- what tips_pulse_tick() below sends
 * to newbie players. */
bool tips_repo_random(char *out, size_t size) {
    out[0] = '\0';

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "select body from tip order by rand() limit 1") && db_fetch_row(db);
    if (ok)
        snprintf(out, size, "%s", db_get(db, "body"));

    db_close(db);
    return ok;
}

/* Formats every tip (id + body), newest first, into a single colorized
 * text block -- for immortals reviewing/managing the tip pool. Returns
 * false if there are none. */
bool tips_repo_list(char *out, size_t size) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    size_t n = 0;
    out[0] = '\0';
    bool any = false;

    if (db_query(db, "select id, body from tip order by id desc")) {
        while (db_fetch_row(db)) {
            n += (size_t)snprintf(out + n, size > n ? size - n : 0,
                                  "<c>#%s<z> %s\r\n", db_get(db, "id"), db_get(db, "body"));
            any = true;
            if (n >= size)
                break;
        }
    }

    db_close(db);
    return any;
}

/* Permanently removes a tip by id. */
bool tips_repo_delete(int id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = db_query(db, "select 1 from tip where id=%i", id) && db_fetch_row(db);
    bool ok = found && db_query(db, "delete from tip where id=%i", id);

    db_close(db);
    return ok;
}

/* Periodic pulse handler that pushes a random tip to every connected
 * newbie player who hasn't opted out (PLR_NOTIPS). Called on the main
 * pulse loop, not a repo-style CRUD function -- pulse_num is unused but
 * kept to match the standard pulse-handler signature. */
void tips_pulse_tick(long pulse_num) {
    (void)pulse_num;

    char tip[512];
    if (!tips_repo_random(tip, sizeof(tip)))
        return;

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        if (!d->character || !(d->character->pflags & PLR_NEWBIE))
            continue;
        if (d->character->pflags & PLR_NOTIPS)
            continue;
        char msg[560];
        snprintf(msg, sizeof(msg), "\r\n<c>Tip:<z> %s\r\n", tip);
        descriptor_notify(d, msg);
    }
}
