/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"

/* `stomp` (missing-skill audit, 2026-08-05: Warrior). Real upstream
 * SKILL_STOMP (disc_warrior_brawling.cc) is a leg/foot attack, one of
 * several sub-attacks the berserk state can auto-trigger from a random
 * pool -- Tobin has no such auto-proc pool (its own `berserk`,
 * cmd_berserk.c, is a flat passive buff, not a proc table), so this
 * ports as its own standalone command instead, same shape as `kick`
 * (cmd_kick.c) -- see that file's header comment for the "one flat
 * roll, no per-zone table" scoping rationale, which applies here too.
 * Distinguished from kick by targeting the legs specifically and a
 * heavier lag (a stomp is a slower, more committed strike). */
bool cmd_stomp(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Stomp whom? You're not fighting anyone.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "stomp")) {
        descriptor_send(d, "You don't know how to stomp.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "stomp", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to stomp %s, but they step out of the way!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to stomp you, but you step out of the way!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You stomp down hard on %s!\r\n", being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s stomps down hard on you!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 2 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 5;
    if (dmg < 1)
        dmg = 1;
    limb_t leg = (rand() % 2 == 0) ? LIMB_LEFT_LEG : LIMB_RIGHT_LEG;
    combat_apply_skill_damage(ch, target, dmg, leg);
    return true;
}
