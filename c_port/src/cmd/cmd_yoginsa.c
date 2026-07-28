/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "pulse.h"
#include "skill.h"

/* `yoginsa` (spell/skill functional-completeness audit, 2026-07-27:
 * Monk roster entry, skill.c level 1, SKILL_TIER_CLASS -- "Meditate to
 * recover your vitality faster."). Checked the real upstream's
 * task_yoginsa() (disc/disc_monk_meditation.cc:9) first: it's a
 * background task that re-checks every 4 pulses while resting, rolling
 * a fresh skill_roll_success()-equivalent each tick to restore HP/Move/
 * mana, plus chained secondary effects (self-salve, cure poison,
 * sterilize, cure disease) at higher proficiency via a SEPARATE
 * "wohlin meditation" skill Tobin's roster doesn't even have. Scoped
 * way down to a single-action heal -- same "one roll, not a recurring
 * background task" convention every other skill this audit pass uses
 * (Tobin has no generic "start a multi-tick self-task" mechanism to
 * reuse; planting.c's tick counter is the closest shape but is single-
 * purpose for seed-growing) -- and to Tobin's own two resources (HP,
 * Vitality) rather than the original's three (Tobin has no separate
 * mana pool at all, see being.h's progress_t). No chained secondary
 * cures ported -- those all gate on "wohlin meditation", which doesn't
 * exist in this roster. Must be resting or sitting -- a standing
 * character is sat down automatically (user 2026-07-27: typing
 * yoginsa/meditate should just start meditating, not bounce off a
 * "go sit first" refusal), matching cmd_sit's own message/echo shape
 * so the auto-sit reads exactly like the player typed `sit` themselves. */
bool cmd_yoginsa(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "yoginsa")) {
        descriptor_send(d, "You don't know how to meditate that way.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't meditate while fighting!\r\n");
        return true;
    }
    if (ch->position != POSITION_RESTING && ch->position != POSITION_SITTING) {
        if (ch->position != POSITION_STANDING) {
            /* Sleeping/mounted/etc -- no automatic path to a meditating
             * posture from here, same refusal the old code gave everyone. */
            descriptor_send(d, "You need to be sitting or resting to meditate.\r\n");
            return true;
        }
        ch->position = POSITION_SITTING;
        descriptor_send(d, "You sit down.\r\n");
        if (ch->base.roomp) {
            char msg[160];
            snprintf(msg, sizeof(msg), "%s sits down.\r\n", ch->base.name);
            descriptor_room_echo(ch->base.roomp, ch, msg);
        }
    }

    const skill_def_t *sk = skill_find(ch->char_class, "yoginsa", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    if (!success) {
        descriptor_send(d, "You try to meditate, but your mind won't settle.\r\n");
        return true;
    }

    int heal = 5 + ch->progress.level / 2;
    being_heal(ch, heal);
    being_heal_vit(ch, heal);

    descriptor_send(d, "<g>Meditating refreshes your inner harmonies!<z>\r\n");
    return true;
}
