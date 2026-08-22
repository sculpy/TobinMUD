/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"

/* `stabbing` (Thief, level 25, level-25 audit batch: "A piercing-weapon
 * melee attack."). Tobin's obj.h has no weapon-type distinction
 * (slashing/piercing/blunt) to gate this on -- only the generic
 * `slash|blunt|pierce|barehand proficiency` COMBAT-tier skills exist as
 * flavor-text placeholders, same as `ranged proficiency`'s own "no
 * subsystem to hook into" precedent (STATUS.md, this session). Ported as
 * a plain bonus-damage attack, same shape as `kneestrike`/`chop`. */
bool cmd_stabbing(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Stab whom? You're not fighting anyone.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "stabbing")) {
        descriptor_send(d, "You don't know how to stab an opponent like that.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "stabbing", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You lunge at %s, but your stab goes wide!\r\n", being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }

    int dmg = 3 + ch->progress.level / 4 + (rand() % 6);
    limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
    int limb_hp_before = target->limbs[limb].hp;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, limb);
    const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
    snprintf(msg, sizeof(msg), "You lunge and stab %s %s!\r\n", being_display_name(target), intensity);
    descriptor_send(d, msg);
    if (!defeated && target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s lunges and stabs you %s!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), intensity);
        descriptor_notify(target->desc, msg);
    }
    return true;
}
