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

#include "affect.h"
#include "being.h"
#include "cmd.h"
#include "material.h"
#include "obj.h"
#include "obj_repo.h"
#include "room.h"
#include "room_repo.h"
#include "shop_repo.h"
#include "thing.h"
#include "treasury_repo.h"

/* Flat sales tax on ordinary `buy` purchases (Money system v2, Sneezy →
 * Tobin feature audit) -- see cmd_buy()'s own comment. */
#define SALES_TAX_RATE 0.05

/* Same keyword-abbreviation matching spirit as cmd_object.c's
 * obj_name_matches() -- duplicated locally rather than shared, same
 * precedent as cmd_drink.c's own copy. */
static bool keyword_matches(const char *keywords, const char *tok) {
    size_t tok_len = strlen(tok);
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
        if (*p == ' ')
            p++;
    }
    return false;
}

/* Same duplication precedent as cmd_object.c's cap_first(). */
static const char *cap_first(const char *s, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", s);
    if (buf[0])
        buf[0] = (char)toupper((unsigned char)buf[0]);
    return buf;
}

/* Finds the shop (if any) operating in `room`, AND confirms its keeper mob
 * is actually alive and present there right now -- classic Diku "the shop
 * is closed if the keeper's dead or gone" behavior (a shop with no live
 * keeper can't be interacted with, even though shop_repo still has the
 * row). Returns NULL (leaving *shop untouched) if either check fails. */
static being_t *find_active_shop(room_t *room, shop_t *shop) {
    if (!room || !shop_repo_find_by_room(room->vnum, shop))
        return NULL;
    for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_MOB && t->id == shop->keeper)
            return (being_t *)t;
    }
    return NULL;
}

/* Resale stock (user, 2026-08-04: "the shop should attempt to resell the
 * item at a profit" -- reversing cmd_sell()'s original "destroyed on sale,
 * avoids unbounded shopkeeper inventory growth" simplification). A sold
 * item now moves into the keeper's OWN inventory (thing_move_to()) instead
 * of being destroyed, and `list`/`buy` both also draw from it, numbered/
 * matched right after the shop's static `shopproducing` catalog -- priced
 * the SAME way a fresh catalog item is (`.price * profit_buy`, material
 * multiplier included), which is inherently a markup over whatever the
 * shop paid the original seller (`profit_sell` is always < `profit_buy`,
 * shop_repo.h's own struct comment). SHOP_RESALE_MAX caps how many resold
 * items one keeper holds at once -- the original unbounded-growth concern
 * is real over a long server lifetime, so the OLDEST resale item is
 * destroyed outright to make room once the cap is hit, rather than
 * growing forever. */
#define SHOP_RESALE_MAX 20

static int count_resale_items(const being_t *keeper) {
    int n = 0;
    for (thing_t *t = keeper->base.stuff_head; t; t = t->stuff_next)
        if (t->kind == THING_OBJ)
            n++;
    return n;
}

/* Finds the `idx`-th (1-based) resold item in `keeper`'s own inventory,
 * in stuff_head order -- same "list position -> buy <#>" convention
 * shop_repo_producing()'s catalog already uses, just numbered to
 * continue right after it (see cmd_list()/cmd_buy()). */
static obj_t *find_resale_item_by_index(const being_t *keeper, int idx) {
    int n = 0;
    for (thing_t *t = keeper->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        n++;
        if (n == idx)
            return (obj_t *)t;
    }
    return NULL;
}

static obj_t *find_resale_item_by_keyword(const being_t *keeper, const char *tok) {
    for (thing_t *t = keeper->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        if (keyword_matches(t->name, tok))
            return (obj_t *)t;
    }
    return NULL;
}

/* Hospital (TODO.md: "add hospital code to the todo list" -- limb repair
 * + disease cures, ported from the original's hospital.cc `doctor` spec-
 * proc). A hospital shop's own `shopproducing` is empty -- nothing
 * physical to sell -- so `list`/`buy` branch here instead whenever
 * shop_repo_is_hospital() says the active shop's keeper is a doctor.
 * Deliberately NOT a port of the original's per-level-squared pricing
 * formula or its wound-flag (PART_BLEEDING/BROKEN/...) tiers -- Tobin has
 * no wound-flag tracking yet (see TODO.md's "Limbs.h gap review"), just
 * raw limb HP, so pricing is simply 1 gold per missing point, and every
 * limb below full is listed (not just the badly hurt ones -- a hospital
 * can top off a scratch same as a shattered arm). */
