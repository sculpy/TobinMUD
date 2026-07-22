/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "drug.h"
#include "drug_repo.h"
#include "obj.h"
#include "obj_repo.h"
#include "thing.h"

/* `smoke <item>` (Sneezy -> Tobin feature audit, "drug tracking"). Any
 * carried/worn/held item keyworded "drug" (same generic keyword-match
 * convention as spell components/holy symbols -- a builder can create
 * any themed item without a new object category) works: `val[0]` is
 * the real `drug_type_t`, `val[1]` current charges, `val[2]` max
 * charges (obj.h's val[] doc, same MAGIC_DEVICE-style precedent as
 * components/wands). See drug.h for the real effect/addiction/
 * withdrawal design and its documented deviations from the upstream. */

static obj_t *find_drug_item(const being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && thing_name_matches(t->name, tok, len)) {
            obj_t *o = (obj_t *)t;
            if (thing_name_matches(o->base.name, "drug", 4))
                return o;
        }
    }
    return NULL;
}

static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

bool cmd_smoke(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Smoke what?\r\n");
        return true;
    }

    obj_t *o = find_drug_item(ch, tok);
    if (!o) {
        descriptor_send(d, "You aren't carrying anything like that.\r\n");
        return true;
    }

    drug_type_t type = (drug_type_t)o->val[0];
    if (type < 0 || type >= DRUG_COUNT) {
        descriptor_send(d, "That doesn't seem to be anything you can smoke.\r\n");
        return true;
    }

    const char *msg = drug_smoke(ch, type);
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        char capbuf[128], roommsg[192];
        snprintf(roommsg, sizeof(roommsg), "%s lights up and takes a long smoke.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, roommsg);
    }

    if (ch->base.kind == THING_PC)
        drug_repo_save(ch->player_id, type, &ch->drugs[type]);

    /* Spends one charge -- destroys it only once that was the last one,
     * same convention consume_component()/consume_symbol() already use
     * (cmd_cast.c/cmd_pray.c). A pre-existing/never-charged item
     * (val[1]==0) is treated as a single fallback charge. */
    int charges = o->val[1] > 0 ? o->val[1] : 1;
    if (charges > 1) {
        o->val[1] = charges - 1;
    } else {
        char capbuf[128], usedmsg[192];
        const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
        snprintf(usedmsg, sizeof(usedmsg), "%s is spent.\r\n", cap_first(label, capbuf, sizeof(capbuf)));
        descriptor_send(d, usedmsg);
        obj_destroy(o);
    }
    if (ch->base.kind == THING_PC)
        player_inventory_save(ch->player_id, ch);

    return true;
}
