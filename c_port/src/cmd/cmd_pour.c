/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "liquids.h"
#include "log.h"
#include "obj.h"

/* `pour <container>` (Liquids, Sneezy -> Tobin feature audit, user
 * 2026-07-26: "pouring one out pools on the ground"). Ported from
 * TBaseCup::pourMeOut() (obj_base_cup.cc) -- empties a carried
 * OBJ_CAT_DRINK container onto the floor of the current room as a real
 * ground puddle (obj_grow_pool(), obj.c -- the exact same puddle
 * machinery `pee`/blood already use, so it grows an existing matching
 * puddle instead of spawning a separate one, and decays the same way).
 * Resets the container back to empty/plain-water afterward, same as the
 * original's genericEmpty(). */
bool cmd_pour(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Usage: pour <container>\r\n");
        return true;
    }

    obj_t *container = liquid_find_carried_container(ch, raw);
    if (!container) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }
    if (container->val[1] <= 0) {
        descriptor_send(d, "It's already empty.\r\n");
        return true;
    }

    char bare[64];
    liquid_bare_name(container->val[2], bare, sizeof(bare));
    char keywords[80];
    snprintf(keywords, sizeof(keywords), "puddle pool %s", bare);
    obj_grow_pool(ch->base.roomp, bare, keywords, bare);

    const char *label = container->base.short_descr[0] ? container->base.short_descr : container->base.name;
    char msg[320];
    snprintf(msg, sizeof(msg), "You empty %s onto the ground.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s empties %s onto the ground.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    game_log(LOG_SILENT, "%s poured out %s (vnum %d) in room %d",
             ch->base.name, label, container->vnum, ch->base.roomp->vnum);

    container->val[1] = 0;
    container->val[2] = LIQUID_TYPE_DEFAULT;

    return true;
}
