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
#include "obj_repo.h"
#include "room.h"
#include "thing.h"

/* get/drop/inventory/wear/remove/equipment -- the mortal-usable slice of
 * Phase 2C (see the plan notes / STATUS.md). `oload` (immortal, spawns a
 * prototype into a room) is cmd_oload.c; edobject (the menu editor) is a
 * separate future session.
 *
 * Containment: every obj_t a being has -- carried, worn, or held -- lives in
 * the ONE thing_t chain (ch->base.stuff_head, parent == &ch->base, attached/
 * detached via thing_move_to()/thing_remove_from_parent()). equipment[]/
 * held[] are fast-lookup pointers into that same set, not separate storage
 * (see being.h). "Carried" (as `inventory` shows it) means attached AND not
 * also pointed to by equipment[]/held[]. */

/* Multi-keyword object names ("bag large real") match if `tok` is a
 * case-insensitive prefix of ANY individual keyword -- same abbreviation
 * spirit as command/social matching, but per-word since (unlike a player's
 * single-word name) an object's base.name is a space-separated keyword
 * list straight from the DB. */
static bool obj_name_matches(const char *keywords, const char *tok, size_t tok_len) {
    if (tok_len == 0)
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

static bool is_loose(const being_t *ch, const obj_t *o) {
    if (ch->held[0] == o || ch->held[1] == o)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (ch->equipment[i] == o)
            return false;
    return true;
}

/* Finds an obj by keyword among a thing_t chain, optionally restricted to
 * "loose" items on `owner` (pass owner=NULL to skip that filter, e.g. when
 * searching a room floor). */
static obj_t *find_obj(thing_t *chain, const char *tok, const being_t *owner_for_loose_filter) {
    size_t len = strlen(tok);
    for (thing_t *t = chain; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (owner_for_loose_filter && !is_loose(owner_for_loose_filter, o))
            continue;
        if (obj_name_matches(t->name, tok, len))
            return o;
    }
    return NULL;
}

/* Finds a WORN/HELD obj by keyword -- the inverse filter of find_obj's
 * "loose" search, used by `remove`. */
static obj_t *find_worn(const being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    for (int i = 0; i < LIMB_COUNT; i++) {
        if (ch->equipment[i] && obj_name_matches(ch->equipment[i]->base.name, tok, len))
            return ch->equipment[i];
    }
    for (int i = 0; i < 2; i++) {
        if (ch->held[i] && obj_name_matches(ch->held[i]->base.name, tok, len))
            return ch->held[i];
    }
    return NULL;
}

bool cmd_get(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64], conttok[64];
    int nargs = sscanf(args, "%63s %63s", tok, conttok);
    if (nargs < 1) {
        descriptor_send(d, "Usage: get <item> [container]\r\n");
        return true;
    }

    /* `get <item> <container>` -- take an item out of a container (one you're
     * carrying/wearing, or one on the room floor). */
    if (nargs == 2) {
        obj_t *cont = find_obj(ch->base.stuff_head, conttok, NULL);
        if (!cont)
            cont = find_obj(ch->base.roomp->base.stuff_head, conttok, NULL);
        if (!cont) {
            descriptor_send(d, "You don't see that container here.\r\n");
            return true;
        }
        if (!obj_is_container(cont)) {
            descriptor_send(d, "That's not a container.\r\n");
            return true;
        }
        if (obj_container_closed(cont)) {
            descriptor_send(d, "It's closed.\r\n");
            return true;
        }
        obj_t *item = find_obj(cont->base.stuff_head, tok, NULL);
        if (!item) {
            descriptor_send(d, "You don't see that in there.\r\n");
            return true;
        }
        thing_move_to(&item->base, &ch->base);
        player_inventory_save(ch->player_id, ch);
        char msg[512];
        const char *il = item->base.short_descr[0] ? item->base.short_descr : item->base.name;
        const char *cl = cont->base.short_descr[0] ? cont->base.short_descr : cont->base.name;
        snprintf(msg, sizeof(msg), "You get %s from %s.\r\n", il, cl);
        descriptor_send(d, msg);
        snprintf(msg, sizeof(msg), "%s gets %s from %s.\r\n", ch->base.name, il, cl);
        descriptor_room_echo(ch->base.roomp, ch, msg);
        return true;
    }

    obj_t *o = find_obj(ch->base.roomp->base.stuff_head, tok, NULL);
    if (!o) {
        descriptor_send(d, "You don't see that here.\r\n");
        return true;
    }
    if (!obj_takeable(o->wear_flag)) {
        descriptor_send(d, "You can't take that.\r\n");
        return true;
    }

    thing_move_to(&o->base, &ch->base);
    player_inventory_save(ch->player_id, ch);

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "You get %s.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s gets %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}

/* `put <item> <container>` -- move a loose carried item into a container that
 * you're carrying/wearing or that's on the room floor. Refuses closed
 * containers and respects the weight capacity in val[0] (0 == unlimited). */
bool cmd_put(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char itemtok[64], conttok[64];
    if (sscanf(args, "%63s %63s", itemtok, conttok) != 2) {
        descriptor_send(d, "Usage: put <item> <container>\r\n");
        return true;
    }

    obj_t *item = find_obj(ch->base.stuff_head, itemtok, ch);
    if (!item) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }
    obj_t *cont = find_obj(ch->base.stuff_head, conttok, NULL);
    if (!cont)
        cont = find_obj(ch->base.roomp->base.stuff_head, conttok, NULL);
    if (!cont) {
        descriptor_send(d, "You don't see that container here.\r\n");
        return true;
    }
    if (item == cont) {
        descriptor_send(d, "You can't put something inside itself.\r\n");
        return true;
    }
    if (!obj_is_container(cont)) {
        descriptor_send(d, "That's not a container.\r\n");
        return true;
    }
    if (obj_container_closed(cont)) {
        descriptor_send(d, "It's closed.\r\n");
        return true;
    }
    if (cont->val[0] > 0 && obj_contained_weight(cont) + item->weight > (double)cont->val[0]) {
        descriptor_send(d, "It won't fit.\r\n");
        return true;
    }

    thing_move_to(&item->base, &cont->base);
    player_inventory_save(ch->player_id, ch);

    char msg[512];
    const char *il = item->base.short_descr[0] ? item->base.short_descr : item->base.name;
    const char *cl = cont->base.short_descr[0] ? cont->base.short_descr : cont->base.name;
    snprintf(msg, sizeof(msg), "You put %s in %s.\r\n", il, cl);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s puts %s in %s.\r\n", ch->base.name, il, cl);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}

bool cmd_drop(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: drop <item>\r\n");
        return true;
    }

    obj_t *o = find_obj(ch->base.stuff_head, tok, ch);
    if (!o) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }

    thing_move_to(&o->base, &ch->base.roomp->base);
    player_inventory_save(ch->player_id, ch);

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "You drop %s.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s drops %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}

