/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "skill.h"

/* `meditate` (Mage/Druid, skill.c level 1, "Governs... recover vitality/
 * mana faster") -- user 2026-08-06: "meditate isnt a spell" / "meditate
 * sits a character down and meditates back to his max mana" / "gain is
 * automatic like it is in yoginsa". Was previously wired as `cast
 * meditate` (a one-shot spell effect, requiring a component and a skill
 * roll to even go off) -- wrong shape entirely; real SneezyMUD's
 * meditate is a passive discipline, not something you cast. Rebuilt as
 * its own standalone command, an exact mirror of cmd_yoginsa.c's own
 * shape (auto-sits a standing character, toggles `being_t.meditating`
 * on/off, the shared meditate_tick_run() background task does the
 * actual per-tick heal roll and proficiency gain) -- the only
 * difference is WHICH resource that tick restores, which
 * meditate_tick_run() itself now decides by checking whether the
 * caller actually knows "meditate" (Mana, Mage only -- see
 * being_calc_max_mana()'s own doc comment for why not Druid) versus
 * "yoginsa" (HP/Vitality, unchanged). A being knowing both (shouldn't
 * happen given the roster, but not asserted against) gets the Mana
 * path, since `meditate` is checked first there. */
bool cmd_meditate(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "meditate")) {
        descriptor_send(d, "You don't know how to meditate that way.\r\n");
        return true;
    }

    if (ch->meditating) {
        ch->meditating = false;
        /* Restore to plain sitting -- see cmd_yoginsa.c's matching branch
         * for why the original sit/rest posture isn't tracked to restore
         * exactly. */
        ch->position = POSITION_SITTING;
        descriptor_send(d, "You stop meditating.\r\n");
        return true;
    }

    if (ch->fighting) {
        descriptor_send(d, "You can't meditate while fighting!\r\n");
        return true;
    }
    if (ch->position != POSITION_RESTING && ch->position != POSITION_SITTING) {
        if (ch->position != POSITION_STANDING) {
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
    ch->position = POSITION_MEDITATE;
    descriptor_send(d, "You begin meditating.\r\n");
    return true;
}
