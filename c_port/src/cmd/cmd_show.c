/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj.h"
#include "room.h"
#include "thing.h"

/* `show <item> <person>` (Sneezy port, user 2026-07-12). Sneezy's own
 * `show.cc` is a sprawling immortal admin utility (room/zone listings
 * etc) that's really a different feature from what most players mean
 * by "show" -- and those admin views are already covered by this
 * backlog's own `stat`/`zone list` items. This is the ordinary social
 * meaning instead: hold an item up for someone else in the room to
 * see. Purely a message -- the item never changes hands (that's
 * `give`, not yet ported). */
bool cmd_show(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char item_tok[64] = "", who_tok[64] = "";
    if (sscanf(args, "%63s %63s", item_tok, who_tok) != 2) {
        descriptor_send(d, "Usage: show <item> <person>\r\n");
        return true;
    }

    size_t item_len = strlen(item_tok);
    obj_t *item = NULL;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        if (thing_name_matches(t->name, item_tok, item_len)) {
            item = (obj_t *)t;
            break;
        }
    }
    if (!item) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }

    size_t who_len = strlen(who_tok);
    being_t *target = NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC || t == &ch->base)
            continue;
        if (thing_name_matches(t->name, who_tok, who_len)) {
            target = (being_t *)t;
            break;
        }
    }
    if (!target) {
        char out[128];
        snprintf(out, sizeof(out), "You don't see '%s' here.\r\n", who_tok);
        descriptor_send(d, out);
        return true;
    }

    const char *label = item->base.short_descr[0] ? item->base.short_descr : item->base.name;
    char out[300];
    snprintf(out, sizeof(out), "You show %s to %s.\r\n", label, target->base.name);
    descriptor_send(d, out);
    if (target->desc) {
        snprintf(out, sizeof(out), "%s shows you %s.\r\n", ch->base.name, label);
        descriptor_notify(target->desc, out);
    }
    return true;
}
