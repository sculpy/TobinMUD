/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"

/* `bonebreak` (Unimplemented skills/spells backlog, Session 158 audit:
 * Monk, skill.c level 50 -- the capstone). Real upstream
 * (cmd/cmd_bonebreak.cc) targets a body slot, deals damage, and flags
 * the limb PART_BROKEN. Tobin has no PART_BROKEN limb flag, but it DOES
 * have AFFECT_DISEASE_BROKEN_BONE (affect.h) -- so a successful break
 * deals heavy damage to a random limb AND leaves the victim with a
 * lingering broken-bone affliction (an HP-draining debuff), which is the
 * faithful stand-in for the broken limb.
 *
 * `iron bones` (Monk, level 38) is the passive counter, wired here: a
 * victim who knows it shrugs the fracture off (the hit still lands, but
 * no broken-bone affect takes hold), and exercising that defence trains
 * their own iron bones. Same fighting-required / one-roll / heavy-lag
 * shape as bash/defenestrate. */
bool cmd_bonebreak(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Break whose bones? You're not fighting anyone.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "bonebreak")) {
        descriptor_send(d, "You know nothing about breaking bones.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "bonebreak", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[192];
    if (!success) {
        snprintf(msg, sizeof(msg), "You seize %s's limb to snap it, but lose your grip!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s seizes your limb to snap it, but loses their grip!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
    int dmg = 5 + (ch->attrs.strength - ATTR_BASE) / 2 + rand() % 8;
    if (dmg < 1)
        dmg = 1;

    /* `iron bones` (Monk, level 38) -- the passive that keeps the bone from
     * breaking. The blow still lands; the fracture doesn't take hold. */
    bool iron_bones = !being_is_immortal(target) && being_knows_skill(target, "iron bones");
    if (iron_bones && target->base.kind == THING_PC) {
        const skill_def_t *ib_sk = skill_find(target->char_class, "iron bones", false);
        if (ib_sk)
            skill_learn_from_doing(target, ib_sk);
    }

    bool defeated = combat_apply_skill_damage(ch, target, dmg, limb);
    if (defeated)
        return true;

    if (iron_bones) {
        snprintf(msg, sizeof(msg), "You wrench %s's %s with a sickening force -- but their bones hold like iron!\r\n",
                 being_display_name(target), limb_name(limb));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s wrenches your %s -- but your iron-hard bones refuse to break!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)), limb_name(limb));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    being_apply_affect(target, AFFECT_DISEASE_BROKEN_BONE, 20);
    snprintf(msg, sizeof(msg), "You wrench %s's %s until the bone snaps with a horrible crack!\r\n",
             being_display_name(target), limb_name(limb));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s wrenches your %s until the bone snaps with a horrible crack!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), limb_name(limb));
        descriptor_send(target->desc, msg);
    }
    return true;
}