typedef struct {
    bool is_limb;
    limb_t limb;
    affect_type_t affect; /* the cure target when !is_limb -- a disease or AFFECT_POISON */
    int price;
} ailment_t;

#define HOSPITAL_MAX_AILMENTS (LIMB_COUNT + MAX_ACTIVE_AFFECTS)

/* Enumerates `ch`'s current ailments (damaged limbs, then active
 * diseases/poison) into `out` (capacity `max`), in a STABLE order
 * (LIMB_COUNT order, then MAX_ACTIVE_AFFECTS slot order) -- both
 * list_hospital() and buy_hospital_cure() call this fresh each time
 * rather than caching, so the two always agree on what number means
 * what, the same "recompute, don't cache" contract cmd_edaccount.c's
 * menu uses. Returns the count found. */
static int hospital_ailments(const being_t *ch, ailment_t *out, int max) {
    int n = 0;
    for (int i = 0; i < LIMB_COUNT && n < max; i++) {
        if (ch->limbs[i].hp < ch->limbs[i].max_hp) {
            int missing = ch->limbs[i].max_hp - ch->limbs[i].hp;
            out[n].is_limb = true;
            out[n].limb = (limb_t)i;
            out[n].price = missing > 0 ? missing : 1;
            n++;
        }
    }
    for (int i = 0; i < MAX_ACTIVE_AFFECTS && n < max; i++) {
        affect_type_t type = ch->affects[i].type;
        if (affect_is_disease(type) || type == AFFECT_POISON) {
            out[n].is_limb = false;
            out[n].affect = type;
            out[n].price = affect_cure_price(type);
            n++;
        }
    }
    return n;
}

/* `list` at a hospital shop: shows the numbered ailment menu built by
 * hospital_ailments(), with each row's gold price -- the hospital
 * equivalent of an ordinary shop's product listing. */
static void list_hospital(descriptor_t *d, being_t *ch, being_t *keeper) {
    ailment_t ailments[HOSPITAL_MAX_AILMENTS];
    int count = hospital_ailments(ch, ailments, HOSPITAL_MAX_AILMENTS);

    char keeper_name[128];
    being_display_name_cap(keeper, keeper_name, sizeof(keeper_name));

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\n%s looks you over:\r\n", keeper_name);
    for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
        if (ailments[i].is_limb) {
            int pct = ch->limbs[ailments[i].limb].max_hp > 0
                ? (ch->limbs[ailments[i].limb].hp * 100) / ch->limbs[ailments[i].limb].max_hp
                : 0;
            n += snprintf(out + n, sizeof(out) - (size_t)n, " %2d) Your %-16s (%3d%% health)      %d gold\r\n",
                          i + 1, limb_name(ailments[i].limb), pct, ailments[i].price);
        } else {
            n += snprintf(out + n, sizeof(out) - (size_t)n, " %2d) %-25s                %d gold\r\n",
                          i + 1, affect_name(ailments[i].affect), ailments[i].price);
        }
    }
    if (count == 0 && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  You look perfectly healthy to me.\r\n");
    else if ((size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "\r\n(buy <#> to be treated)\r\n");
    descriptor_page_start(d, out, 0);
}

/* `buy <#>` at a hospital shop: re-enumerates hospital_ailments() (same
 * stable order list_hospital() just showed), charges gold for the
 * chosen row, and either tops off the limb's HP or removes the disease/
 * poison affect. */
static void buy_hospital_cure(descriptor_t *d, being_t *ch, being_t *keeper, const shop_t *shop, const char *args) {
    ailment_t ailments[HOSPITAL_MAX_AILMENTS];
    int count = hospital_ailments(ch, ailments, HOSPITAL_MAX_AILMENTS);

    char tok[64] = "";
    sscanf(args, "%63s", tok);
    bool all_digits = tok[0] != '\0';
    for (const char *p = tok; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            all_digits = false;
            break;
        }
    }
    int idx = all_digits ? atoi(tok) : -1;
    if (idx < 1 || idx > count) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop->no_such_item1);
        descriptor_send(d, msg);
        return;
    }

    ailment_t ail = ailments[idx - 1];
    if (ch->progress.gold < ail.price) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop->missing_cash1);
        descriptor_send(d, msg);
        return;
    }

    char keeper_name[128];
    being_display_name_cap(keeper, keeper_name, sizeof(keeper_name));

    char confirm[300];
    if (ail.is_limb) {
        ch->limbs[ail.limb].hp = ch->limbs[ail.limb].max_hp;
        snprintf(confirm, sizeof(confirm), "%s tends to your %s -- it feels much better!\r\n",
                 keeper_name, limb_name(ail.limb));
    } else {
        being_remove_affect(ch, ail.affect);
        snprintf(confirm, sizeof(confirm), "%s administers a cure for your %s!\r\n",
                 keeper_name, affect_name(ail.affect));
    }
    descriptor_send(d, confirm);

    char paid[SHOP_MSG_LEN + 16];
    snprintf(paid, sizeof(paid), shop->message_buy, ail.price);
    strncat(paid, "\r\n", sizeof(paid) - strlen(paid) - 1);
    descriptor_send(d, paid);

    ch->progress.gold -= ail.price;
    player_progress_save(ch->player_id, &ch->progress);
}

