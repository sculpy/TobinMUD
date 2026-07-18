/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SHOP_REPO_H
#define TOBIN_SHOP_REPO_H

#include <stdbool.h>

/* Read-only DB access for the `shop`/`shoptype` tables -- real seeded data
 * from the original SneezyMUD import (264 shops, each with its own
 * keeper mob/room/prices/flavor text; `shoptype` lists which raw itemTypeT
 * categories each one buys). Tobin's shop model is deliberately simpler
 * than the original's: no temper/haggling, no open/close hours
 * enforcement yet, GOLD-COIN-ONLY (message_buy/message_sell were
 * originally "%d talens" -- bulk-updated to "%d gold" to match, see
 * TODO.md). See cmd_shop.c for the `list`/`buy`/`sell` commands that
 * consume this. */

#define SHOP_MSG_LEN 128

typedef struct {
    int shop_nr;
    double profit_buy;   /* multiplier applied to an item's base price when a player BUYS */
    double profit_sell;  /* multiplier applied when a player SELLS (always < profit_buy) */
    int keeper;           /* shopkeeper mob vnum (thing_t.id on the live being) */
    int in_room;           /* room vnum the shop operates in */
    char no_such_item1[SHOP_MSG_LEN]; /* buyer asks for something the shop doesn't stock right now */
    char no_such_item2[SHOP_MSG_LEN]; /* seller offers something the shop doesn't recognize at all */
    char do_not_buy[SHOP_MSG_LEN];    /* seller offers something the shop recognizes but won't buy (wrong category) */
    char missing_cash1[SHOP_MSG_LEN]; /* buyer can't afford it */
    char message_buy[SHOP_MSG_LEN];   /* success message, "%d" -> price paid */
    char message_sell[SHOP_MSG_LEN];  /* success message, "%d" -> price received */
} shop_t;

/* Finds the shop (if any) operating out of room `room_vnum`. Returns false
 * (out untouched) if no shop.in_room matches. */
bool shop_repo_find_by_room(int room_vnum, shop_t *out);

/* True if shop `shop_nr` deals in `category` (checked against every
 * shoptype row for that shop, each collapsed from the raw seeded
 * itemTypeT value via category_for_item_type(), same as any other raw
 * `obj.type` value elsewhere in the codebase). */
bool shop_repo_buys_category(int shop_nr, int category);

#define SHOP_PRODUCING_MAX 32

/* Fills `out` (capacity `max`) with the object vnums shop `shop_nr`
 * always has in stock (the seeded `shopproducing` table -- NOT the
 * keeper mob's own carried inventory, which the original zone-reset data
 * never actually populates for these shopkeepers; `shopproducing` is the
 * real "this shop manufactures/always sells these" catalog). Sets *count
 * to how many were found (0..max). A shop can always sell every vnum
 * here -- buying one spawns a fresh instance (obj_create_from_proto()),
 * it never "runs out". */
void shop_repo_producing(int shop_nr, int *out, int max, int *count);

#endif
