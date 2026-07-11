/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "log.h"
#include "obj.h"
#include "obj_repo.h"
#include "room.h"
#include "thing.h"
#include "trigger.h"

/* short_descr is stored lowercase-first by convention ("a torch");
 * capitalize only when it starts a whole message (mid-sentence uses stay
 * lowercase, e.g. "You drop a torch."). Copies into `buf`. Skips any
 * leading inline color tag first (e.g. "<o>a dirty refuse hauler<1>",
 * real seeded content) -- same duplicated-helper bug as cmd_look.c's
 * cap_first(), fixed there Session 43 continued but missed here since
 * this is a separate copy, not a shared function. Found while working
 * nearby on the get/drop logging feature. */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

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

    /* `get all <container>` -- empty an entire container (corpse, chest, ...)
     * into your inventory in one go, rather than naming each item. User,
     * 2026-07-11: "corpses are supposed to act like containers. get all
     * corpse should get all items the player/mob was carrying upon death." */
    if (nargs == 2 && strcasecmp(tok, "all") == 0) {
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
        if (!cont->base.stuff_head) {
            char msg[256];
            const char *cl = cont->base.short_descr[0] ? cont->base.short_descr : cont->base.name;
            snprintf(msg, sizeof(msg), "There's nothing in %s.\r\n", cl);
            descriptor_send(d, msg);
            return true;
        }

        thing_t *t = cont->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next; /* thing_move_to() relinks t out of cont's list */
            obj_t *item = (obj_t *)t;
            thing_move_to(&item->base, &ch->base);

            game_log(LOG_SILENT, "%s gets %s (vnum %d) from %s (vnum %d) in room %d",
                     ch->base.name, item->base.short_descr, item->vnum,
                     cont->base.short_descr, cont->vnum, ch->base.roomp->vnum);

            char msg[512];
            const char *il = item->base.short_descr[0] ? item->base.short_descr : item->base.name;
            const char *cl = cont->base.short_descr[0] ? cont->base.short_descr : cont->base.name;
            snprintf(msg, sizeof(msg), "You get %s from %s.\r\n", il, cl);
            descriptor_send(d, msg);
            snprintf(msg, sizeof(msg), "%s gets %s from %s.\r\n", ch->base.name, il, cl);
            descriptor_room_echo(ch->base.roomp, ch, msg);

            room_t *here = ch->base.roomp;
            trigger_t trigs[8];
            int n = trigger_repo_load_for("obj", item->vnum, "get", trigs, 8);
            for (int i = 0; i < n; i++)
                trigger_run(&trigs[i], ch, here, NULL);

            t = next;
        }
        player_inventory_save(ch->player_id, ch);
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
        /* Dispute-research log (user: "anytime a char gets an item ... i
         * want those logged into the game log ... these should not be
         * reported via any log type, just inserted into the game log") --
         * LOG_SILENT is recorded to the file (searchable via `log search`)
         * but never echoed to immortals, exactly matching that spec. */
        game_log(LOG_SILENT, "%s gets %s (vnum %d) from %s (vnum %d) in room %d",
                 ch->base.name, item->base.short_descr, item->vnum,
                 cont->base.short_descr, cont->vnum, ch->base.roomp->vnum);
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
    game_log(LOG_SILENT, "%s gets %s (vnum %d) in room %d",
             ch->base.name, o->base.short_descr, o->vnum, ch->base.roomp->vnum);

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "You get %s.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s gets %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    {
        /* "get" triggers (user, 2026-07-11: "interaction with mobs objs
         * and room via scripts"). Room may have changed if the trigger
         * itself teleports the getter -- captured before running it. */
        room_t *here = ch->base.roomp;
        trigger_t trigs[8];
        int n = trigger_repo_load_for("obj", o->vnum, "get", trigs, 8);
        for (int i = 0; i < n; i++)
            trigger_run(&trigs[i], ch, here, NULL);
    }
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
    game_log(LOG_SILENT, "%s drops %s (vnum %d) in room %d",
             ch->base.name, o->base.short_descr, o->vnum, ch->base.roomp->vnum);

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
        char capbuf[128];
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s\r\n",
                      cap_first(label, capbuf, sizeof(capbuf)));
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  Nothing.\r\n");

    descriptor_send(d, out);
    return true;
}

/* Right-aligned "<label>: <value>" column, one limb/hand per line (user
 * 2026-07-09 reformat, replacing the old "<label> value" bracket form).
 * EQUIP_LABEL_WIDTH matches the longest label ("secondary hold"). */
#define EQUIP_LABEL_WIDTH 14

static void equip_line(char *out, size_t out_sz, size_t *n, const char *label,
                        const obj_t *o) {
    const char *value = o
        ? (o->base.short_descr[0] ? o->base.short_descr : o->base.name)
        : "nothing";
    *n += (size_t)snprintf(out + *n, out_sz - *n, "  %*s: %s\r\n",
                           EQUIP_LABEL_WIDTH, label, value);
}

