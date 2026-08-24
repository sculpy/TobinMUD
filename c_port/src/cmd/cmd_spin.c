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

/* `spin <target>` (spell/skill functional-completeness audit continued,
 * level 17: skill.c's own Warrior roster entry "A spinning grapple-style
 * strike -- needs a free hand."). Checked the real upstream first
 * (cmd/cmd_spin.cc's `canSpin()`/`spin()`/`spinHit()`/`spinMiss()`):
 * another heavy function -- a flying-victim difficulty roll (still lets
 * the spin proceed either way, just flavor text), a graduated held-item
 * restriction that eases with proficiency, a Monk counter-move defense
 * and a separate "focused avoidance" defense roll, and on a hit either
 * `knockOffMount()` or `crashLanding()` depending on whether the victim
 * was mounted.
 *
 * Scoped down, same "Tobin-scale slice" spirit as bodyslam/headbutt: one
 * `skill_roll_success()` roll (no countermove/focused-avoidance defense
 * rolls -- Tobin has no equivalent secondary-defense mechanic to reuse),
 * reusing `combat_apply_skill_damage()`'s STR-flavored placeholder
 * formula at bash's baseline scale (not bodyslam's x2 -- spin's own
 * roster text reads as a strike, not a full slam). Two real checks DID
 * port cheaply since Tobin already has the underlying affect: refuses a
 * flying target outright unless they're already fighting you (matching
 * `canSpin()`'s hard refusal, not the softer in-`spin()` difficulty-roll
 * flavor that still let it proceed either way), and requires the primary
 * hand empty (matching the roster's own "needs a free hand" -- simpler
 * than the real version's proficiency-graduated one/two-hand easing).
 * Same knockdown-on-hit / knockdown-on-miss shape bodyslam already uses
 * as a cheap stand-in for `crashLanding()`. 6 Vitality, matching the real
 * 6-Move `SPIN_COST` directly. Deliberately NOT ported: mount dismounting
 * (refused outright while either side is mounted, same scope cut bodyslam/
 * shove already made) and the proficiency-graduated held-item easing. */
bool cmd_spin(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "spin")) {
        descriptor_send(d, "You know nothing about spinning.\r\n");
        return true;
    }
    if (ch->position == POSITION_MOUNTED || ch->mount) {
        descriptor_send(d, "You can't spin someone while mounted!\r\n");
        return true;
    }
    if (being_limb_pct(ch, LIMB_LEFT_ARM) < 20 || being_limb_pct(ch, LIMB_RIGHT_ARM) < 20) {
        descriptor_send(d, "You can't spin someone with an injured arm.\r\n");
        return true;
    }
    if (!imm && ch->held[0]) {
        descriptor_send(d, "You need a free hand to spin someone.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Spin whom?\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(ch, raw);
    if (!target) {
        /* Also covers typing your own name -- combat_find_room_target()
         * excludes self already, same precedent as bodyslam/headbutt. */
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (being_is_immortal(target)) {
        descriptor_send(d, "You can't successfully spin an immortal...unless you want to dance.\r\n");
        return true;
    }
    if (target->position == POSITION_MOUNTED || target->mount) {
        descriptor_send(d, "You can't spin someone off a mount that way.\r\n");
        return true;
    }
    if (being_has_affect(target, AFFECT_FLYING) && target->fighting != ch) {
        descriptor_send(d, "You can only spin fliers that are fighting you.\r\n");
        return true;
    }
    if (target->position < POSITION_STANDING) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%s is on the ground. You can't spin them.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }

    if (!imm && ch->progress.vit < 6) {
        descriptor_send(d, "You don't have the vitality to spin anyone!\r\n");
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

    const skill_def_t *sk = skill_find(ch->char_class, "spin", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to spin %s but lose your footing.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to spin you but loses their footing.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        ch->position = POSITION_SITTING;
        return true;
    }

    snprintf(msg, sizeof(msg), "You take ahold of %s and pull hard, spinning them to the ground!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s takes ahold of you and pulls hard, spinning you to the ground!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 1 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 4;
    if (dmg < 1)
        dmg = 1;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    if (!defeated)
        target->position = POSITION_SITTING;
    return true;
}
