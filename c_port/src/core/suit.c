/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "suit.h"

#include "being.h"
#include "obj.h"
#include "obj_repo.h"
#include "suit_repo.h"
#include "thing.h"

/* Instantiates every item in suit `suit_id` and drops them loose into
 * `ch`'s inventory (not auto-equipped), saving the inventory afterward.
 * Returns the count actually granted (protos that fail to create are
 * silently skipped). Used for gearing a character up in one shot, e.g. a
 * starting kit or an immortal reward suit. */
int suit_grant(being_t *ch, int suit_id) {
    if (!ch || suit_id < 0)
        return 0;

    int vnums[SUIT_MAX_ITEMS];
    int qtys[SUIT_MAX_ITEMS];
    int n = suit_repo_load_items_qty(suit_id, vnums, qtys, SUIT_MAX_ITEMS);

    int granted = 0;
    for (int i = 0; i < n; i++) {
        /* Per-wear-location quantities (Menu-driven loadsuit editor,
         * TODO.md priority item, 2026-08-02) -- a suit_item row's
         * quantity can be >1 (two wrist bands, two boots, ...), so each
         * one is instantiated as its OWN separate obj_t, same as loading
         * the same vnum twice by hand would produce. */
        for (int q = 0; q < qtys[i]; q++) {
            obj_t *o = obj_create_from_proto(vnums[i]);
            if (!o)
                continue;
            /* Loose in inventory, not auto-equipped -- user 2026-07-26:
             * "they can hold the items themselves, just load into
             * inventory." Same landing spot `get`/`load` already use. */
            thing_move_to(&o->base, &ch->base);
            granted++;
        }
    }

    if (granted > 0)
        player_inventory_save(ch->player_id, ch);
    return granted;
}
