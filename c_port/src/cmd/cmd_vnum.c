/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "db.h"

/* `vnum <room|obj|mob> <pattern>` -- builder helper (inspired by the
 * original's vnum index lookups): list the vnums and names of rooms,
 * objects, or mobiles whose `name` column contains <pattern> (case-
 * insensitive substring). Room/obj/mob prototypes all live in DB_TOBIN, so
 * this is a direct query (same precedent as cmd_mudstats.c). Immortal
 * builder tool, gated at BUILD_MIN_LEVEL. */

/* Safety cap so a very broad pattern can't try to build an unbounded list;
 * well above any realistic builder search, and the output is paged anyway. */
#define VNUM_LIST_MAX 500
#define VNUM_PAGE_SIZE 20  /* lines per page in the pager (like `news`) */

bool cmd_vnum(descriptor_t *d, const char *args) {
    char cat[16] = "";
    int consumed = 0;
    if (sscanf(args, "%15s %n", cat, &consumed) < 1 || !cat[0]) {
        descriptor_send(d, "Usage: vnum <room|obj|mob> <pattern>\r\n");
        return true;
    }
    const char *pattern = args + consumed;
    if (!pattern[0]) {
        descriptor_send(d, "Usage: vnum <room|obj|mob> <pattern>\r\n");
        return true;
    }

    /* Category (abbreviatable) selects which table to query. The table name
     * is never user input -- only <pattern> is interpolated, and db_query
     * escapes it. Each branch uses a literal format string (no runtime
     * format) so there's nothing for -Wformat to second-guess. */
    size_t clen = strlen(cat);
    const char *label;
    int which; /* 0 room, 1 obj, 2 mob */
    if (strncasecmp(cat, "room", clen) == 0) {
        label = "room"; which = 0;
    } else if (strncasecmp(cat, "object", clen) == 0) {
        label = "object"; which = 1;
    } else if (strncasecmp(cat, "mobile", clen) == 0) {
        label = "mobile"; which = 2;
    } else {
        descriptor_send(d, "Usage: vnum <room|obj|mob> <pattern>\r\n");
        return true;
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        descriptor_send(d, "The database is unavailable.\r\n");
        return true;
    }

    /* Sized to the pager's buffer -- the whole list is built here, then
     * released a page at a time by descriptor_page_start(). */
    char out[16384];
    int n = snprintf(out, sizeof(out),
                     "\r\n<c>-- %s vnums matching \"%s\" --<z>\r\n",
                     label, pattern);
    bool ok = false;
    switch (which) {
        case 0:
            ok = db_query(db, "select vnum, name from room where name like "
                              "'%%%s%%' order by vnum limit %i", pattern, VNUM_LIST_MAX);
            break;
        case 1:
            ok = db_query(db, "select vnum, name from obj where name like "
                              "'%%%s%%' order by vnum limit %i", pattern, VNUM_LIST_MAX);
            break;
        default:
            ok = db_query(db, "select vnum, name from mob where name like "
                              "'%%%s%%' order by vnum limit %i", pattern, VNUM_LIST_MAX);
            break;
    }
    int count = 0;
    bool buffer_full = false;
    if (ok) {
        while (db_fetch_row(db)) {
            /* Leave room for one more max-length line + the truncation note. */
            if ((size_t)n >= sizeof(out) - 200) {
                buffer_full = true;
                break;
            }
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                          "  <c>[<z>%5s<c>]<z> %s\r\n",
                          db_get(db, "vnum"), db_get(db, "name"));
            count++;
        }
    }
    db_close(db);

    if (count == 0)
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  none\r\n");
    else if (count >= VNUM_LIST_MAX || buffer_full)
        n += snprintf(out + n, sizeof(out) - (size_t)n,
                      "  <o>... list truncated -- narrow your pattern.<z>\r\n");
    /* Released a page at a time (the descriptor pager, like `news`). */
    descriptor_page_start(d, out, VNUM_PAGE_SIZE);
    return true;
}
