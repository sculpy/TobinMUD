/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "db.h"
#include "room.h"
#include "thing.h"
#include "world.h"

/* `scan [direction|name]` -- port of the original's doScan() (misc/range.cc):
 * peer several rooms deep down each exit and report the players/mobs visible
 * out there, tagged with a distance word and the direction. `scan <dir>`
 * scans just that direction; `scan <name>` filters to beings whose name
 * matches. A closed or secret door blocks the line of sight down that exit.
 *
 * Deliberately simplified vs. the original: no movement-point cost and no
 * blindness gate (Tobin has neither system yet); the multi-room ray-cast,
 * distance words, direction/name arguments, and door hindrance are ported. */

/* Was 6 -- user 2026-07-19: "scan works too well, can we cut the range
 * down to 4 or 5 rooms distance?" Split the difference at 5. */
#define SCAN_MAX_RANGE 5

/* Distance description indexed by rooms away (1..SCAN_MAX_RANGE); [0] unused.
 * Trimmed from the original's longer rng_desc[] table. */
static const char *const SCAN_DIST[] = {
    "right here",
    "nearby",
    "a short way off",
    "not too far off",
    "a good way off",
    "far off",
};

/* Prefix-match a direction word (same convention as cmd_open's parse_dir). */
static int scan_parse_dir(const char *tok) {
    size_t len = strlen(tok);
    if (len == 0)
        return -1;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    return -1;
}

/* short_descr is stored lowercase-first by convention ("a city watchman");
 * capitalize when it starts a whole message. Copies into `buf`. Skips any
 * leading inline color tag first (e.g. "<o>a dirty refuse hauler<1>", real
 * seeded content, mob vnum 33271) -- same duplicated-helper bug as
 * cmd_look.c's cap_first(), fixed there Session 43 continued but missed
 * here since this is a separate copy, not a shared function. Found while
 * working nearby on the get/drop logging feature. */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

/* Case-insensitive "does haystack contain needle" (strcasestr is GNU-only). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0)
        return true;
    for (; *haystack; haystack++)
        if (strncasecmp(haystack, needle, nl) == 0)
            return true;
    return false;
}

/* Destination vnum of room `vnum`'s exit in `dir` (or -1), with *cond set to
 * the exit's condition bitmask. Prefers the in-memory (active) room; falls
 * back to a roomexit query so the ray-cast can pass THROUGH rooms no player
 * has loaded (occupants, though, only exist in active rooms). */
static int scan_exit(db_conn_t *db, int vnum, int dir, int *cond) {
    *cond = 0;
    room_t *r = world_get_room(vnum);
    if (r) {
        *cond = r->exit_cond[dir];
        return r->exits[dir];
    }
    int dest = -1;
    if (db && db_query(db, "select destination, condition_flag from roomexit "
                           "where vnum=%i and direction=%i", vnum, dir)
        && db_fetch_row(db)) {
        dest = atoi(db_get(db, "destination"));
        *cond = atoi(db_get(db, "condition_flag"));
    }
    return dest;
}

/* `scan [direction|name]` command -- see file-top comment for the full
 * port rationale. Parses the optional argument as either a direction
 * (scan just that way) or a name filter, then ray-casts outward via
 * scan_exit() up to SCAN_MAX_RANGE rooms per exit, listing every visible
 * (non-linkdead) PC/mob it finds with a distance word from SCAN_DIST[]. */
bool cmd_scan(descriptor_t *d, const char *args) {
    being_t *me = d->character;
    if (!me || !me->base.roomp) {
        descriptor_send(d, "You are nowhere to scan from.\r\n");
        return true;
    }

    char arg1[32] = "";
    sscanf(args, "%31s", arg1);
    int only_dir = -1;
    const char *grep = NULL;
    if (arg1[0] && strcasecmp(arg1, "all") != 0) {
        only_dir = scan_parse_dir(arg1);
        if (only_dir < 0)
            grep = arg1; /* not a direction -> a name filter */
    }

    /* The room sees you looking (matches the original's TO_ROOM act). */
    char rmsg[96];
    snprintf(rmsg, sizeof(rmsg), "%s scans the surrounding area.\r\n", me->base.name);
    descriptor_room_echo(me->base.roomp, me, rmsg);

    char out[4096];
    int n;
    if (grep)
        n = snprintf(out, sizeof(out),
                     "\r\n<c>You scan the area for '%s'...<z>\r\n", grep);
    else if (only_dir >= 0)
        n = snprintf(out, sizeof(out),
                     "\r\n<c>You peer intently to the %s...<z>\r\n",
                     DIR_NAMES[only_dir]);
    else
        n = snprintf(out, sizeof(out),
                     "\r\n<c>You scan the surrounding area...<z>\r\n");

    db_conn_t *db = db_open(DB_TOBIN);
    int start = me->base.roomp->vnum;
    bool found = false;
    for (int dir = 0; dir < ROOM_NUM_EXITS; dir++) {
        if (only_dir >= 0 && dir != only_dir)
            continue;
        int cur = start;
        for (int dist = 1; dist <= SCAN_MAX_RANGE; dist++) {
            int cond = 0;
            int dest = scan_exit(db, cur, dir, &cond);
            if (dest < 0)
                break;
            if (cond & (EXIT_COND_CLOSED | EXIT_COND_SECRET))
                break; /* a shut or hidden door blocks the line of sight */
            room_t *dr = world_get_room(dest);
            if (dr) {
                for (thing_t *t = dr->base.stuff_head; t; t = t->stuff_next) {
                    if (t->kind != THING_PC && t->kind != THING_MOB)
                        continue;
                    /* Linkdead PCs are visible (tagged) in a room's own
                     * `look` listing, but not scannable from a distance --
                     * same "not a real target" treatment as
                     * combat_find_room_target() (user: "scan should
                     * ignore linkdead chars"). */
                    if (t->kind == THING_PC && !((const being_t *)t)->desc)
                        continue;
                    if (grep && !ci_contains(t->name, grep))
                        continue;
                    if ((size_t)n >= sizeof(out))
                        break;
                    const char *label = t->short_descr[0] ? t->short_descr : t->name;
                    char capbuf[128];
                    n += snprintf(out + n, sizeof(out) - (size_t)n,
                                  "  %s <o>%s to the %s.<z>\r\n",
                                  cap_first(label, capbuf, sizeof(capbuf)),
                                  SCAN_DIST[dist], DIR_NAMES[dir]);
                    found = true;
                }
            }
            cur = dest;
        }
    }
    if (db)
        db_close(db);

    if (!found)
        n += snprintf(out + n, sizeof(out) - (size_t)n,
                      "  You see nothing of note.\r\n");
    descriptor_send(d, out);
    return true;
}
