/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "meditate.h"

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
    }
}
