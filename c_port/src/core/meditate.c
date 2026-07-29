/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "meditate.h"

#include "affect.h"
#include "being.h"
#include "descriptor.h"
#include "skill.h"

/* Periodic hook that advances every connected, currently-meditating
 * character by one heal roll -- see being.h's `meditating` doc comment
 * and cmd_yoginsa.c for where the task starts/stops on command. */
void meditate_tick_run(long pulse_num) {
    (void)pulse_num;

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *ch = d->character;
        if (!ch || !ch->meditating)
            continue;

        if (ch->position != POSITION_RESTING && ch->position != POSITION_SITTING) {
            ch->meditating = false;
            descriptor_send(d, "Your meditation is broken.\r\n");
            continue;
        }
        if (ch->fighting) {
            ch->meditating = false;
            descriptor_send(d, "Your meditation is broken -- you're fighting!\r\n");
            continue;
        }

        bool imm = being_is_immortal(ch);
        const skill_def_t *sk = skill_find(ch->char_class, "yoginsa", imm);
        bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
        if (!success) {
            descriptor_send(d, "You try to meditate, but your mind won't settle.\r\n");
            continue;
        }

        int heal = 5 + ch->progress.level / 2;
        being_heal(ch, heal);
        being_heal_vit(ch, heal);
        descriptor_send(d, "<g>Meditating refreshes your inner harmonies!<z>\r\n");

        /* `wohlin meditation` (Monk, level 25, level-25 audit batch:
         * "While meditating, unlocks bonus self-cure effects."). This is
         * the "chained secondary cures (self-salve, cure poison,
         * sterilize, cure disease)" cmd_yoginsa.c's own doc comment
         * flagged as blocked on this skill not existing yet -- it now
         * does. Scoped to poison + every disease (Tobin's real cure
         * targets, same affect.h range this whole audit already reuses
         * for `cure poison`/`cure disease`), rolled once per meditation
         * tick alongside the HP/Vitality heal above, not a separate
         * command. */
        if (being_knows_skill(ch, "wohlin meditation")) {
            bool cured_something = false;
            if (being_has_affect(ch, AFFECT_POISON)) {
                being_remove_affect(ch, AFFECT_POISON);
                cured_something = true;
            }
            for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
                if (affect_is_disease(ch->affects[i].type)) {
                    being_remove_affect(ch, ch->affects[i].type);
                    cured_something = true;
                }
            }
            if (cured_something)
                descriptor_send(d, "<g>Your meditation burns away poison and sickness alike!<z>\r\n");
        }
    }
}
