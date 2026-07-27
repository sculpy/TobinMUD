/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "thing.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

/* Unlinks `t` from its current parent's intrusive stuff_head/stuff_next
 * list, if it has one, and clears t->parent/stuff_next. This is the shared
 * primitive every containment change (thing_move_to(), thing_set_room(),
 * obj_destroy(), etc) goes through first -- a thing is only ever in one
 * container's list at a time. No-op if `t` has no parent. */
void thing_remove_from_parent(thing_t *t) {
    if (!t || !t->parent)
        return;

    thing_t *parent = t->parent;
    if (parent->stuff_head == t) {
        parent->stuff_head = t->stuff_next;
    } else {
        thing_t *cur = parent->stuff_head;
        while (cur && cur->stuff_next != t)
            cur = cur->stuff_next;
        if (cur)
            cur->stuff_next = t->stuff_next;
    }

    t->stuff_next = NULL;
    t->parent = NULL;
}

/* Removes `t` from wherever it currently lives, then (unless `parent` is
 * NULL, e.g. destroying/extracting an item) pushes it onto `parent`'s
 * contents list as the new head. This is THE containment primitive: rooms,
 * beings, and objects all use the same thing_t stuff_head/stuff_next
 * list, so moving an item into a room, into a container, or into a
 * character's inventory is always just this one call. */
void thing_move_to(thing_t *t, thing_t *parent) {
    if (!t)
        return;

    thing_remove_from_parent(t);

    if (!parent)
        return;

    t->parent = parent;
    t->stuff_next = parent->stuff_head;
    parent->stuff_head = t;
}

/* thing_move_to() plus updating t->roomp, for the common case of moving
 * something directly into a room (as opposed to into a container or
 * inventory, which leaves roomp alone/stale until it's next looked up
 * through its parent chain). `r` is taken as `struct room *` rather than
 * room_t* so this header has no dependency on room.h -- safe because
 * room_t embeds thing_t as its first member (see thing.h's doc comment). */
void thing_set_room(thing_t *t, struct room *r) {
    if (!t)
        return;
    thing_move_to(t, (thing_t *)r);
    t->roomp = r;
}

/* True iff `tok` is a case-insensitive prefix of any one space-separated
 * word in `keywords` (e.g. typing "vrock" or "demon" both match a mob
 * keyword string of "vrock demon"). Generic over any thing_t.name, so it's
 * shared by combat targeting (combat.c) and `look <target>` (cmd_look.c)
 * rather than each reimplementing word-splitting. */
bool thing_name_matches(const char *keywords, const char *tok, size_t tok_len) {
    if (!keywords || !tok || tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}

/* Parses a leading "N." ordinal off `arg` (classic DikuMUD multi-target
 * convention -- "2.sword" means the SECOND sword among matches). Writes
 * whatever comes after the ordinal (or all of `arg`, if there was no
 * ordinal prefix) into *rest and returns the ordinal, always >= 1. A
 * malformed prefix (bare "2." with nothing after it, or a leading "0.") is
 * treated as no prefix at all, so a legitimately weird name starting with
 * a digit doesn't get input eaten out from under it. */
int thing_parse_ordinal(const char *arg, const char **rest) {
    *rest = arg;
    if (!arg || !isdigit((unsigned char)arg[0]))
        return 1;

    const char *p = arg;
    long n = 0;
    while (isdigit((unsigned char)*p)) {
        n = n * 10 + (*p - '0');
        p++;
    }
    if (*p != '.' || p[1] == '\0' || n < 1)
        return 1;

    *rest = p + 1;
    return (int)n;
}
