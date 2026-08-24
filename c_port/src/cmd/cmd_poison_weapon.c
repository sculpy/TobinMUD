/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"

/* `poison weapon` (Unimplemented skills/spells backlog, Session 158
 * audit: Thief, skill.c level 25). Real upstream (disc_thief_murder.cc's
 * poisonWeapon) coats a wielded blade so its hits envenom the victim.
 * Ported as a self-affect on the thief -- AFFECT_POISON_BLADE, a plain
 * flag/timer, same shape as fortify/berserk -- rather than per-object
 * weapon state (Tobin has no transient per-instance weapon field to
 * write a coating onto): while it's up and the thief is wielding a
 * weapon, each landed melee hit has a chance to leave the victim
 * poisoned (AFFECT_POISON DoT), applied in combat_strike(). One
 * skill_roll_success() roll to apply the coating; requires a weapon in
 * hand. The coating lasts a set number of rounds (it dries out), which
 * is also the recast gate. */
bool cmd_poison_weapon(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "poison weapon")) {
        descriptor_send(d, "You don't know how to poison a weapon.\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_POISON_BLADE)) {
        descriptor_send(d, "Your weapon is already coated with venom.\r\n");
        return true;
    }
    if (!imm && !combat_wielded_weapon(ch)) {
        descriptor_send(d, "You need a weapon in hand to poison it.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "poison weapon", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, COMBAT_ROUND_PULSES);

    if (!success) {
        descriptor_send(d, "You fumble the vial and spill the venom, coating nothing.\r\n");
        return true;
    }

    being_apply_affect(ch, AFFECT_POISON_BLADE, POISON_BLADE_DURATION_ROUNDS);
    descriptor_send(d, "You carefully smear a slick of venom along your weapon's edge.\r\n");
    if (ch->base.roomp) {
        char capbuf[128], echo[160];
        snprintf(echo, sizeof(echo), "%s carefully coats a weapon with something.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, echo);
    }
    return true;
}
