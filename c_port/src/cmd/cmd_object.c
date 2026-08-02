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
#include "combat.h"
#include "log.h"
#include "obj.h"
#include "obj_repo.h"
#include "player_repo.h"
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

/* Money-pile objects (category OBJ_CAT_MONEY -- the seeded "pile of gold"
 * treasure/quest-reward objects, val[0]=coin amount) aren't real inventory
 * items in Tobin's gold-coin-only economy (see [[Money system]]/shop_repo.h):
 * picking one up credits its coin amount straight to the wallet
 * (player_progress.gold) and destroys the object instead of letting it sit
 * as an inert prop forever (user 2026-07-17: "once you pick up an object
 * that contains gold it should increase your wealth and get rid of the
 * obj"). Returns true if `o` was money and has been consumed this way --
 * callers must not touch `o` (or move it into inventory) afterward. */
static bool pick_up_money(descriptor_t *d, being_t *ch, obj_t *o) {
    if (o->category != OBJ_CAT_MONEY)
        return false;

    int amount = o->val[0];
    ch->progress.gold += amount;
    player_progress_save(ch->player_id, &ch->progress);
    game_log(LOG_SILENT, "%s picks up %d gold (obj vnum %d) in room %d",
             ch->base.name, amount, o->vnum, ch->base.roomp->vnum);

    char msg[256];
    snprintf(msg, sizeof(msg), "You find %d gold.\r\n", amount);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s finds some gold.\r\n", ch->base.name);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    obj_destroy(o);
    return true;
}

/* True if `o` is neither held in a hand nor worn in an equipment slot --
 * "loose" inventory, the set `get`/`drop` normally operate on. */
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
 * searching a room floor). Supports the "N.keyword" ordinal prefix (user
 * 2026-07-11: "getting multiple objects, obj 2.obj 3.obj" should reach the
 * 2nd/3rd match instead of always the first) -- parsed once here so every
 * caller (get/drop/put/give/wear/...) gets it for free. */
static obj_t *find_obj(thing_t *chain, const char *tok, const being_t *owner_for_loose_filter) {
    const char *rest;
    int ordinal = thing_parse_ordinal(tok, &rest);
    size_t len = strlen(rest);
    int seen = 0;
    for (thing_t *t = chain; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (owner_for_loose_filter && !is_loose(owner_for_loose_filter, o))
            continue;
        if (obj_name_matches(t->name, rest, len)) {
            seen++;
            if (seen == ordinal)
                return o;
        }
    }
    return NULL;
}

/* Finds a WORN/HELD obj by keyword -- the inverse filter of find_obj's
 * "loose" search, used by `remove`. Same "N.keyword" ordinal support. */
static obj_t *find_worn(const being_t *ch, const char *tok) {
    const char *rest;
    int ordinal = thing_parse_ordinal(tok, &rest);
    size_t len = strlen(rest);
    int seen = 0;
    for (int i = 0; i < LIMB_COUNT; i++) {
        if (ch->equipment[i] && obj_name_matches(ch->equipment[i]->base.name, rest, len)) {
            seen++;
            if (seen == ordinal)
                return ch->equipment[i];
        }
    }
    for (int i = 0; i < 2; i++) {
        if (ch->held[i] && obj_name_matches(ch->held[i]->base.name, rest, len)) {
            seen++;
            if (seen == ordinal)
                return ch->held[i];
        }
    }
    return NULL;
}

/* `get all` (every takeable item on the room floor) and `get all.<name>`
 * (every item on the floor matching <name>, classic Diku dot-syntax) --
 * user 2026-07-26. `name_filter` NULL means "everything takeable"; a
 * non-NULL filter is matched the same case-insensitive per-keyword-prefix
 * way find_obj()'s single-item lookup already uses (obj_name_matches()),
 * just collecting every match instead of stopping at the first/Nth one.
 * Shares the same per-item message/log/trigger shape the single-item and
 * `get all <container>` paths above already use. */
