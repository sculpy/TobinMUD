/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* `sedit` / `edit shop`: Administrator (58+) only -- a menu-driven editor
 * for the shop operating out of the immortal's OWN current room: pricing
 * multipliers, keeper/room vnums, the six canned buy/sell/refusal
 * messages, the stable/repair/bank flags, and the accepted-item-types
 * (shoptype) list. No target argument -- unlike edroom/edzone/pedit,
 * there's no separate room/zone/name to type, so an immortal just walks
 * into the shop's room first, same as how the shop itself is found at
 * runtime (shop_repo_find_by_room(), cmd_shop.c). The whole editor lives
 * in descriptor.c's CONN_EDSHOP_* state machine (see
 * descriptor_edshop_begin), mirroring edzone. */
bool cmd_sedit(descriptor_t *d, const char *args) {
    (void)args;
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    if (!descriptor_edshop_begin(d, d->character->base.roomp->vnum)) {
        descriptor_send(d, "There is no shop here to edit.\r\n");
    }
    return true;
}
