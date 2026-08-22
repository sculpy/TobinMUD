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

/* `chi [<target>]` (spell/skill functional-completeness audit,
 * 2026-07-27 continued: Monk roster entry, skill.c level 1,
 * SKILL_TIER_CLASS). Checked the real upstream `TBeing::doChi()`/
 * `chiSelf()`/`chiTarget()`/`chiRoom()`/`chiObject()` (misc/being.cc)
 * first -- skill.c's own pre-existing flavor text ("a mana-based
 * healing touch") turned out to be wrong: the real skill is primarily
 * an OFFENSIVE chi-blast against a target (`chiTarget`/`doChiTarget`,
 * WIS-flavored damage), with `chi self` a defensive mana-refill +
 * temporary cold-immunity buff, `chi all` a room-wide AOE version of
 * the same attack, and `chi <object>` a separate object-targeted
 * effect. Corrected the roster flavor text below to match.
 *
 * Scoped down to `chi <target>` only -- the real self/room/object
 * variants all key off mana (self refills it, room/object spend it),
 * and Tobin has no mana pool at all (casting/praying is component-
 * consumption-based instead, see cmd_cast.c/cmd_pray.c's own doc
 * comments) -- there's no resource left to refill, spend, or gate an
 * AOE's spam-prevention cooldown on. No existing Tobin skill has an
 * AOE-attack shape to extend either. A `self`/`all`/object target
 * would each need their own from-scratch design; cut for now, same
 * "disclosed scope cut" spirit as drug.c's opium/frogslime deviations.
 *
 * Real `doChi()` defaults to your current opponent when no target is
 * given, otherwise resolves one from the room -- `combat_find_room_
 * target()` already carries the PK-consent/linkdead-exclusion logic
 * every other targeted attack uses (cmd_attack.c), so this reuses it
 * rather than duplicating that gate. Usable whether or not you're
 * already fighting, same as berserk/rally -- Monks can open with chi
 * as easily as throw it mid-fight, unlike an opener-only skill like
 * backstab. Damage is WIS-scaled (the real upstream's own chiRoom()
 * damage formula factors in STAT_WIS too) rather than the STR/DEX
 * placeholder formula every physical skill-combat command uses --
 * chi is Monk's one mystical, not physical, attack. No separate
 * "still recovering from your last projection" cooldown is ported --
 * that gate only exists on the real self/room variants (guarding
 * against AOE spam), not on a single-target chiTarget() call, so
 * omitting it isn't a scope cut, it's a faithful match. */
bool cmd_chi(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "chi")) {
        descriptor_send(d, "You lack the ability to chi.\r\n");
        return true;
    }

    being_t *target = NULL;
    char raw[64];
    if (sscanf(args, "%63s", raw) == 1) {
        target = combat_find_room_target(ch, raw);
    } else if (ch->fighting) {
        target = ch->fighting;
    }
    if (!target) {
        /* Also covers typing your own name -- combat_find_room_target()
         * excludes self from its search same as it does for attack/kill,
         * so this refusal doubles as "you can't chi yourself" without a
         * separate check. */
        descriptor_send(d, "Focus your chi on whom?\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "chi", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    if (!ch->fighting) {
        ch->fighting = target;
        target->fighting = ch;
        ch->sneaking = false;
        target->sneaking = false;
    }
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You fail to harm %s with your blast of chi!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s fails to harm you with a blast of chi!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You unleash your chi upon %s!\r\n", being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s unleashes chi force upon you, causing extreme pain!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 1 + (ch->attrs.wisdom - ATTR_BASE) / 4 + rand() % 4;
    if (dmg < 1)
        dmg = 1;
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    return true;
}
