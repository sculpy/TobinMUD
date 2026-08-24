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
#include "room.h"
#include "skill.h"

/* `charge <target>` -- Deikhan mounted-combat trio, missing-skill audit
 * batch C, 2026-08-09 (real upstream SKILL_CHARGE, disc_deikhan_mounted.h/
 * cmd_charge.cc). Tobin has no Deikhan class -- same "riding" itself
 * already gets, every class can learn `charge` (see skill.c). Real
 * upstream's own charge() is a large gauntlet: per-mount-race skill
 * gates, a Shock Cavalry weight-trampling damage multiplier, a group/
 * attacker-count "innocent bystander" check, an AFF_ORIENT guaranteed-
 * hit case, and its own "very hard to tank and charge" rule. Tobin has
 * none of that machinery (no per-mount-race skills, no Shock Cavalry
 * skill, no multi-attacker threat tracking, no orient affect) -- scoped
 * down to the real mechanic's core: a mounted bonus-damage attack that
 * knocks the target down, gated on being mounted and not already
 * fighting (real upstream's own "should be an opening move" rule,
 * ported as a hard refusal instead of a modifier since Tobin combat has
 * no per-attacker threat count to make a chance-based version of).
 * `advanced riding` adds a real, disclosed-scaled bonus to the damage,
 * echoing real upstream's own ridingSkillBonus scaling. A failed roll
 * still starts the fight (real upstream's own charge() always calls
 * reconcileDamage() -- win or lose, closing with a mounted charge means
 * combat either way). */
bool cmd_charge(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "charge")) {
        descriptor_send(d, "You don't know how to charge anyone.\r\n");
        return true;
    }
    if (!ch->mount) {
        descriptor_send(d, "You must be mounted to charge!\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "A charge only works as an opening move -- you're already in a fight!\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Charge whom?\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(ch, args);
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (target == ch || target == ch->mount) {
        descriptor_send(d, "That would be a very short charge.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "charge", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    if (!ch->fighting) {
        ch->fighting = target;
        target->fighting = ch;
        being_set_wait(ch, COMBAT_ROUND_PULSES);
    }

    char msg[256];
    if (!success) {
        snprintf(msg, sizeof(msg), "You charge %s, but %s dodges to the side at the last moment!\r\n",
                 being_display_name(target), being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s and their mount come charging at you -- you dodge just in time!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_notify(target->desc, msg);
        }
        return true;
    }

    int dmg = 5 + ch->progress.level / 3 + (rand() % 8);
    /* `advanced riding` (this same batch) -- real upstream scales
     * charge's damage by a "ridingSkillBonus" derived from riding
     * proficiency (60-110%); ported here as a flat extra chunk of
     * damage scaled by proficiency instead, same disclosed-simplified
     * shape the rest of this batch's mounted skills use. */
    if (!imm && being_knows_skill(ch, "advanced riding")) {
        const skill_def_t *adv_sk = skill_find(ch->char_class, "advanced riding", false);
        if (adv_sk) {
            int adv_prof = skill_learn_from_doing(ch, adv_sk);
            dmg += adv_prof / 10;
        }
    }

    limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
    int limb_hp_before = target->limbs[limb].hp;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, limb);
    const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
    snprintf(msg, sizeof(msg), "You charge %s, trampling them with a mighty blow %s!\r\n",
             being_display_name(target), intensity);
    descriptor_send(d, msg);
    if (!defeated) {
        target->position = POSITION_SITTING;
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s and their mount charge you down, trampling you %s!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)), intensity);
            descriptor_notify(target->desc, msg);
        }
    }
    return true;
}
