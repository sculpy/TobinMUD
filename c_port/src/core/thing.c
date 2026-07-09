/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "thing.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

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

void thing_set_room(thing_t *t, struct room *r) {
    if (!t)
        return;
    thing_move_to(t, (thing_t *)r);
    t->roomp = r;
}

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
