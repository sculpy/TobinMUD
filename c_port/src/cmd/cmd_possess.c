/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "room.h"
#include "thing.h"

/* `possess <mob>` / `return` (60+ -- Sneezy → Tobin feature audit,
 * "Switch / return (puppet a mob)"). Named `possess` rather than Sneezy's
 * own `switch` -- that word is already taken in Tobin (swap held items
 * between hands, cmd_object.c) -- but the mechanic is the real admin
 * switch (not the spell-driven polymorph flavor, which does stat
 * transfer and a transformation message): a raw descriptor-pointer swap,
 * nothing else. See descriptor.h's `possess_original` field comment for
 * the exact state shape. */

static being_t *find_mob(room_t *room, const char *tok) {
    const char *rest;
    int ordinal = thing_parse_ordinal(tok, &rest);
    size_t len = strlen(rest);
    int seen = 0;
    for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        if (thing_name_matches(t->name, rest, len)) {
            seen++;
            if (seen == ordinal)
                return (being_t *)t;
        }
    }
    return NULL;
}

bool cmd_possess(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    if (d->possess_original) {
        descriptor_send(d, "You're already possessing something -- `return` first.\r\n");
        return true;
    }

    char tok[64] = "";
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: possess <mob>\r\n");
        return true;
    }

    being_t *target = find_mob(ch->base.roomp, tok);
    if (!target) {
        descriptor_send(d, "You don't see that mob here.\r\n");
        return true;
    }
    if (target->desc) {
        descriptor_send(d, "Something else is already in control of that body.\r\n");
        return true;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "You possess %s.\r\n",
             target->base.short_descr[0] ? target->base.short_descr : target->base.name);
    descriptor_send(d, msg);

    d->possess_original = ch;
    ch->desc = NULL;
    d->character = target;
    target->desc = d;
    return true;
}

bool cmd_return(descriptor_t *d, const char *args) {
    (void)args;
    if (!d->possess_original) {
        descriptor_send(d, "You aren't possessing anything.\r\n");
        return true;
    }

    being_t *mob = d->character;
    being_t *original = d->possess_original;

    mob->desc = NULL;
    d->character = original;
    original->desc = d;
    d->possess_original = NULL;

    descriptor_send(d, "You return to your own body.\r\n");
    return true;
}
