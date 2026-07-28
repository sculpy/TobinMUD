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

/* `slam <target>` (spell/skill functional-completeness audit continued,
 * level 20: skill.c's own Warrior roster entry, corrected this pass --
 * see below). Checked the real upstream first (cmd/cmd_slam.cc's
 * `doSlam()`/`slamSuccess()`/`slamFail()`): no stun anywhere in it --
 * the roster's own old flavor text ("extra damage and a stun") was an
 * inaccurate guess, same pattern as jirin/cintai/teleport's earlier
 * corrections, fixed here too. Real slam's actual standout feature is
 * its damage formula: scaled up to a level-tiered PERCENTAGE of the
 * victim's own max HP (a lookup table keyed by the victim's level,
 * 15% at low level down to 0.75% at endgame) specifically so it stays
 * relevant against high-level targets where a flat STR-based roll
 * would fall off -- not ported (no clean Tobin equivalent without a
 * new formula shape breaking from every other skill-combat command's
 * shared STR-flavored placeholder). Scoped instead to the same
 * placeholder formula at the heaviest scale used so far (x2.5, above
 * bodyslam's x2) so it still reads as the hardest-hitting Warrior
 * strike in the roster, matching the roster's own "considerable extra
 * damage" framing. 3 Vitality, matching the real 3-Move `SLAM_MOVE`
 * directly. Not ported: the held-item restriction (bare hand or a real
 * weapon only, no light/scroll/instrument) and the berserk-doubles-
 * damage branch (AFFECT_BERSERK already exists but doubling on top of
 * this scope cut's already-heavier multiplier felt like double-
 * counting, not a faithful port). */
bool cmd_slam(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "slam")) {
        descriptor_send(d, "You wouldn't even know where to begin in executing that maneuver.\r\n");
        return true;
    }
    if (ch->position == POSITION_MOUNTED || ch->mount) {
        descriptor_send(d, "You can't perform that attack while mounted!\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Slam whom?\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(ch, raw);
    if (!target) {
        /* Also covers typing your own name -- combat_find_room_target()
         * excludes self already, same precedent as bodyslam/headbutt/spin. */
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (being_is_immortal(target)) {
        descriptor_send(d, "Attacking an immortal would be a bad idea.\r\n");
        return true;
    }

    if (!imm && ch->progress.vit < 3) {
        descriptor_send(d, "You don't have the vitality to make the move!\r\n");
        return true;
    }
    if (!imm)
        being_spend_vit(ch, 3);

    if (!ch->fighting) {
        ch->fighting = target;
        target->fighting = ch;
        ch->sneaking = false;
        target->sneaking = false;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "slam", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "Your attempt at slamming %s fails to make contact.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s attempts to slam you but comes up short.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You slam into %s, inflicting considerable damage!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s slams into you, inflicting considerable damage!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = (int)(2.5 * (1 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 6));
    if (dmg < 2)
        dmg = 2;
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    return true;
}
