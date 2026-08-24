/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "db.h"

/* `show <room|obj|mob> <pattern>` (renamed from `vnum`, user 2026-08-22)
 * -- builder helper (inspired by the
 * original's vnum index lookups): list the vnums and names of rooms,
 * objects, or mobiles whose `name` column contains <pattern> (case-
 * insensitive substring). <pattern> may also be a bare vnum ("1017") or a
 * vnum range ("100-200") to browse by vnum directly instead of by name --
 * covers the TODO.md "mlist/olist/rlist" ask without three near-duplicate
 * new commands, since this already does everything else they'd need.
 * Room/obj/mob prototypes all live in DB_TOBIN, so this is a direct query
 * (same precedent as cmd_mudstats.c). Immortal builder tool, gated at
 * BUILD_MIN_LEVEL.
 *
 * Range mode ALSO lists the free/unused vnum gaps within that same range
 * (user 2026-07-26, in the middle of hand-picking vnums for a new
 * feature: "we can reclaim some vnums", then "i need a way to list
 * them") -- a builder reserving a fresh block wants to see what's
 * actually open, not just what's taken. Skipped if the existing-vnum
 * listing itself got truncated (VNUM_LIST_MAX/buffer_full) -- past that
 * point `seen` doesn't cover the whole range, so a reported gap could be
 * wrong. */

/* Safety cap so a very broad pattern can't try to build an unbounded list;
 * well above any realistic builder search, and the output is paged anyway. */
#define VNUM_LIST_MAX 500
#define VNUM_PAGE_SIZE 20  /* lines per page in the pager (like `news`) */
#define VNUM_GAP_MAX 30    /* cap on free-vnum gap segments listed (range mode) */

/* Recognizes a bare vnum ("1017") or a vnum range ("100-200") as an
 * alternative to the usual name/keyword substring search (TODO.md:
 * "mlist/olist/rlist" -- a builder wants to browse a specific vnum or
 * range directly, not just search by name). The WHOLE pattern must parse
 * this way (via %n's consumed-length check) so an ordinary name that
 * happens to start with digits (e.g. "3-piece suit") still falls through
 * to the substring search instead of being misread as a range. */
static bool parse_vnum_range(const char *pattern, int *lo, int *hi) {
    int a, b, n = 0;
    if (sscanf(pattern, "%d-%d%n", &a, &b, &n) == 2 && (size_t)n == strlen(pattern)) {
        *lo = a < b ? a : b;
        *hi = a < b ? b : a;
        return true;
    }
    n = 0;
    if (sscanf(pattern, "%d%n", &a, &n) == 1 && (size_t)n == strlen(pattern)) {
        *lo = *hi = a;
        return true;
    }
    return false;
}

/* `vnum <room|obj|mob> <pattern>` command -- see file-top comment for
 * the full design. Parses the category and pattern, treats the pattern
 * as a name substring unless parse_vnum_range() recognizes it as a bare
 * vnum or range, queries the matching DB_TOBIN table, then (in range
 * mode) also lists the unused vnum gaps within that range. */
bool cmd_show(descriptor_t *d, const char *args) {
    char cat[16] = "";
    int consumed = 0;
    if (sscanf(args, "%15s %n", cat, &consumed) < 1 || !cat[0]) {
        descriptor_send(d, "Usage: show <room|obj|mob> <pattern>\r\n");
        return true;
    }
    const char *pattern = args + consumed;
    if (!pattern[0]) {
        descriptor_send(d, "Usage: show <room|obj|mob> <pattern>\r\n");
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
        descriptor_send(d, "Usage: show <room|obj|mob> <pattern>\r\n");
        return true;
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        descriptor_send(d, "The database is unavailable.\r\n");
        return true;
    }

    int lo, hi;
    bool is_range = parse_vnum_range(pattern, &lo, &hi);

    /* Sized to the pager's buffer -- the whole list is built here, then
     * released a page at a time by descriptor_page_start(). */
    char out[16384];
    int n = is_range
        ? snprintf(out, sizeof(out),
                   "\r\n<c>-- %s vnums %d-%d --<z>\r\n", label, lo, hi)
        : snprintf(out, sizeof(out),
                   "\r\n<c>-- %s vnums matching \"%s\" --<z>\r\n", label, pattern);
    bool ok = false;
    if (is_range) {
        static const char *const TABLE[3] = { "room", "obj", "mob" };
        ok = db_query(db, "select vnum, name from %r where vnum between %i and %i "
                          "order by vnum limit %i", TABLE[which], lo, hi, VNUM_LIST_MAX);
    } else switch (which) {
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
    /* Only tracked in range mode (see the gap-listing block below) -- a
     * name search has no "the whole range" concept to diff against. */
    int seen[VNUM_LIST_MAX];
    if (ok) {
        while (db_fetch_row(db)) {
            /* Leave room for one more max-length line + the truncation note. */
            if ((size_t)n >= sizeof(out) - 200) {
                buffer_full = true;
                break;
            }
            int v = atoi(db_get(db, "vnum"));
            if (is_range && count < VNUM_LIST_MAX)
                seen[count] = v;
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

    /* Free/unused vnums within the same range (user 2026-07-26: "i need a
     * way to list them... to list <room|obj|mob> vnums in a range" --
     * asked for right after "we can reclaim some vnums", i.e. the actual
     * need is finding gaps to reuse, not just seeing what's taken).
     * Skipped if the existing-vnum listing above was itself truncated --
     * `seen` wouldn't cover the whole range, so any gap past that point
     * would be a guess, not a fact. */
    if (is_range && count < VNUM_LIST_MAX && !buffer_full) {
        n += snprintf(out + n, sizeof(out) - (size_t)n,
                      "\r\n<c>-- free %s vnums in %d-%d --<z>\r\n", label, lo, hi);
        int gaps_shown = 0;
        int next_free = lo;
        for (int i = 0; i <= count; i++) {
            int boundary = (i < count) ? seen[i] : hi + 1;
            if (boundary > next_free) {
                if ((size_t)n >= sizeof(out) - 100 || gaps_shown >= VNUM_GAP_MAX) {
                    n += snprintf(out + n, sizeof(out) - (size_t)n,
                                  "  <o>... more free vnums exist -- narrow your range to see them.<z>\r\n");
                    break;
                }
                int gap_hi = boundary - 1;
                if (next_free == gap_hi)
                    n += snprintf(out + n, sizeof(out) - (size_t)n, "  %d\r\n", next_free);
                else
                    n += snprintf(out + n, sizeof(out) - (size_t)n, "  %d-%d\r\n", next_free, gap_hi);
                gaps_shown++;
            }
            next_free = boundary + 1;
        }
        if (gaps_shown == 0)
            n += snprintf(out + n, sizeof(out) - (size_t)n, "  none -- the whole range is taken\r\n");
    }

    /* Released a page at a time (the descriptor pager, like `news`). */
    descriptor_page_start(d, out, VNUM_PAGE_SIZE);
    return true;
}