bool cmd_inventory(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    char out[2048];
    int n = snprintf(out, sizeof(out), "You are carrying:\r\n");
    bool any = false;
    for (thing_t *t = ch->base.stuff_head; t && (size_t)n < sizeof(out); t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!is_loose(ch, o))
            continue;
        any = true;
        const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s\r\n", label);
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  Nothing.\r\n");

    descriptor_send(d, out);
    return true;
}

bool cmd_equipment(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    char out[2048];
    int n = snprintf(out, sizeof(out), "You are using:\r\n");
    for (int i = 0; i < LIMB_COUNT && (size_t)n < sizeof(out); i++) {
        const char *label = ch->equipment[i]
            ? (ch->equipment[i]->base.short_descr[0] ? ch->equipment[i]->base.short_descr : ch->equipment[i]->base.name)
            : "nothing";
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  <%s> %s\r\n", limb_name((limb_t)i), label);
    }
    static const char *const HELD_NAMES[2] = { "primary hand", "off hand" };
    for (int i = 0; i < 2 && (size_t)n < sizeof(out); i++) {
        const char *label = ch->held[i]
            ? (ch->held[i]->base.short_descr[0] ? ch->held[i]->base.short_descr : ch->held[i]->base.name)
            : "nothing";
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  <%s> %s\r\n", HELD_NAMES[i], label);
    }

    descriptor_send(d, out);
    return true;
}

bool cmd_wear(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: wear <item>\r\n");
        return true;
    }

    obj_t *o = find_obj(ch->base.stuff_head, tok, ch);
    if (!o) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }

    int slot = wear_slot_for_flag(o->wear_flag, ch);
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char msg[256];

    if (slot == WEAR_SLOT_NOT_WEARABLE) {
        descriptor_send(d, "You can't wear that.\r\n");
        return true;
    }
    if (slot == WEAR_SLOT_NO_ROOM) {
        descriptor_send(d, "You're already wearing something there.\r\n");
        return true;
    }
    if (slot == WEAR_SLOT_HELD) {
        int hand = -1;
        if (!ch->held[ch->handed_right ? 0 : 1])
            hand = ch->handed_right ? 0 : 1;
        else if (!ch->held[ch->handed_right ? 1 : 0])
            hand = ch->handed_right ? 1 : 0;
        if (hand < 0) {
            descriptor_send(d, "Your hands are full.\r\n");
            return true;
        }
        ch->held[hand] = o;
        snprintf(msg, sizeof(msg), "You wield %s.\r\n", label);
        descriptor_send(d, msg);
        snprintf(msg, sizeof(msg), "%s wields %s.\r\n", ch->base.name, label);
        descriptor_room_echo(ch->base.roomp, ch, msg);
        player_inventory_save(ch->player_id, ch);
        return true;
    }

    ch->equipment[slot] = o;
    snprintf(msg, sizeof(msg), "You wear %s on your %s.\r\n", label, limb_name((limb_t)slot));
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s wears %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    player_inventory_save(ch->player_id, ch);
    return true;
}

bool cmd_remove(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: remove <item>\r\n");
        return true;
    }

    obj_t *o = find_worn(ch, tok);
    if (!o) {
        descriptor_send(d, "You aren't wearing or holding that.\r\n");
        return true;
    }

    for (int i = 0; i < LIMB_COUNT; i++)
        if (ch->equipment[i] == o)
            ch->equipment[i] = NULL;
    for (int i = 0; i < 2; i++)
        if (ch->held[i] == o)
            ch->held[i] = NULL;

    player_inventory_save(ch->player_id, ch);

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "You remove %s.\r\n", label);
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        snprintf(msg, sizeof(msg), "%s removes %s.\r\n", ch->base.name, label);
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}
