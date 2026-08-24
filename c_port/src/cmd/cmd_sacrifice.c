/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* Same corpse-finding helper as cmd_skin.c/cmd_butcher.c, duplicated
 * locally per that same precedent. */
static obj_t *find_corpse(const being_t *ch) {
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category == OBJ_CAT_CONTAINER && strcasecmp(t->name, "corpse") == 0)
            return o;
    }
    return NULL;
}

/* `sacrifice <corpse>` (Full spell/skill/prayer roster import, one of 6
 * named Shaman spells/skills ported onto Druid, user 2026-07-26). Real
 * upstream mechanic (task_sacrifice.cc) is a full multi-round timed
 * ritual: needs a "totem" tool item that wears down with use, a corpse
 * present the whole time, can be interrupted by room guards, and its
 * payoff/cost is "lifeforce" -- a resource Tobin has no equivalent of at
 * all. Scoped down to a single-action skill here instead, same shape as
 * `skin`/`butcher` (cmd_skin.c/cmd_butcher.c) -- no totem requirement, no
 * multi-round ritual, no lifeforce currency. "Lifeforce" gained/spent
 * maps onto Tobin's existing `vit` resource pool (progress.vit/max_vit,
 * being_heal_vit()) as the closest real analog already wired
 * everywhere vit is spent (movement, etc.) -- not a literal port of the
 * original's own separate stat. The corpse is fully consumed either way
 * (success or failure), matching the original's own "sacrifice" framing
 * (an offering, not a harvest you can fail and retry on the same body). */
bool cmd_sacrifice(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "sacrifice")) {
        descriptor_send(d, "You don't know how to perform that ritual.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return true;
    }

    obj_t *corpse = find_corpse(ch);
    if (!corpse) {
        descriptor_send(d, "You don't see a corpse here to sacrifice.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(CLASS_DRUID, "sacrifice", false);
    int pct = sk ? skill_learn_from_doing(ch, sk) : 0;
    obj_destroy(corpse);

    char cap[128], msg[192];
    being_display_name_cap(ch, cap, sizeof(cap));

    if (!skill_roll_success(pct)) {
        descriptor_send(d, "You chant over the corpse, but the loa ignore your offering.\r\n");
        snprintf(msg, sizeof(msg), "%s chants over a corpse, but nothing seems to happen.\r\n", cap);
        descriptor_room_echo(ch->base.roomp, ch, msg);
        return true;
    }

    /* Scaled by the caster's own level, same "modest, level-tracking
     * reward" spirit as other placeholder combat/growth numbers
     * elsewhere in this port (e.g. combat_defeat()'s XP-on-kill). */
    int amount = 5 + ch->progress.level / 4;
    being_heal_vit(ch, amount);

    char amsg[128];
    snprintf(amsg, sizeof(amsg), "You complete the ritual sacrifice -- the loa accept your offering! (+%d Move)\r\n", amount);
    descriptor_send(d, amsg);
    snprintf(msg, sizeof(msg), "%s completes a ritual sacrifice over a corpse.\r\n", cap);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