/* Stable (Mount / riding system, Sneezy → Tobin feature audit -- user,
 * AskUserQuestion 2026-07-19: "a simple immortal-stocked stable, using
 * the existing shop system"). Same "own `shopproducing` is empty,
 * `list`/`buy` special-case it" shape as the hospital above -- a stable
 * has nothing physical to sell either. v1 sells exactly one thing (a
 * tame plow-horse, mob vnum 558, real seeded data -- race=47/HORSE,
 * level 6) at a flat price; more variety can join this list later
 * without touching cmd_list()/cmd_buy() at all. */
#define STABLE_HORSE_VNUM 558
#define STABLE_HORSE_PRICE 100

/* `list` at a stable: shows the (currently single-entry) horse menu --
 * see the stable block comment above for why this is hardcoded to one
 * item rather than driven by shopproducing. */
static void list_stable(descriptor_t *d, being_t *ch, being_t *keeper) {
    (void)ch;
    char keeper_name[128];
    being_display_name_cap(keeper, keeper_name, sizeof(keeper_name));

    char out[512];
    snprintf(out, sizeof(out),
             "\r\n%s waves you over to the stalls:\r\n"
             " 1) A sturdy plow-horse                    %d gold\r\n"
             "\r\n(buy <#> or buy horse)\r\n",
             keeper_name, STABLE_HORSE_PRICE);
    descriptor_page_start(d, out, 0);
}

/* `buy <#>`/`buy horse` at a stable: charges STABLE_HORSE_PRICE and
 * spawns a fresh plow-horse mob (STABLE_HORSE_VNUM) into the room for
 * the buyer to `ride`. */
static void buy_stable_horse(descriptor_t *d, being_t *ch, being_t *keeper, const shop_t *shop, const char *args) {
    char tok[16] = "";
    sscanf(args, "%15s", tok);
    if (strcasecmp(tok, "1") != 0 && !keyword_matches("horse plowhorse plow-horse", tok)) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop->no_such_item1);
        descriptor_send(d, msg);
        return;
    }
    if (ch->progress.gold < STABLE_HORSE_PRICE) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop->missing_cash1);
        descriptor_send(d, msg);
        return;
    }

    being_t *horse = being_create_mob(STABLE_HORSE_VNUM);
    if (!horse) {
        descriptor_send(d, "Something went wrong -- no horse could be found for you.\r\n");
        return;
    }
    thing_set_room(&horse->base, ch->base.roomp);

    char keeper_name[128];
    being_display_name_cap(keeper, keeper_name, sizeof(keeper_name));
    char confirm[256];
    snprintf(confirm, sizeof(confirm), "%s leads a horse out of the stalls for you. Try `ride plow-horse`.\r\n",
             keeper_name);
    descriptor_send(d, confirm);

    char paid[SHOP_MSG_LEN + 16];
    snprintf(paid, sizeof(paid), shop->message_buy, STABLE_HORSE_PRICE);
    strncat(paid, "\r\n", sizeof(paid) - strlen(paid) - 1);
    descriptor_send(d, paid);

    ch->progress.gold -= STABLE_HORSE_PRICE;
    player_progress_save(ch->player_id, &ch->progress);
}

