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

/* `headbutt <target>` (spell/skill functional-completeness audit
 * continued, level 15: skill.c's own Warrior roster entry, corrected
 * below). Checked the real upstream first (cmd/cmd_headbutt.cc's
 * canHeadbutt()/headbutt()/headbuttHit()/headbuttMiss()): a relative-
 * HEIGHT-driven skill -- picks a different body region to strike
 * (foot/leg/crotch/body/throat/jaw/skull) depending on how the
 * attacker's height compares to the victim's, refuses outright if the
 * attacker is more than 25% shorter than the target, and a miss can
 * either be a clean dodge (if the victim knows counter-move) or a
 * "moves head, you stumble" flub.
 *
 * Tobin has no height stat at all, so the whole region-selection
 * mechanic has no faithful port -- scoped down to the same "Tobin-scale
 * slice" shape as chi/bodyslam: one `skill_roll_success()` roll,
 * striking LIMB_HEAD specifically (skill.c's own corrected roster text
 * still calls out "roughly your own height" as flavor, even though it's
 * no longer mechanically enforced -- an honest, disclosed simplification
 * rather than silently dropping the flavor line entirely), reusing
 * `combat_apply_skill_damage()`'s STR-flavored placeholder formula like
 * every other physical skill-combat command. No knockdown (the real
 * version doesn't knock anyone down either, just a brief extra wait on
 * a high-value hit -- reused here unconditionally via `being_set_wait()`
 * same as chi). 6 Vitality, matching the real version's own Move cost
 * directly (unlike bash/bodyslam's own halved costs -- headbutt has no
 * comparable Move-heavy real cost to halve from). Deliberately NOT
 * ported: the counter-move-avoidance branch (Tobin's own "counter move"
 * roster entry has no handler yet either, same gap shove.c's own miss
 * path already notes) and the flying-target/flying-attacker exemption
 * (Tobin's `AFFECT_FLYING` doesn't currently interact with any other
 * melee skill's targeting either). */
bool cmd_headbutt(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "headbutt")) {
        descriptor_send(d, "You know nothing about headbutting.\r\n");
        return true;
    }
    if (ch->position == POSITION_MOUNTED || ch->mount) {
        descriptor_send(d, "You can't butt heads while mounted!\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Butt whose head?\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(ch, raw);
    if (!target) {
        /* Also covers typing your own name -- combat_find_room_target()
         * excludes self from its search same as it does for attack/kill
         * (and cmd_chi.c's own identical note). */
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (being_is_immortal(target)) {
        descriptor_send(d, "You can't butt an immortal.\r\n");
        return true;
    }

    if (!imm && ch->progress.vit < 6) {
        descriptor_send(d, "You lack the vitality.\r\n");
        return true;
    }
    if (!imm)
        being_spend_vit(ch, 6);

    if (!ch->fighting) {
        ch->fighting = target;
        target->fighting = ch;
        ch->sneaking = false;
        target->sneaking = false;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "headbutt", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "%s moves their head out of the way, causing you to miss.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "You move your head out of the way, causing %s to miss.\r\n",
                     being_display_name(ch));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You headbutt %s, slamming your head into their skull!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s headbutts you, slamming their head into your skull!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 1 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 4;
    if (dmg < 1)
        dmg = 1;
    combat_apply_skill_damage(ch, target, dmg, LIMB_HEAD);
    return true;
}
