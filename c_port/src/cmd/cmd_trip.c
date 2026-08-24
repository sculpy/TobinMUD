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
#include "spellcast.h"

/* `trip` (spell/skill functional-completeness audit, 2026-07-27: Warrior
 * roster entry "Knock an opponent to the ground.", skill.c level 1,
 * SKILL_TIER_COMBAT). Same scoped-down shape as bash/kick/disarm (one
 * skill_roll_success() roll, no per-object weight/footing math Sneezy's
 * real version leans on) -- but unlike bash, trip deals NO damage at all
 * (Sneezy: a pure knockdown/positioning skill, not an offensive one), so
 * this only ever moves the defender to POSITION_SITTING and costs them a
 * round, matching bash's own knockdown branch but without the limb-damage
 * deviation bash discloses taking. Warrior-only (this port's roster; Sneezy
 * also grants it to some Ranger/Monk lines, not modeled here). */
bool cmd_trip(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Trip whom? You're not fighting anyone.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "trip")) {
        descriptor_send(d, "You don't know how to trip an opponent.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "trip", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* `brawl avoidance` (Warrior, level 25, level-25 audit batch:
     * "Passive resistance to grapple- and trip-style attacks."). Same
     * passive-defensive-save shape as `weapon retention` (cmd_disarm.c) --
     * a separate resist roll for the defender, checked here and in
     * cmd_grapple.c, the two skills it names. */
    if (success && !being_is_immortal(target) && being_knows_skill(target, "brawl avoidance")) {
        const skill_def_t *avoid_sk = skill_find(target->char_class, "brawl avoidance", false);
        if (avoid_sk && skill_roll_success(skill_learn_from_doing(target, avoid_sk)))
            success = false;
    }

    /* Same LAG_3-equivalent as bash (a full knockdown attempt, not the
     * lighter disarm-style LAG_2). */
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to trip %s, but they keep their footing!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to trip you, but you keep your footing!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You sweep %s's legs out from under them!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s sweeps your legs out from under you!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    spellcast_distract(target, 1); /* trip distracts a caster mid-`cast` (Sneezy: trip 1-2) */
    target->position = POSITION_SITTING;
    being_set_wait(target, COMBAT_ROUND_PULSES);
    return true;
}
