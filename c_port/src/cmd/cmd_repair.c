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
#include "obj.h"
#include "obj_repo.h"
#include "repair_repo.h"
#include "shop_repo.h"
#include "skill.h"
#include "thing.h"

/* Object maintenance tasks 3-4 (Sneezy → Tobin feature audit, "full
 * system" scope, user 2026-07-21). Checked the real upstream
 * `misc/repair.cc`/`disc/disc_warrior_blacksmithing.cc` first: a mature,
 * file-backed ticket system with a real-time repair delay and per-
 * MATERIAL repair skills (SKILL_BLACKSMITHING for metal, SKILL_REPAIR_
 * MONK for organic/wood/hide/rock, SKILL_REPAIR_CLERIC/DEIKHAN for holy
 * items, etc.). Tobin has no material-property system yet to gate on
 * (separate, still-open "Material properties" audit item), so this is
 * scoped to ONE `repair` skill (Warrior, matching "blacksmithing" most
 * closely) for self-repair, plus a DB-backed ticket (not a physical note
 * object) for the shop economy -- no real-time delay, ready immediately,
 * a deliberate Tobin-scale simplification (see tobin_migrations.sql's
 * comment). Depreciation and monogramming are real upstream mechanics,
 * carried over: every repair (self or shop) permanently lowers how high
 * `cur_struct` can ever be restored to again (`max_struct -
 * depreciation`, floored at 1); a successful SELF-repair also stamps the
 * item with the repairer's name (`obj_t.monogram`), cosmetic only. */

/* Same small local keyword-match helper every other cmd_*.c file in this
 * session duplicates rather than shares (cmd_object.c's own
 * obj_name_matches(), cmd_shop.c's keyword_matches(), etc.) -- searches
 * an object's whole thing_t chain (carried OR worn/held, unlike
 * cmd_object.c's "loose only" find_obj()) since the item most likely to
 * need repairing is whatever's currently equipped and just took
 * structure damage in a fight. */
static obj_t *find_any_obj(const being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    if (len == 0)
        return NULL;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        const char *p = t->name;
        while (*p) {
            while (*p == ' ')
                p++;
            const char *start = p;
            while (*p && *p != ' ')
                p++;
            size_t wlen = (size_t)(p - start);
            if (wlen >= len && strncasecmp(start, tok, len) == 0)
                return (obj_t *)t;
            if (*p == ' ')
                p++;
        }
    }
    return NULL;
}

/* Effective repair ceiling -- depreciation permanently eats into how high
 * cur_struct can ever be restored, same idea as the real upstream's
 * TObj::maxFix(). Floored at 1 so a heavily-depreciated item is always
 * still USABLE, just increasingly fragile. */
static int effective_max_struct(const obj_t *o) {
    int m = o->max_struct - o->depreciation;
    return m < 1 ? 1 : m;
}

/* Flat per-point gold cost for makeshift repair materials -- Tobin has no
 * commodity-shop/raw-material system to price this against for real (the
 * upstream's own repairPrice() shops for actual ore/leather/etc. at a
 * separate commodity shop, see repair.cc), so this is a simple stand-in:
 * costlier for a shop (skilled labor charged for) than doing it yourself. */
#define SELF_REPAIR_GOLD_PER_POINT 2
#define SHOP_REPAIR_GOLD_PER_POINT 5

/* `repair <item>` command: self-repair using the SKILL_REPAIR skill (see
 * file-top comment for why Tobin has just the one skill instead of
 * upstream's per-material set). Charges gold for makeshift materials
 * regardless of success/failure, rolls the skill on success, and on a
 * hit restores cur_struct to effective_max_struct() while bumping
 * depreciation and stamping the item's monogram. */