static bool get_all_from_room(descriptor_t *d, being_t *ch, const char *name_filter) {
    size_t filter_len = name_filter ? strlen(name_filter) : 0;
    thing_t *t = ch->base.roomp->base.stuff_head;
    int gotten = 0;
    while (t) {
        thing_t *next = t->stuff_next; /* thing_move_to() relinks t out of the room's chain */
        if (t->kind == THING_OBJ) {
            obj_t *item = (obj_t *)t;
            if (!obj_takeable(item->wear_flag)) {
                t = next;
                continue;
            }
            if (name_filter && !obj_name_matches(item->base.name, name_filter, filter_len)) {
                t = next;
                continue;
            }
            if (pick_up_money(d, ch, item)) {
                gotten++;
                t = next;
                continue;
            }
            thing_move_to(&item->base, &ch->base);
            gotten++;

            game_log(LOG_SILENT, "%s gets %s (vnum %d) in room %d",
                     ch->base.name, item->base.short_descr, item->vnum, ch->base.roomp->vnum);

            char msg[256];
            const char *label = item->base.short_descr[0] ? item->base.short_descr : item->base.name;
            snprintf(msg, sizeof(msg), "You get %s.\r\n", label);
            descriptor_send(d, msg);
            snprintf(msg, sizeof(msg), "%s gets %s.\r\n", ch->base.name, label);
            descriptor_room_echo(ch->base.roomp, ch, msg);

            room_t *here = ch->base.roomp;
            trigger_t trigs[8];
            int n = trigger_repo_load_for("obj", item->vnum, "get", trigs, 8);
            for (int i = 0; i < n; i++)
                trigger_run(&trigs[i], ch, here, NULL);
        }
        t = next;
    }
    if (gotten == 0) {
        descriptor_send(d, name_filter ? "You don't see any of those here.\r\n"
                                        : "There's nothing here to get.\r\n");
    } else {
        player_inventory_save(ch->player_id, ch);
    }
    return true;
}

/* The `get` command: `get <item>` picks up a loose item from the room
 * floor (or a container, if a second argument names one), with `all`/
 * `all.<name>`/`all <container>` bulk forms handled by get_all_from_room()
 * above. A money-pile object is credited straight to gold instead of
 * entering inventory -- see pick_up_money(). */
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

    /* `get all` (bare, no container) -- every takeable item on the room
     * floor. `get all.<name>` -- every item on the floor matching <name>.
     * Both single-token (nargs == 1), distinct from `get all <container>`
     * (nargs == 2) below. */
    if (nargs == 1 && strcasecmp(tok, "all") == 0)
        return get_all_from_room(d, ch, NULL);
    if (nargs == 1 && strncasecmp(tok, "all.", 4) == 0 && tok[4] != '\0')
        return get_all_from_room(d, ch, tok + 4);

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
            if (pick_up_money(d, ch, item)) {
                t = next;
                continue;
            }
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
        if (pick_up_money(d, ch, item))
            return true;
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
    if (pick_up_money(d, ch, o))
        return true;

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

/* The `drop` command: drops one loose carried item onto the room floor,
 * or (`drop all`) every loose item at once -- worn/held items are exempt
 * from `all` and must be `remove`d first, same as a single named drop. */
/* Drops one item to the floor, then checks the NEWBIE-item-disappears
 * rule (NEWBIE-item-drop feature, TODO.md priority item, 2026-08-02) --
 * ported from SneezyMUD's own `drop` (misc/inventory.cc): an
 * ITEM_NEWBIE-flagged item explodes in a flash of white light rather
 * than sitting on the floor, but only if it's empty (a newbie-issue
 * BAG full of real loot shouldn't vanish along with its contents).
 * Returns true if the item disappeared (already destroyed -- caller
 * must not touch `o` again). */
static bool drop_one_item_check_newbie(being_t *ch, obj_t *o) {
    if (!(o->action_flag & ITEM_NEWBIE) || o->base.stuff_head)
        return false;

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "The %s explodes in a flash of white light!\r\n", label);
    descriptor_room_echo(ch->base.roomp, NULL, msg);
    obj_destroy(o);
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

    /* `drop all` (user 2026-07-26) -- every LOOSE carried item (same
     * "not worn/held" filter find_obj's owner_for_loose_filter already
     * enforces for a single `drop <item>` -- remove it first to drop
     * something equipped), one at a time so each still gets its own
     * message/log/save exactly like a normal drop would. */
    if (strcasecmp(tok, "all") == 0) {
        thing_t *t = ch->base.stuff_head;
        int dropped = 0;
        while (t) {
            thing_t *next = t->stuff_next; /* thing_move_to() relinks t out of ch's chain */
            if (t->kind == THING_OBJ && is_loose(ch, (obj_t *)t)) {
                obj_t *o = (obj_t *)t;
                thing_move_to(&o->base, &ch->base.roomp->base);
                game_log(LOG_SILENT, "%s drops %s (vnum %d) in room %d",
                         ch->base.name, o->base.short_descr, o->vnum, ch->base.roomp->vnum);
                char msg[256];
                const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
                snprintf(msg, sizeof(msg), "You drop %s.\r\n", label);
                descriptor_send(d, msg);
                snprintf(msg, sizeof(msg), "%s drops %s.\r\n", ch->base.name, label);
                descriptor_room_echo(ch->base.roomp, ch, msg);
                drop_one_item_check_newbie(ch, o);
                dropped++;
            }
            t = next;
        }
        if (dropped == 0)
            descriptor_send(d, "You aren't carrying anything.\r\n");
        else
            player_inventory_save(ch->player_id, ch);
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
    drop_one_item_check_newbie(ch, o);
    return true;
}