bool cmd_equipment(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    char out[2048];
    size_t n = (size_t)snprintf(out, sizeof(out), "You are using:\r\n");
    for (int i = 0; i < LIMB_COUNT && n < sizeof(out); i++) {
        /* Genitalia is not a wear slot -- nothing is ever worn there (user
         * 2026-07-09); it becomes an object on decapitation instead, see
         * the crit-hit TODO item. Skip it from this listing entirely. */
        if (i == LIMB_GENITALIA)
            continue;
        equip_line(out, sizeof(out), &n, limb_name((limb_t)i), ch->equipment[i]);
    }
    /* "Primary"/"secondary" tracks the caller's dominant hand (handed_right),
     * not a fixed held[0]/held[1] -- held[] fills dominant-hand-first (see
     * do_hold_or_wield()), but which INDEX that is flips with handedness. */
    int primary = ch->handed_right ? 0 : 1;
    int secondary = ch->handed_right ? 1 : 0;
    if (n < sizeof(out))
        equip_line(out, sizeof(out), &n, "primary hold", ch->held[primary]);
    if (n < sizeof(out))
        equip_line(out, sizeof(out), &n, "secondary hold", ch->held[secondary]);

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
    /* Holdables split off `wear` entirely (user 2026-07-09): a weapon must
     * be `wield`ed, anything else holdable uses `hold` -- `wear` only
     * covers the body-slot (equipment[]) case from here down. */
    if (slot == WEAR_SLOT_HELD) {
        const char *verb = o->category == OBJ_CAT_WEAPON ? "wield" : "hold";
        snprintf(msg, sizeof(msg), "That isn't something you wear -- try `%s %s` instead.\r\n",
                 verb, tok);
        descriptor_send(d, msg);
        return true;
    }

    ch->equipment[slot] = o;
    snprintf(msg, sizeof(msg), "You wear %s on your %s.\r\n", label, limb_name((limb_t)slot));
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s wears %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    player_inventory_save(ch->player_id, ch);

    if (ch->base.roomp) {
        /* "wear" triggers (user, 2026-07-11: "interaction with mobs objs
         * and room via scripts"). */
        trigger_t trigs[8];
        int n = trigger_repo_load_for("obj", o->vnum, "wear", trigs, 8);
        for (int i = 0; i < n; i++)
            trigger_run(&trigs[i], ch, ch->base.roomp, NULL);
    }
    return true;
}

/* Shared by `hold` and `wield` (user 2026-07-09: split from the old unified
 * `wear`-onto-a-hand behavior) -- weapons must be wielded, everything else
 * holdable must be held; each verb refuses the other's kind of item. Fills
 * whichever hand is free, preferring the caller's dominant hand first (same
 * preference `wear` used to apply). */
static bool do_hold_or_wield(descriptor_t *d, const char *args, bool wielding) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    const char *verb = wielding ? "wield" : "hold";
    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Usage: %s <item>\r\n", verb);
        descriptor_send(d, msg);
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

    if (slot != WEAR_SLOT_HELD) {
        snprintf(msg, sizeof(msg), "You can't %s that.\r\n", verb);
        descriptor_send(d, msg);
        return true;
    }
    bool is_weapon = o->category == OBJ_CAT_WEAPON;
    if (wielding && !is_weapon) {
        descriptor_send(d, "Only weapons need to be wielded -- try `hold` instead.\r\n");
        return true;
    }
    if (!wielding && is_weapon) {
        descriptor_send(d, "A weapon must be wielded, not merely held -- try `wield` instead.\r\n");
        return true;
    }

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
    snprintf(msg, sizeof(msg), "You %s %s.\r\n", verb, label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s %ss %s.\r\n", ch->base.name, verb, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    player_inventory_save(ch->player_id, ch);
    return true;
}

bool cmd_hold(descriptor_t *d, const char *args) {
    return do_hold_or_wield(d, args, false);
}

bool cmd_wield(descriptor_t *d, const char *args) {
    return do_hold_or_wield(d, args, true);
}

/* Swaps whatever is in each hand with the other -- no unwielding/unholding
 * needed (user 2026-07-09). A no-op swap (both hands empty) is refused with
 * a friendlier message than a silent success. */
bool cmd_switch(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (!ch->held[0] && !ch->held[1]) {
        descriptor_send(d, "You aren't holding anything to switch.\r\n");
        return true;
    }

    obj_t *tmp = ch->held[0];
    ch->held[0] = ch->held[1];
    ch->held[1] = tmp;
    player_inventory_save(ch->player_id, ch);

    descriptor_send(d, "You switch hands.\r\n");
    char msg[256];
    snprintf(msg, sizeof(msg), "%s switches hands.\r\n", ch->base.name);
    descriptor_room_echo(ch->base.roomp, ch, msg);
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