bool cmd_repair(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "repair")) {
        descriptor_send(d, "You don't know how to repair equipment yourself.\r\n");
        return true;
    }

    char tok[64] = "";
    sscanf(args, "%63s", tok);
    if (!*tok) {
        descriptor_send(d, "Repair what?\r\n");
        return true;
    }

    obj_t *o = find_any_obj(ch, tok);
    if (!o) {
        descriptor_send(d, "You don't have that.\r\n");
        return true;
    }
    if (o->max_struct <= 0) {
        descriptor_send(d, "That isn't the sort of thing that can be damaged or repaired.\r\n");
        return true;
    }
    int ceiling = effective_max_struct(o);
    if (o->cur_struct >= ceiling) {
        descriptor_send(d, "It's already in as good a condition as you can get it.\r\n");
        return true;
    }

    int needed = ceiling - o->cur_struct;
    int cost = needed * SELF_REPAIR_GOLD_PER_POINT;
    if (ch->progress.gold < cost) {
        char msg[128];
        snprintf(msg, sizeof(msg), "You'd need %d gold in makeshift materials to attempt that.\r\n", cost);
        descriptor_send(d, msg);
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "repair", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* Materials are spent either way -- a failed attempt still uses them
     * up, same "cost regardless of outcome" shape bash/kick/disarm's own
     * lag already established this session. */
    ch->progress.gold -= cost;

    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char msg[256];
    if (!success) {
        snprintf(msg, sizeof(msg), "You work on %s, but can't quite get it right. The attempt wastes your materials.\r\n", label);
        descriptor_send(d, msg);
        player_progress_save(ch->player_id, &ch->progress);
        return true;
    }

    o->cur_struct = ceiling;
    /* `advanced blacksmithing` (missing-skill audit, 2026-08-05): a
     * real, working difference from base `repair` -- an advanced
     * blacksmith's work doesn't wear the item down further. */
    if (!being_knows_skill(ch, "advanced blacksmithing"))
        o->depreciation += 1;
    snprintf(o->monogram, sizeof(o->monogram), "%s", being_display_name(ch));

    snprintf(msg, sizeof(msg), "You repair %s, mending it back into shape.\r\n", label);
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        char capbuf[128];
        char room_msg[224];
        snprintf(room_msg, sizeof(room_msg), "%s works on %s, mending it back into shape.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), label);
        descriptor_room_echo(ch->base.roomp, ch, room_msg);
    }

    player_progress_save(ch->player_id, &ch->progress);
    if (ch->base.kind == THING_PC)
        player_inventory_save(ch->player_id, ch);
    return true;
}

/* Finds the shop (if any) operating in `room`, confirming its keeper is
 * alive and present -- same "shop is closed without a live keeper" check
 * cmd_shop.c's own (file-local, not exported) find_active_shop() makes,
 * duplicated here rather than shared, same small-helper precedent this
 * whole session's new cmd_*.c files already follow. */
static being_t *find_repair_shop(room_t *room, shop_t *shop) {
    if (!room || !shop_repo_find_by_room(room->vnum, shop) || !shop_repo_is_repair(shop->shop_nr))
        return NULL;
    for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_MOB && t->id == shop->keeper)
            return (being_t *)t;
    }
    return NULL;
}

/* `submit <item>` command: hands an item over to a repair shop's keeper
 * in exchange for a DB-backed ticket (repair_ticket_create()) instead of
 * a real-time repair delay -- see file-top comment for why. The item is
 * destroyed on submission and only comes back (fixed) via cmd_retrieve(). */
bool cmd_submit(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp)
        return true;

    shop_t shop;
    being_t *keeper = find_repair_shop(ch->base.roomp, &shop);
    if (!keeper) {
        descriptor_send(d, "You don't see a repair shop here.\r\n");
        return true;
    }

    char tok[64] = "";
    sscanf(args, "%63s", tok);
    if (!*tok) {
        descriptor_send(d, "Submit what for repair?\r\n");
        return true;
    }

    obj_t *o = find_any_obj(ch, tok);
    if (!o) {
        descriptor_send(d, "You don't have that.\r\n");
        return true;
    }
    if (o->max_struct <= 0) {
        descriptor_send(d, "That isn't the sort of thing that can be repaired.\r\n");
        return true;
    }
    int ceiling = effective_max_struct(o);
    if (o->cur_struct >= ceiling) {
        char capbuf[128];
        char msg[256];
        snprintf(msg, sizeof(msg), "%s looks that over and shrugs: \"Nothing wrong with this one.\"\r\n",
                 being_display_name_cap(keeper, capbuf, sizeof(capbuf)));
        descriptor_send(d, msg);
        return true;
    }

    int price = (ceiling - o->cur_struct) * SHOP_REPAIR_GOLD_PER_POINT;
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;

    int id = repair_ticket_create(ch->player_id, shop.shop_nr, o->vnum, label,
                                   o->max_struct, o->depreciation, o->monogram, price);
    if (id < 0) {
        descriptor_send(d, "Something went wrong and the shop couldn't take your item.\r\n");
        return true;
    }

    char capbuf[128];
    char msg[300];
    snprintf(msg, sizeof(msg),
             "%s takes %s and hands you a small claim ticket. \"That'll be %d gold when you come back for it -- "
             "ticket number %d.\"\r\n",
             being_display_name_cap(keeper, capbuf, sizeof(capbuf)), label, price, id);
    descriptor_send(d, msg);

    obj_destroy(o);
    if (ch->base.kind == THING_PC)
        player_inventory_save(ch->player_id, ch);
    return true;
}

/* `retrieve <ticket#>` command: pays off a repair_ticket_t created by
 * cmd_submit() and reconstitutes the object from its prototype (the
 * original was destroyed on submission), carrying over the ticket's
 * saved depreciation/monogram and applying one more point of
 * depreciation for this repair. */