/* `junk <item>` (Sneezy → Tobin feature audit, "Object manipulation
 * depth"). Checked Sneezy's own `junk` help topic first: "so worthless it
 * isn't even worth dropping... totally destroy the object with no chance
 * of recovery" -- a real command there too, not skill/spell-gated, so a
 * straight port. Same loose-carried-only scope as `drop` (find_obj's
 * owner filter) -- junking something worn/held requires removing it
 * first, same as dropping it would. No confirmation prompt (matches the
 * original's own "be explicit... no reimbursement if the coordination is
 * off" warning -- the destructiveness is the point, not a bug to guard
 * against). */
bool cmd_junk(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: junk <item>\r\n");
        return true;
    }

    obj_t *o = find_obj(ch->base.stuff_head, tok, ch);
    if (!o) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }

    char msg[256];
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    snprintf(msg, sizeof(msg), "You junk %s. It's gone for good.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s junks %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    game_log(LOG_SILENT, "%s junks %s (vnum %d)", ch->base.name, label, o->vnum);
    obj_destroy(o);
    player_inventory_save(ch->player_id, ch);
    return true;
}

/* Renders one inventory line (label + condition suffix, no leading
 * "  " indent or trailing \r\n -- the caller adds those). */
static void render_inventory_item(char *buf, size_t bufsz, const obj_t *o) {
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char capbuf[128];
    const char *cond = obj_condition_word(o);
    snprintf(buf, bufsz, "%s%s%s%s",
             cap_first(label, capbuf, sizeof(capbuf)),
             cond ? " (" : "", cond ? cond : "", cond ? ")" : "");
}

/* Object stacking (user 2026-07-26: "object stacking needs to work on
 * inventory") -- groups by the RENDERED line itself, same "identical
 * output -> one line, count it" technique cmd_look.c's group_room_items()
 * already uses for room-floor items/mobs, rather than a separate vnum-
 * equality check: two real prototype items of the same vnum in the same
 * condition tier render identical text and stack; two ephemeral items
 * (vnum 0, e.g. Planting's fruit/hide/meat) with the same label do too,
 * since they share the same label/condition-less rendering; anything
 * visually distinct (different condition, different label) stays its
 * own line. First-seen order preserved, same as room listings. */
#define INVENTORY_LINE_LEN 160
bool cmd_inventory(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    char lines[64][INVENTORY_LINE_LEN];
    int counts[64];
    int groups = 0;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!is_loose(ch, o))
            continue;

        char line[INVENTORY_LINE_LEN];
        render_inventory_item(line, sizeof(line), o);

        int i;
        for (i = 0; i < groups; i++) {
            if (strcmp(lines[i], line) == 0) {
                counts[i]++;
                break;
            }
        }
        if (i == groups && groups < 64) {
            snprintf(lines[groups], INVENTORY_LINE_LEN, "%s", line);
            counts[groups] = 1;
            groups++;
        }
    }

    char out[2048];
    int n = snprintf(out, sizeof(out), "You are carrying:\r\n");
    for (int i = 0; i < groups && (size_t)n < sizeof(out); i++) {
        if (counts[i] > 1)
            n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s (x%d)\r\n", lines[i], counts[i]);
        else
            n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s\r\n", lines[i]);
    }
    if (groups == 0 && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  Nothing.\r\n");

    descriptor_page_start(d, out, 0);
    return true;
}

/* The `equipment` command: lists everything `ch` currently has worn/held,
 * one line per slot -- see being_render_equipment() for the actual
 * rendering, shared with `look <person>`. */
