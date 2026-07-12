/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "db.h"

/* `stat obj|mob|room <vnum>` (Sneezy port, user 2026-07-12: "add stat
 * command so an immortal of level 55+ can see everything about the mob
 * obj or room with a vnum argument"). Unlike `vnum` (cmd_vnum.c), which
 * searches by name/keyword and shows one summary line per match, `stat`
 * takes an exact vnum and dumps EVERY column of that one row -- a
 * generic "<column>: <value>" listing built from `db_col_count()`/
 * `db_col_name()` (new, db.c) rather than hardcoding each table's ~20-40
 * field names by hand, so it can never silently go stale as columns are
 * added later. A room additionally lists its exits (roomexit table); an
 * object additionally lists any objaffect rows (hitroll/damroll/AC
 * modifiers etc) -- both genuinely part of "everything about it" that a
 * bare column dump of the base row would otherwise miss. */

static void dump_row(char *out, size_t out_sz, size_t *n, db_conn_t *db) {
    unsigned int cols = db_col_count(db);
    for (unsigned int i = 0; i < cols && *n < out_sz; i++) {
        *n += (size_t)snprintf(out + *n, out_sz - *n, "  %-16s %s\r\n",
                               db_col_name(db, i), db_get_idx(db, i));
    }
}

bool cmd_stat(descriptor_t *d, const char *args) {
    char cat[16] = "";
    int vnum = 0;
    if (sscanf(args, "%15s %d", cat, &vnum) != 2) {
        descriptor_send(d, "Usage: stat <obj|mob|room> <vnum>\r\n");
        return true;
    }

    size_t clen = strlen(cat);
    const char *table, *label;
    if (strncasecmp(cat, "object", clen) == 0) {
        table = "obj"; label = "Object";
    } else if (strncasecmp(cat, "mobile", clen) == 0) {
        table = "mob"; label = "Mobile";
    } else if (strncasecmp(cat, "room", clen) == 0) {
        table = "room"; label = "Room";
    } else {
        descriptor_send(d, "Usage: stat <obj|mob|room> <vnum>\r\n");
        return true;
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        descriptor_send(d, "The database is unavailable.\r\n");
        return true;
    }

    char out[8192];
    size_t n = 0;
    if (!db_query(db, "select * from %s where vnum=%i", table, vnum) || !db_fetch_row(db)) {
        n = (size_t)snprintf(out, sizeof(out), "No such %s vnum %d.\r\n", table, vnum);
        descriptor_send(d, out);
        db_close(db);
        return true;
    }

    n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n<c>-- %s %d --<z>\r\n", label, vnum);
    dump_row(out, sizeof(out), &n, db);

    if (strcmp(table, "obj") == 0) {
        if (db_query(db, "select type,mod1,mod2 from objaffect where vnum=%i", vnum)
            && db_has_results(db)) {
            n += (size_t)snprintf(out + n, sizeof(out) - n, "<c>-- Affects --<z>\r\n");
            while (db_fetch_row(db) && n < sizeof(out)) {
                n += (size_t)snprintf(out + n, sizeof(out) - n, "  type=%s mod1=%s mod2=%s\r\n",
                                      db_get(db, "type"), db_get(db, "mod1"), db_get(db, "mod2"));
            }
        }
    } else if (strcmp(table, "room") == 0) {
        if (db_query(db, "select direction,destination,condition_flag,name from roomexit where vnum=%i", vnum)
            && db_has_results(db)) {
            n += (size_t)snprintf(out + n, sizeof(out) - n, "<c>-- Exits --<z>\r\n");
            while (db_fetch_row(db) && n < sizeof(out)) {
                n += (size_t)snprintf(out + n, sizeof(out) - n,
                                      "  dir=%s -> %s  cond=%s  door=%s\r\n",
                                      db_get(db, "direction"), db_get(db, "destination"),
                                      db_get(db, "condition_flag"), db_get(db, "name"));
            }
        }
    }

    db_close(db);
    descriptor_send(d, out);
    return true;
}