/* `list` (user 2026-07-17: "implement money and shops"): shows the active
 * shop's wares from its `shopproducing` catalog (see shop_repo.h -- NOT
 * the keeper mob's own carried items; the seeded zone-reset data never
 * actually stocks these keepers that way), priced at each item's own
 * prototype `price` times the shop's `profit_buy` multiplier. Every
 * listed item is always available -- a shop never "runs out" of what it
 * produces.
 *
 * Numbered (user 2026-07-17: "number the list of items in a shop so a
 * player can buy #") -- each line's number is its 1-based position in
 * shop_repo_producing()'s (stable, ORDER BY producing) vnum list, and
 * cmd_buy() below indexes into that exact same list, so the number always
 * means the same item whether or not `list` was run first this session. */
bool cmd_list(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    shop_t shop;
    being_t *keeper = find_active_shop(ch->base.roomp, &shop);
    if (!keeper) {
        descriptor_send(d, "You don't see a shop here.\r\n");
        return true;
    }
    if (shop_repo_is_hospital(shop.shop_nr)) {
        list_hospital(d, ch, keeper);
        return true;
    }
    if (shop_repo_is_stable(shop.shop_nr)) {
        list_stable(d, ch, keeper);
        return true;
    }

    int vnums[SHOP_PRODUCING_MAX];
    int count = 0;
    shop_repo_producing(shop.shop_nr, vnums, SHOP_PRODUCING_MAX, &count);

    char keeper_name[128];
    being_display_name_cap(keeper, keeper_name, sizeof(keeper_name));

    char out[4096];
    int n = snprintf(out, sizeof(out), "\r\n%s offers:\r\n", keeper_name);
    int shown = 0;
    for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
        obj_proto_t proto;
        if (!obj_proto_load(vnums[i], &proto))
            continue;
        int price = (int)(proto.price * shop.profit_buy);
        char capbuf[128];
        const char *label = proto.short_descr[0] ? proto.short_descr : proto.name;
        n += snprintf(out + n, sizeof(out) - (size_t)n, " %2d) %-42s %d gold\r\n",
                      i + 1, cap_first(label, capbuf, sizeof(capbuf)), price);
        shown++;
    }
    /* Resale stock (see the doc comment on the helpers above) -- numbered
     * continuing right after the catalog, so `buy <#>` can index into
     * either range without ambiguity. */
    int resale_shown = 0;
    for (thing_t *t = keeper->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ || (size_t)n >= sizeof(out))
            continue;
        obj_t *o = (obj_t *)t;
        int price = (int)(o->price * shop.profit_buy
                           * material_tier_value_mult(material_tier_for_id(o->material)));
        char capbuf[128];
        const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
        n += snprintf(out + n, sizeof(out) - (size_t)n, " %2d) %-42s %d gold (used)\r\n",
                      count + resale_shown + 1, cap_first(label, capbuf, sizeof(capbuf)), price);
        resale_shown++;
    }

    if (shown == 0 && resale_shown == 0 && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  (nothing for sale right now)\r\n");
    else if ((size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "\r\n(buy <#> or buy <name>)\r\n");
    descriptor_page_start(d, out, 0);
    return true;
}

/* `buy <item>` / `buy <#>`: purchase an item from the shop's
 * `shopproducing` catalog (see cmd_list()'s comment) -- spawns a fresh
 * instance (obj_create_from_proto()) rather than moving a pre-existing
 * one, since the catalog is an infinite "this shop always sells these"
 * list, not a finite pool. A purely-numeric argument is the item's
 * `list` position (1-based, same order shop_repo_producing() always
 * returns -- see cmd_list()'s comment); anything else matches by keyword
 * as before. */
/* SPEC_TICKET_GUY (spec_mobs.cc's `TicketGuy`) -- id 51, one of the
 * "wrongly marked [-] before the array-position correction" finds
 * (SPEC_PROCS.md correction #2): no named constant in spec_mobs.h, but
 * a real slot in `mob_specials[]`. `buy ticket` from a matching mob
 * sells a one-way trip to a fixed destination room for a flat price --
 * ported here (rather than mob_ai.c, alongside the pulse-hook procs)
 * because it needs the `CMD_BUY` hook, which only cmd_buy() has; not a
 * pulse proc, so it's checked BEFORE the normal `find_active_shop()`
 * gate below (a ticket-guy mob has no real shop catalog, upstream's own
 * `TicketGuy()` runs standalone off `CMD_BUY` too). Upstream's
 * destination (`Room::TICKET_DESTINATION = 6969`) maps onto a real
 * imported Tobin room ("The Arrivals Circle") -- verified before
 * porting, unlike several other zone-hardcoded procs already logged
 * `[B]` in SPEC_PROCS.md. */
#define SPEC_TICKET_GUY 51
#define TICKET_PRICE 1000
#define TICKET_DESTINATION_VNUM 6969

static being_t *find_ticket_guy(const room_t *room) {
    for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *m = (being_t *)t;
        if (m->mob_spec_proc == SPEC_TICKET_GUY)
            return m;
    }
    return NULL;
}