bool cmd_equipment(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    char out[2048];
    size_t n = (size_t)snprintf(out, sizeof(out), "You are using:\r\n");
    /* Rendering itself (label column, genitalia skip, primary/secondary
     * hand ordering) moved to being_render_equipment() (being.c,
     * 2026-07-12) so `look <person>` can share it. */
    being_render_equipment(ch, out, sizeof(out), &n);

    descriptor_send(d, out);
    return true;
}

/* Wears a single already-located item onto its body equipment[] slot.
 * Shared by `wear <item>` and `wear all` (the latter calls this once per
 * carried item, so failure messages are only sent when `announce` is set --
 * `wear all` silently skips items that don't fit rather than spamming). */
static bool wear_one_item(descriptor_t *d, being_t *ch, obj_t *o, bool announce) {
    int slot = wear_slot_for_flag(o->wear_flag, ch);
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char msg[256];

    if (slot == WEAR_SLOT_NOT_WEARABLE) {
        if (announce)
            descriptor_send(d, "You can't wear that.\r\n");
        return false;
    }
    if (slot == WEAR_SLOT_NO_ROOM) {
        if (announce)
            descriptor_send(d, "You're already wearing something there.\r\n");
        return false;
    }
    /* Holdables split off `wear` entirely (user 2026-07-09): a weapon must
     * be `wield`ed, anything else holdable uses `hold` -- `wear` only
     * covers the body-slot (equipment[]) case from here down. */
    if (slot == WEAR_SLOT_HELD) {
        if (announce) {
            const char *verb = o->category == OBJ_CAT_WEAPON ? "wield" : "hold";
            snprintf(msg, sizeof(msg), "That isn't something you wear -- try `%s %s` instead.\r\n",
                     verb, o->base.name);
            descriptor_send(d, msg);
        }
        return false;
    }

    ch->equipment[slot] = o;
    obj_apply_equip_affects(ch, o, 1);
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

/* The `wear` command: puts a carried item into its body equipment[] slot
 * (armor, jewelry, etc). Weapons and other holdables are redirected to
 * `wield`/`hold` instead -- see the WEAR_SLOT_HELD branch below and the
 * do_hold_or_wield() comment further down for why they split off.
 * `wear all` (SneezyMUD canonical syntax) wears everything carried that
 * fits an open body slot, one item at a time, silently skipping anything
 * unwearable/held/already-occupied. */
bool cmd_wear(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: wear <item>\r\n");
        return true;
    }

    if (strcasecmp(tok, "all") == 0) {
        int worn = 0;
        /* Snapshot the chain up front: wear_one_item() doesn't unlink the
         * object from stuff_head (worn items stay in inventory, just also
         * referenced from equipment[]), so a plain forward walk is safe. */
        for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_OBJ)
                continue;
            obj_t *o = (obj_t *)t;
            if (wear_one_item(d, ch, o, false))
                worn++;
        }
        if (worn == 0)
            descriptor_send(d, "You have nothing to wear.\r\n");
        return true;
    }

    obj_t *o = find_obj(ch->base.stuff_head, tok, ch);
    if (!o) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }

    wear_one_item(d, ch, o, true);
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

/* The `remove` command: takes off a worn or held/wielded item, unapplying
 * its stat affects if it was worn gear (see the inline comment below on
 * why held/wielded items don't need that). */
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

    bool was_worn = false;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (ch->equipment[i] == o) {
            ch->equipment[i] = NULL;
            was_worn = true;
        }
    for (int i = 0; i < 2; i++)
        if (ch->held[i] == o)
            ch->held[i] = NULL;
    /* Only equipment[] slots (WORN gear -- rings, armor, cloaks) carry
     * stat/HP/Vitality affects, not held[]/wielded weapons (those get
     * hit/damroll bonuses instead, applied live at attack time via
     * obj_load_combat_mods() -- no wear/remove hook needed for those). */
    if (was_worn)
        obj_apply_equip_affects(ch, o, -1);

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

/* Finds a PC/mob in `ch`'s room by name/keyword, for `give`'s recipient --
 * deliberately NOT combat_find_room_target() (combat.c), whose
 * combat_pk_allowed() gate exists to stop one PC from being auto-targeted
 * into a fight they haven't consented to (`toggle pk`) and would wrongly
 * block a perfectly friendly item/gold hand-off between two PCs who
 * never toggled PK at all. Same linkdead-exclusion and exact-name-first/
 * prefix-fallback shape as that function otherwise, just without the PK
 * check. */
