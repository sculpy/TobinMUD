/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "extraction.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* See cmd_skin.c's doc comment -- same mechanic, different skill/yield.
 * `find_corpse()` duplicated locally rather than shared, same per-file
 * helper precedent as cmd_object.c's obj_name_matches(). */
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

bool cmd_butcher(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "butcher")) {
        descriptor_send(d, "You don't know how to butcher anything.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return true;
    }

    obj_t *corpse = find_corpse(ch);
    if (!corpse) {
        descriptor_send(d, "You don't see a corpse like that here.\r\n");
        return true;
    }
    if (corpse->raw_type != CORPSE_KIND_MOB) {
        descriptor_send(d, "You can't bring yourself to butcher that.\r\n");
        return true;
    }
    if (corpse->val[3] & CORPSE_BUTCHERED) {
        descriptor_send(d, "This corpse has already been butchered.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(CLASS_DRUID, "butcher", false);
    int pct = sk ? skill_learn_from_doing(ch, sk) : 0;
    corpse->val[3] |= CORPSE_BUTCHERED;

    if (!skill_roll_success(pct)) {
        descriptor_send(d, "You botch the job -- the meat is spoiled beyond use.\r\n");
        return true;
    }

    /* FOOD category convention (obj.h): val[0]=max units, val[1]=current
     * units -- weight-scaled, same spirit as skin's hide yield. */
    int units = (int)(corpse->weight / 5.0);
    if (units < 1)
        units = 1;
    if (units > 20)
        units = 20;
    obj_t *meat = obj_create_ephemeral("steak meat", "a raw steak",
                                        "A raw steak lies here.", OBJ_CAT_FOOD);
    if (!meat)
        return true;
    meat->val[0] = units;
    meat->val[1] = units;
    thing_move_to(&meat->base, &ch->base.roomp->base);

    descriptor_send(d, "You carve a steak from the corpse.\r\n");
    char cap[128], msg[192];
    being_display_name_cap(ch, cap, sizeof(cap));
    snprintf(msg, sizeof(msg), "%s butchers a corpse.\r\n", cap);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