bool cmd_retrieve(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp)
        return true;

    shop_t shop;
    being_t *keeper = find_repair_shop(ch->base.roomp, &shop);
    if (!keeper) {
        descriptor_send(d, "You don't see a repair shop here.\r\n");
        return true;
    }

    int id = atoi(args);
    if (id <= 0) {
        descriptor_send(d, "Retrieve which ticket number?\r\n");
        return true;
    }

    repair_ticket_t ticket;
    if (!repair_ticket_find(id, ch->player_id, shop.shop_nr, &ticket)) {
        descriptor_send(d, "That doesn't match any ticket you have here.\r\n");
        return true;
    }
    if (ch->progress.gold < ticket.price) {
        char msg[128];
        snprintf(msg, sizeof(msg), "You need %d gold to pay for that repair.\r\n", ticket.price);
        descriptor_send(d, msg);
        return true;
    }

    obj_t *fixed = obj_create_from_proto(ticket.obj_vnum);
    if (!fixed) {
        descriptor_send(d, "Your item seems to have gone missing -- talk to an immortal.\r\n");
        return true;
    }
    fixed->depreciation = ticket.depreciation_before + 1;
    fixed->cur_struct = fixed->max_struct - fixed->depreciation;
    if (fixed->cur_struct < 1)
        fixed->cur_struct = 1;
    snprintf(fixed->monogram, sizeof(fixed->monogram), "%s", ticket.monogram);
    thing_move_to(&fixed->base, &ch->base);

    char capbuf[128];
    char msg[128 + REPAIR_TICKET_LABEL_LEN + 48];
    /* `item_label` is the object's own short_descr ("a dented shield"),
     * already carrying its own leading article -- "hands you %s" reads
     * right, "hands back your %s" would double up ("your a dented
     * shield"). */
    snprintf(msg, sizeof(msg), "%s hands you %s, good as new.\r\n",
             being_display_name_cap(keeper, capbuf, sizeof(capbuf)), ticket.item_label);
    descriptor_send(d, msg);

    ch->progress.gold -= ticket.price;
    repair_ticket_delete(id);
    player_progress_save(ch->player_id, &ch->progress);
    if (ch->base.kind == THING_PC)
        player_inventory_save(ch->player_id, ch);
    return true;
}

/* `tickets` command: lists the caller's outstanding repair_ticket_t rows
 * at the current shop, so they can see what's waiting and its price
 * before spending gold on cmd_retrieve(). */
bool cmd_tickets(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp)
        return true;

    shop_t shop;
    if (!find_repair_shop(ch->base.roomp, &shop)) {
        descriptor_send(d, "You don't see a repair shop here.\r\n");
        return true;
    }

    repair_ticket_t tickets[32];
    int count = repair_ticket_list_for_player(ch->player_id, shop.shop_nr, tickets, 32);
    if (count == 0) {
        descriptor_send(d, "You have no tickets waiting at this shop.\r\n");
        return true;
    }

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\nYour tickets here:\r\n");
    for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  #%d) %s -- %d gold\r\n",
                       tickets[i].id, tickets[i].item_label, tickets[i].price);
    }
    descriptor_send(d, out);
    return true;
}

/* `debride <item>` (missing-skill audit, 2026-08-05): real upstream
 * SKILL_DEBRIDE (disc_warrior_blacksmithing.cc's doDebride()) strips an
 * ITEM_RUSTY flag Tobin has no equivalent for -- ported as a real,
 * working inverse of `repair`'s own depreciation increment instead:
 * carefully working over an item undoes 1 point of its accumulated
 * wear, raising the ceiling `repair` can ever restore it to again
 * (cmd_repair.c's own effective_max_struct(), above). No gold cost
 * (unlike repair) -- just time and skill, matching the real upstream's
 * own "no material cost" shape (a start_task duration there; Tobin has
 * no matching multi-tick task system yet, resolved instantly here,
 * same "v1 scope" simplification as everywhere else in this session). */
bool cmd_debride(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "debride")) {
        descriptor_send(d, "You don't know how to debride equipment.\r\n");
        return true;
    }

    char tok[64] = "";
    sscanf(args, "%63s", tok);
    if (!*tok) {
        descriptor_send(d, "Debride what?\r\n");
        return true;
    }

    obj_t *o = find_any_obj(ch, tok);
    if (!o) {
        descriptor_send(d, "You don't have that.\r\n");
        return true;
    }
    if (o->max_struct <= 0) {
        descriptor_send(d, "That isn't the sort of thing that can be damaged or repaired.\r\n");
        return true;
    }
    if (o->depreciation <= 0) {
        descriptor_send(d, "It's already in as good a condition as it's ever been.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "debride", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char msg[256];
    if (!success) {
        snprintf(msg, sizeof(msg), "You work over %s, but can't quite undo any of its wear.\r\n", label);
        descriptor_send(d, msg);
        return true;
    }

    o->depreciation -= 1;
    snprintf(msg, sizeof(msg), "You carefully work over %s, undoing some of its wear.\r\n", label);
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        char capbuf[128];
        char room_msg[224];
        snprintf(room_msg, sizeof(room_msg), "%s carefully works over %s.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), label);
        descriptor_room_echo(ch->base.roomp, ch, room_msg);
    }
    return true;
}
