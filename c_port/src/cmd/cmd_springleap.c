/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "pulse.h"
#include "skill.h"

/* `springleap` (spell/skill functional-completeness audit continued,
 * level 20: skill.c's own Monk roster entry "Spring instantly from
 * sitting or resting to standing."). Checked the real upstream first
 * (disc/disc_monk_leverage.cc's `doSpringleap()`/`springleap()`): a
 * genuinely simple one, no real scope cut needed -- refuses outright
 * unless resting or sitting, one `skill_roll_success()` roll, success
 * stands you up immediately with a flavor message, failure leaves you
 * exactly where you were ("flop back down") with its own flavor
 * message. Ported faithfully. */
bool cmd_springleap(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "springleap")) {
        descriptor_send(d, "You don't know how to do that.\r\n");
        return true;
    }
    if (ch->position != POSITION_RESTING && ch->position != POSITION_SITTING) {
        descriptor_send(d, "You're not in position for that!\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "springleap", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, COMBAT_ROUND_PULSES);

    if (success) {
        descriptor_send(d, "You spring off the ground and land lightly on your feet.\r\n");
        char msg[128];
        snprintf(msg, sizeof(msg), "%s springs up and lands lightly on their feet.\r\n",
                 ch->base.name);
        if (ch->base.roomp)
            descriptor_room_echo(ch->base.roomp, ch, msg);
        ch->position = POSITION_STANDING;
    } else {
        descriptor_send(d, "You try to spring up, but flop back down.\r\n");
        char msg[128];
        snprintf(msg, sizeof(msg), "%s tries to spring up, but flops back down.\r\n",
                 ch->base.name);
        if (ch->base.roomp)
            descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}
