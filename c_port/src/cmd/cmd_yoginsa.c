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
 * recover your vitality faster."). Originally scoped down to a single-
 * action heal (real upstream's own task_yoginsa(), disc/disc_monk_
 * meditation.cc, is a recurring background task, re-checking every 4
 * pulses while resting) -- user 2026-07-28: "yoginsa should be
 * automatic, a task", reverting that scope-down back toward the real
 * shape now that a background-task pattern exists in Tobin (planting.c).
 * This command just TOGGLES `being_t.meditating` on/off (see its own
 * doc comment, being.h) -- meditate_tick_run() (meditate.c) does the
 * actual per-tick heal roll. Must be resting or sitting -- a standing
 * character is sat down automatically (user 2026-07-27: typing
 * yoginsa should just start meditating, not bounce off a "go sit
 * first" refusal), matching cmd_sit's own message/echo shape. Chained
 * secondary cures (self-salve, cure poison, sterilize, cure disease)
 * still not ported -- those all gate on "wohlin meditation", which
 * doesn't exist in this roster. Tobin's own two resources (HP,
 * Vitality) stand in for the original's three (no separate mana pool
 * exists, see being.h's progress_t). */
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

    if (ch->meditating) {
        ch->meditating = false;
        descriptor_send(d, "You stop meditating.\r\n");
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

    ch->meditating = true;
    descriptor_send(d, "You begin meditating.\r\n");
    return true;
}
