/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"
#include "being.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `forage` (Crafting & extraction, Sneezy -> Tobin feature audit) --
 * gathers a bit of wild food from the terrain, no target needed. Reuses
 * room_can_plant()'s outdoor/not-water/not-indoor gate rather than the
 * original's own slightly different valid-sector list (also excludes
 * cities) -- a disclosed, small divergence, not worth a second nearly-
 * identical room-check function. Cooldown via AFFECT_FORAGE_COOLDOWN
 * (affect.h) instead of a dedicated timestamp field, reusing the
 * existing buff/debuff expiry machinery. */
bool cmd_forage(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "forage")) {
        descriptor_send(d, "You don't know how to forage for food.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return true;
    }
    if (!room_can_plant(ch->base.roomp)) {
        descriptor_send(d, "There's nothing to forage here.\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_FORAGE_COOLDOWN)) {
        descriptor_send(d, "You've picked this area over recently -- give it a while.\r\n");
        return true;
    }

    being_apply_affect(ch, AFFECT_FORAGE_COOLDOWN, FORAGE_COOLDOWN_ROUNDS);

    const skill_def_t *sk = skill_find(CLASS_DRUID, "forage", false);
    int pct = sk ? skill_learn_from_doing(ch, sk) : 0;
    if (!skill_roll_success(pct)) {
        descriptor_send(d, "You search around but come up empty-handed.\r\n");
        return true;
    }

    obj_t *food = obj_create_ephemeral("wild berries", "a handful of wild berries",
                                        "A handful of wild berries lies here.", OBJ_CAT_FOOD);
    if (!food)
        return true;
    food->val[0] = 5;
    food->val[1] = 5;
    thing_move_to(&food->base, &ch->base.roomp->base);

    descriptor_send(d, "You forage around and find a handful of wild berries.\r\n");
    char cap[128], msg[192];
    being_display_name_cap(ch, cap, sizeof(cap));
    snprintf(msg, sizeof(msg), "%s forages around for food.\r\n", cap);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
