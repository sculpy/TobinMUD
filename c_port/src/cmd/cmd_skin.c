/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "extraction.h"
#include "log.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `skin`/`butcher` (Crafting & extraction, Sneezy -> Tobin feature audit)
 * -- see extraction.h's doc comment for the full scope-cut disclosure.
 * Resolves instantly (no multi-pulse task, unlike the original) against a
 * mob corpse in the room; once-only per corpse (val[3] flag bits), no
 * partial/half-yield tier. */

/* A corpse's own keyword is always literally "corpse" (combat.c) -- any
 * player-typed token is accepted as long as SOME corpse is present, same
 * "skin corpse"/"skin" both work" leniency real MUD players expect. */
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

/* `skin` command -- see file-top comment for the full scope. Requires
 * the "skin" skill and an un-skinned mob corpse, rolls the skill
 * (consuming the corpse's CORPSE_SKINNED flag either way), and on
 * success drops one weight-scaled hide pelt. */
bool cmd_skin(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "skin")) {
        descriptor_send(d, "You don't know how to skin anything.\r\n");
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
        descriptor_send(d, "You can't bring yourself to skin that.\r\n");
        return true;
    }
    if (corpse->val[3] & CORPSE_SKINNED) {
        descriptor_send(d, "This corpse has already been skinned.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "skin", being_is_immortal(ch));
    int pct = sk ? skill_learn_from_doing(ch, sk) : 0;
    corpse->val[3] |= CORPSE_SKINNED;

    if (!skill_roll_success(pct)) {
        descriptor_send(d, "You make a mess of it -- the hide is ruined.\r\n");
        return true;
    }

    /* Weight-scaled yield, same "corpse weight -> material amount" spirit
     * as the original's real formula, simplified to one generic hide
     * item (not a per-race table -- see extraction.h). */
    double weight = corpse->weight / 10.0;
    if (weight < 1.0)
        weight = 1.0;
    obj_t *hide = obj_create_ephemeral("hide pelt", "a rough hide",
                                        "A rough hide lies here.", OBJ_CAT_OTHER);
    if (!hide)
        return true;
    hide->weight = weight;
    thing_move_to(&hide->base, &ch->base.roomp->base);

    descriptor_send(d, "You carefully skin the corpse, leaving a hide behind.\r\n");
    char cap[128], msg[192];
    being_display_name_cap(ch, cap, sizeof(cap));
    snprintf(msg, sizeof(msg), "%s skins a corpse.\r\n", cap);
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
