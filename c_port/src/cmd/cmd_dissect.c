/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
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

/* `dissect` (missing-skill audit, generic/cross-class, 2026-08-10): real
 * upstream SKILL_DISSECT (cmd_dissect.cc) carves spell components out of a
 * mob corpse, keyed by a per-vnum/per-race `dissect_array` table mapping
 * each creature to the exact component it yields. Tobin has no such
 * per-mob component table, so -- exactly the same disclosed scope-cut
 * cmd_skin.c/cmd_butcher.c already made ("one generic yield item, not a
 * per-race table") -- this yields one generic anatomical reagent instead.
 * Same instant-resolve, once-per-corpse (CORPSE_DISSECTED flag) shape as
 * skin/butcher. */

/* A corpse's own keyword is always literally "corpse" (combat.c). */
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

bool cmd_dissect(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "dissect")) {
        descriptor_send(d, "You don't know how to dissect anything.\r\n");
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
        descriptor_send(d, "You can't bring yourself to dissect that.\r\n");
        return true;
    }
    if (corpse->val[3] & CORPSE_DISSECTED) {
        descriptor_send(d, "This corpse has already been dissected.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "dissect", imm);
    int pct = imm ? 100 : (sk ? skill_learn_from_doing(ch, sk) : 0);
    corpse->val[3] |= CORPSE_DISSECTED;

    if (!skill_roll_success(pct)) {
        descriptor_send(d, "Your blade slips -- whatever you were after is ruined.\r\n");
        return true;
    }

    obj_t *reagent = obj_create_ephemeral("reagent gland organ",
                                          "a glistening reagent",
                                          "A glistening reagent lies here.",
                                          OBJ_CAT_OTHER);
    if (!reagent)
        return true;
    thing_move_to(&reagent->base, &ch->base.roomp->base);

    descriptor_send(d, "You carefully dissect the corpse, extracting a usable reagent.\r\n");
    if (ch->base.roomp) {
        char cap[128], msg[192];
        being_display_name_cap(ch, cap, sizeof(cap));
        snprintf(msg, sizeof(msg), "%s carefully dissects a corpse.\r\n", cap);
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}
