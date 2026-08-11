/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `switch opponents <target>` (Warrior/Thief/Monk, level 25, level-25
 * audit batch: "Change which opponent you're actively fighting."). Real
 * upstream matters for multi-attacker fights; Tobin's `fighting` is a
 * strict mutual 1:1 pointer pair (being.h), so "switching" here means:
 * cleanly break the current fight (both sides' `fighting` cleared) and
 * immediately open a new one against the named target instead, same
 * mutual-pointer-set shape `cmd_attack.c` uses to start a fight in the
 * first place. Mob targets only, same PvP-consent precedent
 * cmd_taunt.c's own doc comment established (redirecting your OWN
 * fight onto a new, unconsenting PC would be a PvP mechanic). No lag
 * cost beyond the normal combat round -- this is a battlefield
 * judgment call, not a technique with its own wind-up. */
bool cmd_switchopp(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!ch->fighting) {
        descriptor_send(d, "You're not fighting anyone to switch from.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Switch to which opponent?\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "switch opponents")) {
        descriptor_send(d, "You don't know how to switch opponents mid-fight.\r\n");
        return true;
    }

    size_t len = strlen(raw);
    being_t *target = NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        if (thing_name_matches(t->name, raw, len)) {
            target = (being_t *)t;
            break;
        }
    }
    if (!target) {
        descriptor_send(d, "You don't see them here.\r\n");
        return true;
    }
    if (target == ch->fighting) {
        descriptor_send(d, "You're already fighting them.\r\n");
        return true;
    }

    being_t *old = ch->fighting;
    old->fighting = NULL;
    ch->fighting = target;
    target->fighting = ch;

    char msg[160];
    snprintf(msg, sizeof(msg), "You break off from %s and turn on %s instead!\r\n",
             being_display_name(old), being_display_name(target));
    descriptor_send(d, msg);
    /* Learn-by-doing: using the skill trains it toward its discipline
     * ceiling (skill_learn_from_doing() self-throttles via its own
     * cooldown). PCs only; immortals already read as maxed. */
    if (!being_is_immortal(ch) && ch->base.kind == THING_PC) {
        const skill_def_t *learn_sk = skill_find(ch->char_class, "switch opponents", true);
        if (learn_sk)
            skill_learn_from_doing(ch, learn_sk);
    }
    return true;
}