static being_t *find_give_target(being_t *ch, const char *name) {
    size_t name_len = strlen(name);
    being_t *prefix_match = NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t == &ch->base)
            continue;
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (t->kind == THING_PC && !((being_t *)t)->desc)
            continue;
        if (strcasecmp(t->name, name) == 0)
            return (being_t *)t;
        if (!prefix_match && thing_name_matches(t->name, name, name_len))
            prefix_match = (being_t *)t;
    }
    return prefix_match;
}

/* `give <amount> gold <person>` / `give <item> <person>` -- object
 * manipulation audit continued (2026-07-29, a fresh Sneezy-vs-Tobin
 * comparison beyond the earlier narrow sacrifice/junk/identify pass):
 * real upstream `TBeing::doGive()` (misc/inventory.cc) was the one
 * gap found with no matching Tobin command at all. Ported at Tobin
 * scale: one item (or "all" of your gold) to one recipient present in
 * the room, PC or mob alike -- not ported: multi-item "give all.sword"/
 * "give 3.sword" bunching (getabunch(), the same N-at-once idea `drop
 * all` already covers differently), mob shop-response/spec-proc hooks
 * (checkResponses()/checkSpec(), no shop-logging or spec-proc dispatch
 * exists here to hook into), the solo-quest/group-quest refusal checks
 * (neither system exists in Tobin), and the hasHands()/no-hands refusal
 * (Tobin has no such capability flag). Refuses mid-fight on either side,
 * same real-upstream check, since accepting a hand-off during combat
 * makes no sense either way. */
bool cmd_give(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while fighting.\r\n");
        return true;
    }

    char tok1[64], tok2[64], tok3[64];
    int n = sscanf(args, "%63s %63s %63s", tok1, tok2, tok3);

    if (n == 3 && strcasecmp(tok2, "gold") == 0) {
        int amount = atoi(tok1);
        if (amount <= 0) {
            descriptor_send(d, "Sorry, you can't do that!\r\n");
            return true;
        }
        if (ch->progress.gold < amount) {
            descriptor_send(d, "You don't have that much gold!\r\n");
            return true;
        }
        being_t *vict = find_give_target(ch, tok3);
        if (!vict) {
            descriptor_send(d, "They aren't here.\r\n");
            return true;
        }
        if (vict->fighting) {
            char msg[192];
            snprintf(msg, sizeof(msg), "Not while %s is fighting.\r\n", being_display_name(vict));
            descriptor_send(d, msg);
            return true;
        }

        ch->progress.gold -= amount;
        vict->progress.gold += amount;
        if (ch->base.kind == THING_PC)
            player_progress_save(ch->player_id, &ch->progress);
        if (vict->base.kind == THING_PC)
            player_progress_save(vict->player_id, &vict->progress);

        char msg[192], capbuf[128];
        snprintf(msg, sizeof(msg), "You give %d gold to %s.\r\n", amount, being_display_name(vict));
        descriptor_send(d, msg);
        if (vict->desc) {
            snprintf(msg, sizeof(msg), "%s gives you %d gold.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)), amount);
            descriptor_notify(vict->desc, msg);
        }
        snprintf(msg, sizeof(msg), "%s gives some gold to %s.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), being_display_name(vict));
        descriptor_room_echo(ch->base.roomp, ch, msg);
        return true;
    }

    if (n < 2) {
        descriptor_send(d, "Give what to whom?\r\n");
        return true;
    }

    obj_t *item = find_obj(ch->base.stuff_head, tok1, ch);
    if (!item) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }
    being_t *vict = find_give_target(ch, tok2);
    if (!vict) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (vict->fighting) {
        char msg[192];
        snprintf(msg, sizeof(msg), "Not while %s is fighting.\r\n", being_display_name(vict));
        descriptor_send(d, msg);
        return true;
    }

    thing_move_to(&item->base, &vict->base);
    player_inventory_save(ch->player_id, ch);
    if (vict->base.kind == THING_PC)
        player_inventory_save(vict->player_id, vict);

    char msg[256], capbuf[128];
    const char *label = item->base.short_descr[0] ? item->base.short_descr : item->base.name;
    snprintf(msg, sizeof(msg), "You give %s to %s.\r\n", label, being_display_name(vict));
    descriptor_send(d, msg);
    if (vict->desc) {
        snprintf(msg, sizeof(msg), "%s gives you %s.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), label);
        descriptor_notify(vict->desc, msg);
    }
    snprintf(msg, sizeof(msg), "%s gives %s to %s.\r\n",
             being_display_name_cap(ch, capbuf, sizeof(capbuf)), label, being_display_name(vict));
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