static bool cmd_buy_ticket(descriptor_t *d, being_t *ch, being_t *keeper) {
    char keeper_name[128];
    being_display_name_cap(keeper, keeper_name, sizeof(keeper_name));

    if (ch->position != POSITION_STANDING) {
        char msg[288];
        snprintf(msg, sizeof(msg), "%s tells you, \"I won't sell you a ticket unless you stand on your own feet.\"\r\n", keeper_name);
        descriptor_send(d, msg);
        return true;
    }
    if (ch->progress.gold < TICKET_PRICE) {
        char msg[224];
        snprintf(msg, sizeof(msg), "%s tells you, \"Tickets cost %d talens.\"\r\n", keeper_name, TICKET_PRICE);
        descriptor_send(d, msg);
        return true;
    }

    ch->progress.gold -= TICKET_PRICE;
    descriptor_send(d, "You buy a ticket.\r\nThe mage makes a strange gesture before you.\r\n      *BLICK*\r\nSuddenly you find yourself in another plane of existence.\r\n");

    char ch_name_cap[128];
    being_display_name_cap(ch, ch_name_cap, sizeof(ch_name_cap));

    if (ch->base.roomp) {
        char room_msg[384];
        snprintf(room_msg, sizeof(room_msg), "%s purchases a ticket and %s transports %s into another plane.\r\n",
                 ch_name_cap, keeper_name, being_display_name(ch));
        descriptor_room_echo(ch->base.roomp, ch, room_msg);
    }

    room_t *dest = room_repo_load(TICKET_DESTINATION_VNUM);
    if (dest) {
        thing_set_room(&ch->base, dest);
        if (ch->base.roomp) {
            char arrive_msg[160];
            snprintf(arrive_msg, sizeof(arrive_msg), "%s blicks into the room.\r\n", ch_name_cap);
            descriptor_room_echo(ch->base.roomp, ch, arrive_msg);
        }
        cmd_dispatch(d, "look");
    }
    return true;
}

