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

/* `defenestrate` (Unimplemented skills/spells backlog, Session 158 audit:
 * Monk, skill.c level 42). Real upstream (disc_monk_leverage.cc) is a
 * leverage throw -- grab the victim and hurl them, ideally out a window
 * into another room. Tobin has no window/room-throw object plumbing, so
 * this is scoped to the mechanic that survives: a heavy leverage throw
 * that slams the victim to the ground and costs them a round, same
 * shape/precedent as `bash` (an extra action layered on the automatic
 * per-round exchange; must already be fighting; one skill_roll_success()
 * roll; a heavy combat-lag round on the thrower win or lose). */
bool cmd_defenestrate(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Defenestrate whom? You're not fighting anyone.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "defenestrate")) {
        descriptor_send(d, "You don't know how to throw anyone like that.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "defenestrate", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[192];
    if (!success) {
        snprintf(msg, sizeof(msg), "You grab for %s to throw them, but they twist free!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s grabs for you to throw you, but you twist free!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You grab %s and hurl them bodily to the ground!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s grabs you and hurls you bodily to the ground!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 2 + (ch->attrs.strength - ATTR_BASE) / 3 + rand() % 5;
    if (dmg < 1)
        dmg = 1;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    if (!defeated) {
        target->position = POSITION_SITTING;
        being_set_wait(target, COMBAT_ROUND_PULSES);
    }
    return true;
}
