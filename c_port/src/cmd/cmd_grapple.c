/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"
#include "spellcast.h"

/* `grapple` (spell/skill functional-completeness audit, 2026-07-27:
 * Warrior roster entry "Grab and hold an opponent, restricting what
 * they can do.", skill.c level 1, SKILL_TIER_COMBAT). Same one-
 * skill_roll_success()-roll shape as bash/kick/trip/disarm, but the
 * "restricting what they can do" part is scoped down to a straight
 * MUTUAL extended wait-state (Tobin's existing being_set_wait() already
 * blocks every command until it expires -- "still recovering!" -- which
 * IS a real "restricted" state, not a new subsystem) rather than a
 * dedicated new restrain flag: a grapple locks BOTH combatants down for
 * longer than a normal round, unlike bash/trip's knockdown (which only
 * costs the DEFENDER a round). No damage either side, matching the
 * roster description's own lack of a damage clause. */
bool cmd_grapple(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Grapple whom? You're not fighting anyone.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "grapple")) {
        descriptor_send(d, "You don't know how to grapple an opponent.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "grapple", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* `brawl avoidance` (Warrior, level 25, level-25 audit batch) -- see
     * cmd_trip.c's own copy of this same check for the full doc comment. */
    if (success && !being_is_immortal(target) && being_knows_skill(target, "brawl avoidance")) {
        const skill_def_t *avoid_sk = skill_find(target->char_class, "brawl avoidance", false);
        if (avoid_sk && skill_roll_success(skill_learn_from_doing(target, avoid_sk)))
            success = false;
    }

    char msg[160];
    if (!success) {
        being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);
        snprintf(msg, sizeof(msg), "You try to grapple %s, but they slip free!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to grapple you, but you slip free!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    /* A grapple locks up BOTH combatants -- longer than bash/trip's
     * knockdown, and unlike those, the attacker pays the same price as
     * the defender (you can't grapple someone without also being tied
     * up yourself). */
    being_set_wait(ch, 3 * COMBAT_ROUND_PULSES);
    spellcast_distract(target, 1); /* grapple distracts a caster mid-`cast` (Sneezy: grapple 1) */
    being_set_wait(target, 3 * COMBAT_ROUND_PULSES);

    snprintf(msg, sizeof(msg), "You grab hold of %s, pinning them down!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s grabs hold of you, pinning you down!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }
    return true;
}
