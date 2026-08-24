/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SHOP_REPO_H
#define TOBIN_SHOP_REPO_H

#include <stdbool.h>

/* DB access for the `shop`/`shoptype` tables -- real seeded data from the
 * original SneezyMUD import (264 shops, each with its own keeper
 * mob/room/prices/flavor text; `shoptype` lists which raw itemTypeT
 * categories each one buys). Tobin's shop model is deliberately simpler
 * than the original's: no temper/haggling, no open/close hours
 * enforcement yet, GOLD-COIN-ONLY (message_buy/message_sell were
 * originally "%d talens" -- bulk-updated to "%d gold" to match, see
 * TODO.md). See cmd_shop.c for the `list`/`buy`/`sell` commands that
 * consume this, and sedit (descriptor.c) for the menu-driven editor that
 * writes it -- mostly read-only otherwise, since shops are seed data. */

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
    bool is_stable;  /* see shop_repo_is_stable() */
    bool is_repair;  /* see shop_repo_is_repair() */
    bool is_bank;    /* see shop_repo_is_bank() */
} shop_t;

/* Finds the shop (if any) operating out of room `room_vnum`. Returns false
 * (out untouched) if no shop.in_room matches. Includes is_stable/is_repair/
 * is_bank (added for sedit's working copy, see descriptor_edshop_begin()) --
 * existing callers that only cared about pricing/messages are unaffected. */
bool shop_repo_find_by_room(int room_vnum, shop_t *out);

/* Persists every sedit-editable column (pricing, keeper, room, the six
 * canned messages, and the stable/repair/bank flags) for shop `s->shop_nr`
 * back to `shop`. Shops are seed data, created via the original import --
 * this is a plain UPDATE, not an upsert, same precedent as
 * zone_repo_save(). */
bool shop_repo_save(const shop_t *s);

#define SHOP_TYPE_MAX 32

/* Raw itemTypeT values (see obj_type_name()) shop `shop_nr` currently
 * accepts, `type`-ascending, for sedit's "Accepted item types" submenu.
 * Returns the count loaded (capped at max). */
int shop_repo_shoptype_list(int shop_nr, int *out, int max);

/* Adds one accepted raw item type to a shop -- INSERT IGNORE, so re-adding
 * an already-accepted type (composite PK shop_nr+type) is a harmless
 * no-op rather than a DB error. */
bool shop_repo_shoptype_add(int shop_nr, int raw_type);

/* Removes one accepted raw item type from a shop. A no-op if it wasn't
 * accepted to begin with. */
bool shop_repo_shoptype_remove(int shop_nr, int raw_type);

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

/* The original engine's "doctor" spec-proc id (misc/hospital.cc) -- seeded
 * verbatim into mob.spec_proc for every real hospital keeper in the
 * import (6 of them: Tobin City, Amber, Logrus, Brightmoon, a field
 * medic, and Xanesla). A data-driven signal, not a hardcoded room/shop
 * vnum list the way the original's own getDoctor() was -- see
 * shop_repo_is_hospital(). */
#define SPEC_PROC_DOCTOR 48

/* True if shop `shop_nr`'s keeper mob is a "doctor" (Hospital, TODO.md:
 * "add hospital code" -- limb repair + disease cures, cmd_shop.c). A
 * hospital shop's own `shopproducing` is empty (nothing physical to
 * sell) -- `list`/`buy` special-case this instead, listing/curing
 * ailments in its place. */
bool shop_repo_is_hospital(int shop_nr);

/* True if shop `shop_nr` is a stable (Mount/riding system, TODO.md).
 * Unlike SPEC_PROC_DOCTOR, there's no real Sneezy-seeded spec_proc to
 * mine here -- the original's own SPEC_PROC_STABLE_MAN
 * (spec/spec_mobs.h) was verified dead/vestigial, never actually
 * assigned to any real mob. Backed by a genuinely new, Tobin-original
 * `shop.is_stable` column instead of overloading `spec_proc` with a
 * fabricated value that could collide with real (currently-unread)
 * seeded data -- same "new column when there's nothing real to reuse"
 * precedent as mob.align. Seeded true for shop_nr 164 (Petir's
 * "Carnivorous Companions", user-selected 2026-07-19) -- previously a
 * non-functional real shop (empty `shopproducing`, framed in its own
 * room description as "buy a trained familiar," which fits a horse
 * closely enough for now and doubles as the future Pet/charm shop). A
 * stable's own `shopproducing` stays empty -- `list`/`buy` special-case
 * this instead, same shape as shop_repo_is_hospital(). */
bool shop_repo_is_stable(int shop_nr);

/* True if shop `shop_nr` is a repair shop (Object maintenance tasks 3-4,
 * Sneezy → Tobin feature audit). Same genuinely-new-column precedent as
 * `is_stable` above. Seeded true for shop_nr 134 ("Blacksmith's Forge",
 * room 7110) -- a real seeded shop, thematically exact. Unlike the
 * stable/hospital special-cases, a repair shop's own `shopproducing`
 * doesn't need special-casing at all -- it keeps buying/selling weapons
 * and armor normally via `list`/`buy`/`sell`; repair adds two BRAND NEW
 * commands instead (`submit`/`retrieve`, cmd_repair.c) that check this
 * flag themselves rather than hooking into the existing shop commands. */
bool shop_repo_is_repair(int shop_nr);

/* True if shop `shop_nr` is a bank (Money system v2, Sneezy → Tobin
 * feature audit). Same genuinely-new-column precedent as `is_stable`/
 * `is_repair` above. Seeded true for shop_nr 4 ("Grimhaven First Kingdom
 * Bank", room 31750) -- the real seeded shop this bank picked as Tobin's
 * single global bank. `deposit`/`withdraw`/`bank balance` (cmd_bank.c)
 * check this flag rather than special-casing inside `buy`/`sell`. */
bool shop_repo_is_bank(int shop_nr);

#endif
