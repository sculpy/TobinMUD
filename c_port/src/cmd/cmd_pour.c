/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <strings.h>

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

/* `pour <container> <container2>` (object manipulation audit continued,
 * 2026-07-29 -- a gap found comparing against real upstream's own
 * `doPour()`, which also supports container-to-container transfer, not
 * just dumping onto the ground). Both containers must be carried by the
 * caster (real upstream's own "pour onto a target being" variant, e.g.
 * dousing a burning victim, isn't ported -- Tobin has no fire/burning
 * status to douse). Mixing liquids is refused outright, same "pour it
 * out first" precedent cmd_fill.c's own mixing check already set, not a
 * new rule invented here. Transfers up to the destination's remaining
 * capacity; the source keeps whatever doesn't fit rather than spilling
 * it (an honest simplification -- real upstream drops the overflow on
 * the ground as a second puddle, not ported, no real player-facing
 * value for the extra complexity). */
static bool cmd_pour_transfer(descriptor_t *d, being_t *ch, obj_t *src, obj_t *dst) {
    if (dst->val[1] > 0 && dst->val[2] != src->val[2]) {
        char msg[256];
        snprintf(msg, sizeof(msg), "You can't mix %s with what's already in there -- pour it out first.\r\n",
                 liquid_info(src->val[2])->name);
        descriptor_send(d, msg);
        return true;
    }
    if (dst->val[1] >= dst->val[0]) {
        descriptor_send(d, "That is already completely full!\r\n");
        return true;
    }

    int room = dst->val[0] - dst->val[1];
    int moved = src->val[1] < room ? src->val[1] : room;
    dst->val[2] = src->val[2];
    dst->val[1] += moved;
    src->val[1] -= moved;
    if (src->val[1] <= 0)
        src->val[2] = LIQUID_TYPE_DEFAULT;

    const char *src_label = src->base.short_descr[0] ? src->base.short_descr : src->base.name;
    const char *dst_label = dst->base.short_descr[0] ? dst->base.short_descr : dst->base.name;
    char msg[640];
    snprintf(msg, sizeof(msg), "You pour %s from %s into %s.\r\n",
             liquid_info(dst->val[2])->name, src_label, dst_label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s pours %s from %s into %s.\r\n",
             ch->base.name, liquid_info(dst->val[2])->name, src_label, dst_label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    game_log(LOG_SILENT, "%s poured %s (vnum %d) into %s (vnum %d) in room %d",
             ch->base.name, src_label, src->vnum, dst_label, dst->vnum, ch->base.roomp->vnum);
    return true;
}

bool cmd_pour(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64], raw2[64];
    int n = sscanf(args, "%63s %63s", raw, raw2);
    if (n < 1) {
        descriptor_send(d, "Usage: pour <container> [<container2> | out]\r\n");
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

    if (n == 2 && strcasecmp(raw2, "out") != 0) {
        obj_t *dest = liquid_find_carried_container(ch, raw2);
        if (!dest) {
            descriptor_send(d, "You aren't carrying that.\r\n");
            return true;
        }
        if (dest == container) {
            descriptor_send(d, "You can't pour something into itself.\r\n");
            return true;
        }
        return cmd_pour_transfer(d, ch, container, dest);
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
