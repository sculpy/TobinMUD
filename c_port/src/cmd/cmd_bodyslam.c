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

/* `bodyslam <target>` (spell/skill functional-completeness audit
 * continued, level 10: skill.c's own Warrior roster entry "A grappling
 * throw that slams your opponent down for damage."). Checked the real
 * upstream first (cmd/cmd_bodyslam.cc's `canBodyslam()`/`bodyslam()`/
 * `bodyslamHit()`/`bodyslamMiss()`): a genuinely heavy function --
 * three distinct miss types (a DEX-avoid, a STR-fails-to-lift, and a
 * separate Monk counter-move defense), a weight/carry-capacity
 * comparison against the victim's total carried weight, held-item
 * restrictions that ease as proficiency climbs, a follow-up
 * `trySpringleap()` chain, and mount dismounting.
 *
 * Scoped way down, same "Tobin-scale slice" spirit as bash/kick/
 * disarm: one `skill_roll_success()` roll (no armor-modifier percent
 * factor, no weight/carry comparison -- Tobin doesn't model carry-
 * capacity robustly enough to gate a skill on it), reusing `combat_
 * apply_skill_damage()`'s STR-flavored placeholder formula like every
 * other physical skill-combat command, scaled up (x2) to read as a
 * heavier hit than bash's baseline. On success, knocks the victim down
 * (POSITION_SITTING, same "easier target" bonus bash's own knockdown
 * gives) and deals real damage; on failure, the ATTACKER goes down
 * instead (a direct, cheap stand-in for the real version's three-way
 * miss branching and its `crashLanding()` call, without needing
 * springleap -- which isn't implemented in Tobin at all yet -- or the
 * countermove/weight-comparison machinery this scope cut already
 * drops). 15 Vitality (half the real 30-Move cost, same rough halving
 * shove's own Vitality cost uses relative to its real Move range).
 * Deliberately NOT ported: mount dismounting (refused outright while
 * either side is mounted, same scope cut shove.c already made) and
 * the proficiency-gated held-item restriction (no clear Tobin
 * equivalent of "not skilled enough to bodyslam one-handed"). */
bool cmd_bodyslam(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "bodyslam")) {
        descriptor_send(d, "You know nothing about bodyslamming.\r\n");
        return true;
    }
    if (ch->position == POSITION_MOUNTED || ch->mount) {
        descriptor_send(d, "You can't bodyslam while mounted!\r\n");
        return true;
    }
    if (being_limb_pct(ch, LIMB_LEFT_ARM) < 20 || being_limb_pct(ch, LIMB_RIGHT_ARM) < 20) {
        descriptor_send(d, "You can't bodyslam with an injured arm.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Bodyslam whom?\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(ch, raw);
    if (!target) {
        /* Also covers typing your own name -- combat_find_room_target()
         * excludes self from its search same as it does for attack/kill
         * (and cmd_chi.c's own identical note), so this refusal doubles
         * as "you can't bodyslam yourself" without a separate check. */
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (being_is_immortal(target)) {
        descriptor_send(d, "You can't successfully bodyslam an immortal.\r\n");
        return true;
    }
    if (target->position == POSITION_MOUNTED || target->mount) {
        descriptor_send(d, "You can't bodyslam someone off a mount that way.\r\n");
        return true;
    }
    if (target->position < POSITION_STANDING) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%s is already down. You can't bodyslam them.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }

    if (!imm && ch->progress.vit < 15) {
        descriptor_send(d, "You don't have the vitality to bodyslam anyone!\r\n");
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

    const skill_def_t *sk = skill_find(ch->char_class, "bodyslam", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to bodyslam %s, but end up falling on your face!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to bodyslam you, but ends up falling on their face!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        ch->position = POSITION_SITTING;
        return true;
    }

    snprintf(msg, sizeof(msg), "You lift %s over your head and slam them to the ground!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s lifts you over their head and slams you to the ground!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 2 * (1 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 4);
    if (dmg < 2)
        dmg = 2;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    if (!defeated)
        target->position = POSITION_SITTING;
    return true;
}
