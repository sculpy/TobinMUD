/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "obj.h"
#include "pulse.h"
#include "skill.h"

/* `fortify` (Unimplemented skills/spells backlog, Session 158 audit:
 * Warrior "fortify", skill.c level 1). Ports upstream cmd/cmd_fortify.cc
 * faithfully: a shield-only defensive maneuver that raises a shield wall,
 * cutting incoming damage for a short spell. Upstream lands an
 * APPLY_PROTECTION affect (reduce damage taken); Tobin gets its own
 * dedicated AFFECT_FORTIFY (a flat FORTIFY_DAM_PCT cut in combat_strike(),
 * distinct from the Cleric protection-from family's AFFECT_PROTECTION so
 * the two never collide on one being's affect slots). One
 * skill_roll_success() roll, same learn-by-doing shape as bash/berserk.
 * The affect itself is the recast gate -- you can't re-brace while
 * already braced -- standing in for upstream's separate 1-mud-hour
 * lockout (Tobin has no second cooldown affect for it). */

/* Same "shields are OBJ_CAT_ARMOR held[] items matched by the 'shield'
 * keyword" idiom cmd_bash.c's wielding_shield() uses -- reproduced here
 * rather than exported, both files being the only two that need it. */
static bool ci_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle || !*needle)
        return false;
    size_t hlen = strlen(haystack), nlen = strlen(needle);
    if (nlen > hlen)
        return false;
    for (size_t i = 0; i + nlen <= hlen; i++)
        if (strncasecmp(haystack + i, needle, nlen) == 0)
            return true;
    return false;
}

static bool wielding_shield(const being_t *ch) {
    for (int i = 0; i < 2; i++) {
        obj_t *o = ch->held[i];
        if (o && ci_contains(o->base.name, "shield"))
            return true;
    }
    return false;
}

bool cmd_fortify(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "fortify")) {
        descriptor_send(d, "You know nothing about advanced defensive maneuvers.\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_FORTIFY)) {
        descriptor_send(d, "You're already braced behind your shield.\r\n");
        return true;
    }
    if (!imm && !wielding_shield(ch)) {
        descriptor_send(d, "You cannot execute this defensive maneuver without a shield equipped!\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "fortify", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char capbuf[128], echo[160];
    if (!success) {
        descriptor_send(d, "You attempt to fortify your defenses but fumble the maneuver.\r\n");
        if (ch->base.roomp) {
            snprintf(echo, sizeof(echo), "%s fumbles a defensive maneuver.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_room_echo(ch->base.roomp, ch, echo);
        }
        return true;
    }

    being_apply_affect(ch, AFFECT_FORTIFY, FORTIFY_DURATION_ROUNDS);
    descriptor_send(d, "You sink in behind your shield and brace against incoming attacks!\r\n");
    if (ch->base.roomp) {
        snprintf(echo, sizeof(echo), "%s raises %s shield and strikes a defensive posture!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), gender_possess(ch->gender));
        descriptor_room_echo(ch->base.roomp, ch, echo);
    }
    return true;
}
