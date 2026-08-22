/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"
/* `cudgel <target>` (Thief, missing-skill audit, skill.c level 41,
 * SKILL_TIER_ADVANCED). Real upstream (disc_thief_murder.cc's own
 * cudgel()) is a non-lethal knockout blow: needs a wielded weapon,
 * deals NO real damage either way (reconcileDamage(victim, 0, ...) on
 * both the success and miss paths -- this is purely a stun skill, not
 * a damage skill, matching the roster's own "non-lethal knockout-style
 * attack" wording), and on a clean success knocks the victim out cold
 * for a while; a partial success against a much lower-level victim
 * instead just daubs on a lesser "dazed" penalty.
 *
 * Tobin has no height/undead/flying/mount checks to port faithfully
 * (same gap headbutt.c's own doc comment already discloses for a
 * different skill), and no separate "dazed" affect infrastructure, so
 * this collapses to a single skill_roll_success() roll: success knocks
 * the target out (POSITION_STUNNED, an existing enum value nothing has
 * ever actually transitioned a being INTO before this), miss does
 * nothing. POSITION_STUNNED is real: regen.c/combat.c/spellcast.c
 * already treat it as "can't act/can't cast", same as POSITION_INCAP.
 * Duration picked at 6*COMBAT_ROUND_PULSES (~7.2s) -- long enough for a
 * knockout to matter without needing Sneezy's own restrict_xp()
 * anti-twink bookkeeping (Tobin has no equivalent system to hook). */
bool cmd_cudgel(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "cudgel")) {
        descriptor_send(d, "You know nothing about cudgeling.\r\n");
        return true;
    }
    if (!imm && !combat_wielded_weapon(ch)) {
        descriptor_send(d, "You need to wield a weapon to cudgel with.\r\n");
        return true;
    }
    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Cudgel whom?\r\n");
        return true;
    }
    being_t *target = combat_find_room_target(ch, raw);
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (being_is_immortal(target)) {
        descriptor_send(d, "Your cudgel attempt has no effect on your immortal target.\r\n");
        return true;
    }
    if (!imm && ch->progress.vit < 15) {
        descriptor_send(d, "You lack the vitality.\r\n");
        return true;
    }
    if (!imm)
        being_spend_vit(ch, 15);
    if (!ch->fighting) {
        ch->fighting = target;
        target->fighting = ch;
        ch->sneaking = false;
        target->sneaking = false;
    }
    const skill_def_t *sk = skill_find(ch->char_class, "cudgel", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, COMBAT_ROUND_PULSES);
    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You miss your attempt to knock %s unconscious.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s misses their attempt to knock you unconscious.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }
    snprintf(msg, sizeof(msg), "You knock %s on the noggin, knocking them unconscious!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s knocks you on the noggin, knocking you unconscious!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }
    target->position = POSITION_STUNNED;
    being_set_wait(target, 6 * COMBAT_ROUND_PULSES);
    return true;
}
