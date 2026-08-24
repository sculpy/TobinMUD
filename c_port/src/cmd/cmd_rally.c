/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"
#include "being.h"
#include "pulse.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `rally` (spell/skill functional-completeness audit, 2026-07-27:
 * Warrior roster entry "A battlecry that boosts nearby allies' combat
 * prowess.", skill.c level 1, SKILL_TIER_COMBAT). The first "buff
 * everyone else in the room" skill in the roster -- reuses the
 * AFFECT_STUPIDITY stat-affect machinery (being_apply_stat_affect(),
 * affect.c's affect_stat_target()) in the positive direction (a
 * STRENGTH bonus, standing in for "combat prowess" -- Tobin has no
 * separate hitroll/damroll stat to target instead). "Nearby allies" is
 * scoped down to "every other PC/mob in the room who isn't the
 * rallier's own current opponent" -- Tobin has no team/faction concept
 * to test against instead, and excluding your own fight's target is the
 * one exclusion that actually matters (you wouldn't rally the person
 * you're trying to beat). Immortals in the room are skipped (a buff
 * would be meaningless against their damage immunity). One
 * skill_roll_success() roll gates whether the rally lands at all, same
 * shape as every other skill-combat command; on success it always
 * reaches every eligible being in the room (no separate per-recipient
 * roll). */
bool cmd_rally(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "rally")) {
        descriptor_send(d, "You don't know how to rally anyone.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "rally", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    if (!success) {
        descriptor_send(d, "You let out a battlecry, but it fails to inspire anyone.\r\n");
        return true;
    }

    int bonus = 3 + ch->progress.level / 10;
    int recipients = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        being_t *ally = (being_t *)t;
        if (ally == ch || ally == ch->fighting)
            continue;
        if (being_is_immortal(ally))
            continue;
        being_apply_stat_affect(ally, AFFECT_RALLY, 8, bonus);
        recipients++;
    }

    descriptor_send(d, "<y>You let out a rousing battlecry!<z>\r\n");
    char msg[128], capbuf[128];
    snprintf(msg, sizeof(msg), "%s lets out a rousing battlecry!\r\n",
             being_display_name_cap(ch, capbuf, sizeof(capbuf)));
    descriptor_room_echo(ch->base.roomp, ch, msg);
    (void)recipients;
    return true;
}