bool cmd_buy(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Buy what? Usage: buy <item> | buy <#>\r\n");
        return true;
    }

    being_t *ticket_guy = find_ticket_guy(ch->base.roomp);
    if (ticket_guy && strcasecmp(args, "ticket") == 0)
        return cmd_buy_ticket(d, ch, ticket_guy);

    shop_t shop;
    being_t *keeper = find_active_shop(ch->base.roomp, &shop);
    if (!keeper) {
        descriptor_send(d, "You don't see a shop here.\r\n");
        return true;
    }
    if (shop_repo_is_hospital(shop.shop_nr)) {
        buy_hospital_cure(d, ch, keeper, &shop, args);
        return true;
    }
    if (shop_repo_is_stable(shop.shop_nr)) {
        buy_stable_horse(d, ch, keeper, &shop, args);
        return true;
    }

    int vnums[SHOP_PRODUCING_MAX];
    int count = 0;
    shop_repo_producing(shop.shop_nr, vnums, SHOP_PRODUCING_MAX, &count);

    char tok[64] = "";
    sscanf(args, "%63s", tok);
    bool all_digits = tok[0] != '\0';
    for (const char *p = tok; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            all_digits = false;
            break;
        }
    }

    obj_proto_t proto;
    int matched_vnum = -1;
    obj_t *resale = NULL;
    if (all_digits) {
        int idx = atoi(tok);
        if (idx >= 1 && idx <= count && obj_proto_load(vnums[idx - 1], &proto))
            matched_vnum = vnums[idx - 1];
        else if (idx > count)
            resale = find_resale_item_by_index(keeper, idx - count);
    } else {
        for (int i = 0; i < count; i++) {
            if (obj_proto_load(vnums[i], &proto) && keyword_matches(proto.name, args)) {
                matched_vnum = vnums[i];
                break;
            }
        }
        if (matched_vnum < 0)
            resale = find_resale_item_by_keyword(keeper, tok);
    }
    if (matched_vnum < 0 && !resale) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop.no_such_item1);
        descriptor_send(d, msg);
        return true;
    }

    /* Material property system (Sneezy → Tobin feature audit): a
     * higher-tier material raises an item's shop price, the one
     * dimension the real upstream genuinely does this way too
     * (obj_base_weapon.cc's price += weight * material.price). */
    /* Resale stock uses the SAME formula against its own already-set
     * `.price`/`.material` (see the resale-stock doc comment above). */
    int price = resale
        ? (int)(resale->price * shop.profit_buy
                * material_tier_value_mult(material_tier_for_id(resale->material)))
        : (int)(proto.price * shop.profit_buy
                * material_tier_value_mult(material_tier_for_id(proto.material)));
    /* Sales tax (Money system v2, Sneezy → Tobin feature audit). The real
     * upstream's chargeTax() only taxes player-OWNED shop transactions,
     * routed to a per-shop tax office and journalized in double-entry --
     * Tobin has no player-shop economy, so this is a flat surcharge on
     * every ordinary `buy` instead, collected into the single global
     * treasury (see tobin_migrations.sql and cmd_bank.c's `treasury`
     * command). Hospital/stable purchases branch out above this point
     * and are deliberately untaxed. */
    int tax = (int)(price * SALES_TAX_RATE);
    int total = price + tax;
    if (ch->progress.gold < total) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop.missing_cash1);
        descriptor_send(d, msg);
        return true;
    }

    obj_t *bought = resale;
    if (!bought)
        bought = obj_create_from_proto(matched_vnum);
    if (!bought) {
        descriptor_send(d, "Something went wrong -- that item couldn't be created.\r\n");
        return true;
    }
    thing_move_to(&bought->base, &ch->base);

    char capbuf[128];
    const char *fallback_name = resale ? bought->base.name : proto.name;
    const char *fallback_short = resale ? bought->base.short_descr : proto.short_descr;
    const char *label = cap_first(fallback_short[0] ? fallback_short : fallback_name,
                                  capbuf, sizeof(capbuf));
    char confirm[OBJ_LONG_DESCR_LEN + 32];
    snprintf(confirm, sizeof(confirm), "You buy %s.\r\n", label);
    descriptor_send(d, confirm);

    char paid[SHOP_MSG_LEN + 16];
    snprintf(paid, sizeof(paid), shop.message_buy, price);
    strncat(paid, "\r\n", sizeof(paid) - strlen(paid) - 1);
    descriptor_send(d, paid);

    if (tax > 0) {
        char taxmsg[96];
        snprintf(taxmsg, sizeof(taxmsg), "A sales tax of %d gold is added to the crown's coffers.\r\n", tax);
        descriptor_send(d, taxmsg);
        treasury_repo_add_gold(tax);
    }

    ch->progress.gold -= total;
    player_progress_save(ch->player_id, &ch->progress);
    player_inventory_save(ch->player_id, ch);
    return true;
}

