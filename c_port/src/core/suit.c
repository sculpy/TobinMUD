/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "suit.h"

#include "being.h"
#include "obj.h"
#include "obj_repo.h"
#include "suit_repo.h"
#include "thing.h"

int suit_grant(being_t *ch, int suit_id) {
    if (!ch || suit_id < 0)
        return 0;

    int vnums[SUIT_MAX_ITEMS];
    int n = suit_repo_load_items(suit_id, vnums, SUIT_MAX_ITEMS);

    int granted = 0;
    for (int i = 0; i < n; i++) {
        obj_t *o = obj_create_from_proto(vnums[i]);
        if (!o)
            continue;
        /* Loose in inventory, not auto-equipped -- user 2026-07-26:
         * "they can hold the items themselves, just load into
         * inventory." Same landing spot `get`/`load` already use. */
        thing_move_to(&o->base, &ch->base);
        granted++;
    }

    if (granted > 0)
        player_inventory_save(ch->player_id, ch);
    return granted;
}
