/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "being.h"
#include "cmd.h"
#include "fall.h"
#include "room.h"
#include "skill.h"

/* `catleap <direction>` (spell/skill functional-completeness audit
 * continued, Monk level 1: skill.c's own "Leap and glide a direction,
 * out of combat."). The real function (`TBeing::doLeap()`) wasn't in
 * the originally-bundled sneezymud-master/ source at all -- found in
 * the fuller peel-sneezymud reference clone's disc/disc_monk.cc:
 * grants brief flight, spends 15 Move, then attempts the real move;
 * on a failed skill roll you don't make it very far and just end up
 * sitting instead (`crashLanding()`). Ported with the same shape:
 * refuses while fighting, refuses if the CURRENT room has no floor
 * (`sector_is_fall()`, room.h -- same "no ground to leap off of" gate
 * the real source uses), spends 15 Vitality (Tobin's own Move-
 * equivalent resource), grants a brief AFFECT_FLYING (long enough to
 * clear fall_check()'s own gate on arrival, see fall.c) then dispatches
 * the real typed direction through the normal command pipeline
 * (`cmd_dispatch()`) rather than reimplementing movement -- reuses
 * every bit of do_move()'s own cost/messaging/trigger/fall-check logic
 * for free, the same trick do_move() itself uses to reach `look`.
 * Deliberately NOT ported: preserving a pre-existing real flight buff
 * across the leap (the real source's own `was_flying` save/restore) --
 * a short, expiring affect duration is close enough for the rare case
 * of a Monk who's independently already flying when they catleap. */
bool cmd_catleap(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "catleap")) {
        descriptor_send(d, "You do not know the secrets of cat-like leaping.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't leap away while fighting!\r\n");
        return true;
    }
    if (sector_is_fall(ch->base.roomp->sector)) {
        descriptor_send(d, "There's no ground beneath you to leap off of here!\r\n");
        return true;
    }

    char raw[32];
    if (sscanf(args, "%31s", raw) != 1) {
        descriptor_send(d, "Which way do you want to leap?\r\n");
        return true;
    }

    int dir = -1;
    size_t len = strlen(raw);
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        if (strncasecmp(DIR_NAMES[i], raw, len) == 0) {
            dir = i;
            break;
        }
    }
    if (dir < 0 || ch->base.roomp->exits[dir] < 0) {
        descriptor_send(d, "You can't go that way.\r\n");
        return true;
    }

    if (!imm && ch->progress.vit < 15) {
        descriptor_send(d, "You're too tired to be jumping around.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "catleap", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    if (!imm)
        being_spend_vit(ch, 15);
    being_apply_affect(ch, AFFECT_FLYING, 2);

    descriptor_send(d, "You leap into the air!\r\n");
    if (ch->base.roomp) {
        char capbuf[128], msg[128];
        snprintf(msg, sizeof(msg), "%s takes a great leap into the air!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }

    if (!success) {
        descriptor_send(d, "You don't make it very far, and stumble back down.\r\n");
        ch->position = POSITION_SITTING;
        return true;
    }

    return cmd_dispatch(d, DIR_NAMES[dir]);
}
