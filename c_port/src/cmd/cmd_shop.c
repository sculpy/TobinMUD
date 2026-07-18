/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "obj.h"
#include "obj_repo.h"
#include "shop_repo.h"

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

/* `list` (user 2026-07-17: "implement money and shops"): shows the active
 * shop's wares from its `shopproducing` catalog (see shop_repo.h -- NOT
 * the keeper mob's own carried items; the seeded zone-reset data never
 * actually stocks these keepers that way), priced at each item's own
 * prototype `price` times the shop's `profit_buy` multiplier. Every
 * listed item is always available -- a shop never "runs out" of what it
 * produces. */
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

    int vnums[SHOP_PRODUCING_MAX];
    int count = 0;
    shop_repo_producing(shop.shop_nr, vnums, SHOP_PRODUCING_MAX, &count);

    char out[4096];
    int n = snprintf(out, sizeof(out), "\r\n%s offers:\r\n", being_display_name(keeper));
    int shown = 0;
    for (int i = 0; i < count && (size_t)n < sizeof(out); i++) {
        obj_proto_t proto;
        if (!obj_proto_load(vnums[i], &proto))
            continue;
        int price = (int)(proto.price * shop.profit_buy);
        char capbuf[128];
        const char *label = proto.short_descr[0] ? proto.short_descr : proto.name;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-42s %d gold\r\n",
                      cap_first(label, capbuf, sizeof(capbuf)), price);
        shown++;
    }
    if (shown == 0 && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  (nothing for sale right now)\r\n");
    descriptor_page_start(d, out, 0);
    return true;
}

/* `buy <item>`: purchase an item from the shop's `shopproducing` catalog
 * (see cmd_list()'s comment) -- spawns a fresh instance
 * (obj_create_from_proto()) rather than moving a pre-existing one, since
 * the catalog is an infinite "this shop always sells these" list, not a
 * finite pool. */
bool cmd_buy(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Buy what? Usage: buy <item>\r\n");
        return true;
    }

    shop_t shop;
    being_t *keeper = find_active_shop(ch->base.roomp, &shop);
    if (!keeper) {
        descriptor_send(d, "You don't see a shop here.\r\n");
        return true;
    }

    int vnums[SHOP_PRODUCING_MAX];
    int count = 0;
    shop_repo_producing(shop.shop_nr, vnums, SHOP_PRODUCING_MAX, &count);

    obj_proto_t proto;
    int matched_vnum = -1;
    for (int i = 0; i < count; i++) {
        if (obj_proto_load(vnums[i], &proto) && keyword_matches(proto.name, args)) {
            matched_vnum = vnums[i];
            break;
        }
    }
    if (matched_vnum < 0) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop.no_such_item1);
        descriptor_send(d, msg);
        return true;
    }

    int price = (int)(proto.price * shop.profit_buy);
    if (ch->progress.gold < price) {
        char msg[SHOP_MSG_LEN + 4];
        snprintf(msg, sizeof(msg), "%s\r\n", shop.missing_cash1);
        descriptor_send(d, msg);
        return true;
    }

    obj_t *bought = obj_create_from_proto(matched_vnum);
    if (!bought) {
        descriptor_send(d, "Something went wrong -- that item couldn't be created.\r\n");
        return true;
    }
    thing_move_to(&bought->base, &ch->base);

    char capbuf[128];
    const char *label = cap_first(proto.short_descr[0] ? proto.short_descr : proto.name,
                                  capbuf, sizeof(capbuf));
    char confirm[OBJ_LONG_DESCR_LEN + 32];
    snprintf(confirm, sizeof(confirm), "You buy %s.\r\n", label);
    descriptor_send(d, confirm);

    char paid[SHOP_MSG_LEN + 16];
    snprintf(paid, sizeof(paid), shop.message_buy, price);
    strncat(paid, "\r\n", sizeof(paid) - strlen(paid) - 1);
    descriptor_send(d, paid);

    ch->progress.gold -= price;
    player_progress_save(ch->player_id, &ch->progress);
    player_inventory_save(ch->player_id, ch);
    return true;
}

/* `sell <item>`: sells a loose carried item to the active shop, if it
 * deals in that item's category (shop_repo_buys_category(), checked
 * against the seeded `shoptype` rows) -- destroyed on sale rather than
 * added to the keeper's own stock, avoiding unbounded shopkeeper
 * inventory growth over a long server lifetime. */
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

    obj_t *found = NULL;
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
        if (keyword_matches(t->name, args)) {
            found = (obj_t *)t;
            break;
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

    int price = (int)(found->price * shop.profit_sell);
    if (price < 0)
        price = 0;

    char capbuf[128];
    const char *label = cap_first(found->base.short_descr[0] ? found->base.short_descr : found->base.name,
                                  capbuf, sizeof(capbuf));
    char confirm[OBJ_LONG_DESCR_LEN + 32];
    snprintf(confirm, sizeof(confirm), "You sell %s.\r\n", label);
    descriptor_send(d, confirm);

    char paid[SHOP_MSG_LEN + 16];
    snprintf(paid, sizeof(paid), shop.message_sell, price);
    strncat(paid, "\r\n", sizeof(paid) - strlen(paid) - 1);
    descriptor_send(d, paid);

    ch->progress.gold += price;
    obj_destroy(found);
    player_progress_save(ch->player_id, &ch->progress);
    player_inventory_save(ch->player_id, ch);
    return true;
}
