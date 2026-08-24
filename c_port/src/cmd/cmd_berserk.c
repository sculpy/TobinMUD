/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"

/* `berserk` (spell/skill functional-completeness audit, 2026-07-27:
 * Warrior roster entry "Forgo defense for a burst of offense -- but
 * you're much harder to rescue or parry while raging.", skill.c level
 * 1, SKILL_TIER_COMBAT). Same one-skill_roll_success()-roll shape as
 * bash/kick, but applies a new plain flag/timer AFFECT_BERSERK (4
 * rounds) instead of dealing damage -- the roster description's own two
 * effects are wired directly where they belong: combat.c's parry check
 * skips itself entirely against a berserking attacker (their hits can't
 * be parried), and cmd_rescue.c refuses to let anyone rescue a
 * berserking ally. 8 rounds -- long enough to matter for a whole fight,
 * same duration as rally's buff (cmd_rally.c). Usable whether or not
 * you're already fighting
 * (unlike a mid-fight-only skill like disarm) -- a Warrior psyching
 * themselves up before a fight is as valid as raging mid-swing. */
bool cmd_berserk(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "berserk")) {
        descriptor_send(d, "You don't know how to berserk.\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_BERSERK)) {
        descriptor_send(d, "You're already in a berserk rage!\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "berserk", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    if (!success) {
        descriptor_send(d, "You try to work yourself into a rage, but can't quite get there.\r\n");
        return true;
    }

    being_apply_affect(ch, AFFECT_BERSERK, 8);
    descriptor_send(d, "<r>You fly into a berserk rage!<z>\r\n");
    if (ch->base.roomp) {
        char msg[128], capbuf[128];
        snprintf(msg, sizeof(msg), "%s flies into a berserk rage!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}