/* `sell <item>`: sells a loose carried item to the active shop, if it
 * deals in that item's category (shop_repo_buys_category(), checked
 * against the seeded `shoptype` rows). The item moves into the keeper's
 * OWN inventory instead of being destroyed (user, 2026-08-04: "the shop
 * should attempt to resell the item at a profit" -- see the resale-stock
 * doc comment above `find_active_shop()`'s own helpers, and `list`/`buy`
 * for the other half of this). A flat sales tax (same rate/treasury
 * destination as `buy`'s, user: "tax should be charged to people selling
 * at shops") is deducted from what the seller receives. */
bool cmd_sell(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Sell what? Usage: sell <item>\r\n");
        return true;
    }

    shop_t shop;
    being_t *keeper = find_active_shop(ch->base.roomp, &shop);
    if (!keeper) {
        descriptor_send(d, "You don't see a shop here.\r\n");
        return true;
    }

    /* Ordinal support (user 2026-07-18: "make it true as part of
     * everything that can exist") -- "sell 2.sword" picks the second
     * matching loose carried item, e.g. when you're holding two of the
     * same thing. */
    const char *sell_tok;
    int sell_ordinal = thing_parse_ordinal(args, &sell_tok);

    obj_t *found = NULL;
    int sell_seen = 0;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        if (ch->held[0] == (obj_t *)t || ch->held[1] == (obj_t *)t)
            continue;
        bool worn = false;
        for (int i = 0; i < LIMB_COUNT && !worn; i++)
            if (ch->equipment[i] == (obj_t *)t)
                worn = true;
        if (worn)
            continue;
        if (keyword_matches(t->name, sell_tok)) {
            sell_seen++;
            if (sell_seen == sell_ordinal) {
                found = (obj_t *)t;
                break;
            }
        }
    }
    if (!found) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop.no_such_item2);
        descriptor_send(d, msg);
        return true;
    }

    if (!shop_repo_buys_category(shop.shop_nr, (int)found->category)) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop.do_not_buy);
        descriptor_send(d, msg);
        return true;
    }

    /* Material property system: same value multiplier as buy, applied to
     * whatever the item is actually worth on sale. */
    int price = (int)(found->price * shop.profit_sell
                       * material_tier_value_mult(material_tier_for_id(found->material)));
    if (price < 0)
        price = 0;
    int tax = (int)(price * SALES_TAX_RATE);
    int net = price - tax;
    if (net < 0)
        net = 0;

    char capbuf[128];
    const char *label = cap_first(found->base.short_descr[0] ? found->base.short_descr : found->base.name,
                                  capbuf, sizeof(capbuf));
    char confirm[OBJ_LONG_DESCR_LEN + 32];
    snprintf(confirm, sizeof(confirm), "You sell %s.\r\n", label);
    descriptor_send(d, confirm);

    char paid[SHOP_MSG_LEN + 16];
    snprintf(paid, sizeof(paid), shop.message_sell, net);
    strncat(paid, "\r\n", sizeof(paid) - strlen(paid) - 1);
    descriptor_send(d, paid);

    if (tax > 0) {
        char taxmsg[96];
        snprintf(taxmsg, sizeof(taxmsg), "A sales tax of %d gold is added to the crown's coffers.\r\n", tax);
        descriptor_send(d, taxmsg);
        treasury_repo_add_gold(tax);
    }

    ch->progress.gold += net;

    /* Resale: the item joins the keeper's own inventory instead of being
     * destroyed, evicting the oldest resale item first if the keeper is
     * already at SHOP_RESALE_MAX (see the doc comment above). */
    if (count_resale_items(keeper) >= SHOP_RESALE_MAX) {
        obj_t *oldest = find_resale_item_by_index(keeper, 1);
        if (oldest)
            obj_destroy(oldest);
    }
    thing_move_to(&found->base, &keeper->base);

    player_progress_save(ch->player_id, &ch->progress);
    player_inventory_save(ch->player_id, ch);
    return true;
}
