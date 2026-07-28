/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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

/* `deathstroke <target>` (spell/skill functional-completeness audit
 * continued, level 20: skill.c's own Warrior roster entry "A heavy,
 * finishing-style attack against a single target."). Checked the real
 * upstream first (cmd/cmd_deathstroke.cc's `doDeathstroke()`/
 * `deathstrokeSuccess()`/`deathstrokeFail()`/`deathstrokeCounterattack()`):
 * a heavy function -- a self-lockout duration preventing reuse for a
 * stretch of real time, an armor-penalty debuff applied regardless of
 * outcome (scaling with the caster's own level squared), a hitroll buff
 * on success, and -- if the VICTIM also happens to know deathstroke --
 * a whole separate counterattack roll against the original caster.
 *
 * Scoped way down, same "Tobin-scale slice" spirit as bodyslam/slam:
 * one `skill_roll_success()` roll, the heaviest single-hit damage
 * multiplier of any Warrior skill so far (x3, above slam's x2.5),
 * reusing `combat_apply_skill_damage()`'s STR-flavored formula. One
 * real check ported cheaply since Tobin already has the mechanic:
 * requires a wielded weapon (`combat_wielded_weapon()`, matching
 * combat.c's own messaging/mods lookup) -- unlike every skill ported so
 * far this audit, deathstroke genuinely refuses bare-handed. 8
 * Vitality, matching the real 8-Move `DEATHSTROKE_MOVE` directly. NOT
 * ported: the self-lockout timer (no generic per-skill cooldown
 * subsystem beyond the per-command `wait`, which only blocks the very
 * next input, not a stretch of real time), the armor-penalty/hitroll-
 * buff affects (no generic arbitrary-numeric-modifier system -- Tobin's
 * AFFECT_* entries are each hardcoded to one specific stat, and neither
 * "armor" nor "hitroll" has its own slot to reuse without conflating
 * with an unrelated existing affect), and the victim's own
 * counterattack-if-they-also-know-deathstroke chain. */
bool cmd_deathstroke(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "deathstroke")) {
        descriptor_send(d, "You know nothing about deathblows.\r\n");
        return true;
    }
    if (!imm && being_has_affect(ch, AFFECT_BERSERK)) {
        descriptor_send(d, "You are berserking! You can't focus enough to deathstroke anyone!\r\n");
        return true;
    }
    if (ch->position == POSITION_MOUNTED || ch->mount) {
        descriptor_send(d, "You can't deathstroke while mounted!\r\n");
        return true;
    }
    if (!imm && !combat_wielded_weapon(ch)) {
        descriptor_send(d, "You need to hold a weapon in your primary hand to make this a success.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Deathstroke whom?\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(ch, raw);
    if (!target) {
        /* Also covers typing your own name -- combat_find_room_target()
         * excludes self already, same precedent as every other
         * skill-combat command this audit. */
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (being_is_immortal(target)) {
        descriptor_send(d, "You cannot attack an immortal.\r\n");
        return true;
    }

    if (!imm && ch->progress.vit < 8) {
        descriptor_send(d, "You don't have the vitality to make the move!\r\n");
        return true;
    }
    if (!imm)
        being_spend_vit(ch, 8);

    if (!ch->fighting) {
        ch->fighting = target;
        target->fighting = ch;
        ch->sneaking = false;
        target->sneaking = false;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "deathstroke", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You fail to hit %s's vital area.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s attempts to hit your vital area, but fails miserably.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You hit %s in their vital organs!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s hits you in your vital organs!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = (int)(3.0 * (1 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 6));
    if (dmg < 3)
        dmg = 3;
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    return true;
}
