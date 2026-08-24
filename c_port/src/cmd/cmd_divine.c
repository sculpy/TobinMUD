/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>

#include "affect.h"
#include "being.h"
#include "liquids.h"
#include "obj.h"
#include "room.h"
#include "skill.h"

/* `divine` (missing-skill audit, generic/cross-class, Tier-3 port
 * 2026-08-16): real upstream (disc_advanced_adventuring.cc's TDrinkCon::
 * divineMe()) is water-dowsing -- outdoors in nature, a diviner draws
 * drinkable water out of the ground into a held drink container, the yield
 * scaling with skill/level, on a recast timer (SKILL_DIVINATION). (Despite
 * the name it is NOT fortune-telling, and is distinct from the Mage's
 * `cast divination` spell.) Ported faithfully: `divine <drink container>`,
 * gated to natural outdoor terrain (reusing room_can_plant(), the same
 * outdoors/not-water/not-indoor test forage/encamp use, rather than
 * re-listing upstream's per-sector table -- a small disclosed divergence),
 * behind a per-being cooldown (AFFECT_DIVINE_COOLDOWN) standing in for the
 * upstream recast affect. A proficiency roll conjures water into the
 * container. Following Tobin's own `fill` convention, a container already
 * holding something other than water must be emptied first (upstream's
 * "turns it to slime" gag is swapped for this cleaner, house-consistent
 * refusal -- disclosed). */
bool cmd_divine(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "divine")) {
        descriptor_send(d, "You don't know how to divine for water.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "The ensuing battle makes it too hard to divine for water.\r\n");
        return true;
    }
    if (ch->position < POSITION_STANDING) {
        descriptor_send(d, "You need to be standing in order to divine for water.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Syntax: divine <drink container>\r\n");
        return true;
    }
    obj_t *container = liquid_find_carried_container(ch, raw);
    if (!container) {
        descriptor_send(d, "You don't have that drink container.\r\n");
        return true;
    }

    if (!room_can_plant(ch->base.roomp)) {
        descriptor_send(d, "You need to be out in nature to divine for water.\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_DIVINE_COOLDOWN)) {
        descriptor_send(d, "You are not ready to divine again so soon.\r\n");
        return true;
    }
    if (container->val[1] >= container->val[0]) {
        descriptor_send(d, "That is already completely full!\r\n");
        return true;
    }
    if (container->val[1] > 0 && container->val[2] != LIQUID_TYPE_DEFAULT) {
        descriptor_send(d, "You'd divine water, but you can't mix it with what's already in there -- pour it out first.\r\n");
        return true;
    }

    being_apply_affect(ch, AFFECT_DIVINE_COOLDOWN, DIVINE_COOLDOWN_ROUNDS);

    const skill_def_t *sk = skill_find(ch->char_class, "divine",
                                       being_is_immortal(ch));
    int pct = being_is_immortal(ch) ? 100
              : (sk ? skill_learn_from_doing(ch, sk) : 0);

    descriptor_send(d, "You divine for water, feeling for it beneath your feet.\r\n");

    if (!skill_roll_success(pct)) {
        descriptor_send(d, "You search, but find no water to draw here.\r\n");
        return true;
    }

    /* Yield scales with proficiency; capped at the container's remaining
     * room -- a strong diviner can top a small skin off outright. */
    int remaining = container->val[0] - container->val[1];
    int drawn = 3 + pct / 20;
    if (drawn > remaining)
        drawn = remaining;

    container->val[2] = LIQUID_TYPE_DEFAULT; /* water */
    container->val[1] += drawn;

    const char *label = container->base.short_descr[0]
                            ? container->base.short_descr : container->base.name;
    char msg[320];
    snprintf(msg, sizeof(msg), "You divine %d ounce%s of clear water into %s.\r\n",
             drawn, drawn == 1 ? "" : "s", label);
    descriptor_send(d, msg);
    if (container->val[1] >= container->val[0]) {
        snprintf(msg, sizeof(msg), "%s is filled.\r\n", label);
        msg[0] = (char)toupper((unsigned char)msg[0]);
        descriptor_send(d, msg);
    }

    char cap[128], rmsg[384];
    being_display_name_cap(ch, cap, sizeof(cap));
    snprintf(rmsg, sizeof(rmsg), "%s divines for water, drawing it into %s.\r\n", cap, label);
    descriptor_room_echo(ch->base.roomp, ch, rmsg);
    return true;
}
