/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"

/* `kick` (Sneezy → Tobin feature audit, "Skill-based combat"). Checked
 * Sneezy's own cmd/cmd_kick.cc first: the real version rolls a random
 * body-zone hit (feet/leg/waist/chest/head) with per-zone bonus effects
 * (a head kick applies extra wait to the VICTIM, a groin kick can stun
 * an unarmored male) and a separate `SKILL_ADVANCED_KICKING` passive
 * that auto-converts melee swings into kicks for monks specifically,
 * rather than making Monk kick its own command. Scoped way down here to
 * match this port's existing granularity: one flat DEX-flavored damage
 * roll to LIMB_BODY, no per-zone table -- Tobin's roster lists "kick" for
 * every non-caster class (user, 2026-08-04: "all classes except for
 * casters should get kick at level 1" -- Warrior/Thief/Monk, all level
 * 1), all reaching this same command rather than splitting an active/
 * passive pair.
 *
 * Same "extra action layered on the automatic round" shape as
 * cmd_bash.c -- see that file's header comment for the full rationale
 * (unconditional attacker lag, parallel automatic combat, disclosed
 * placeholder-damage-on-success deviation). No knockdown effect (unlike
 * bash) -- kick's whole point here is the bonus damage.
 *
 * `kick <target>` can also OPEN a fight (user, 2026-08-05: "kick should
 * be a way to start a fight"), same target-lookup + fighting-pointer-
 * swap shape cmd_attack.c already uses -- sleeping/feared checks, snap
 * to standing, `sneaking` cleared on both sides. If already fighting,
 * a target argument is ignored (kicks the current opponent, same as
 * every other in-combat skill command here); a target is only required
 * when NOT already fighting. */
bool cmd_kick(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    if (!ch->fighting) {
        char tok[64];
        if (sscanf(args, "%63s", tok) != 1) {
            descriptor_send(d, "Kick whom?\r\n");
            return true;
        }
        if (ch->position == POSITION_SLEEPING) {
            descriptor_send(d, "You can't fight in your sleep!\r\n");
            return true;
        }
        if (being_has_affect(ch, AFFECT_FEAR)) {
            descriptor_send(d, "You're too afraid to fight!\r\n");
            return true;
        }
        ch->feigning = false;
        being_t *opener = combat_find_room_target(ch, tok);
        if (!opener) {
            descriptor_send(d, "They aren't here.\r\n");
            return true;
        }
        if (ch->position != POSITION_STANDING && ch->position != POSITION_MOUNTED) {
            ch->position = POSITION_STANDING;
            descriptor_send(d, "You scramble to your feet.\r\n");
        }
        ch->fighting = opener;
        opener->fighting = ch;
        ch->sneaking = false;
        opener->sneaking = false;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "kick")) {
        descriptor_send(d, "You don't know how to kick.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "kick", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* Same LAG_3-equivalent as bash (Sneezy: both are cmd/cmd_*.cc
     * DISC_*_FIGHT skills with matching lag). */
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to kick %s, but they dodge out of the way!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s tries to kick you, but you dodge out of the way!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You land a solid kick on %s!\r\n", being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s lands a solid kick on you!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 1 + (ch->attrs.dexterity - ATTR_BASE) / 4 + rand() % 4;
    if (dmg < 1)
        dmg = 1;
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    return true;
}
