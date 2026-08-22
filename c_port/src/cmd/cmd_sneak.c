/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include "being.h"
#include "skill.h"

/* `sneak` (spell/skill functional-completeness audit, 2026-07-27:
 * Thief/Warrior roster entry, skill.c level 1, SKILL_TIER_COMBAT). A
 * plain toggle, same shape as cmd_disguise.c -- see being.h's own
 * `sneaking` field comment for the full scope-down rationale (only
 * suppresses your own arrival/departure room echo while moving; broken
 * outright on entering combat, and does NOT hide you from a stationary
 * room's person-listing, which is `hide`'s separate, higher-level job). */
bool cmd_sneak(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "sneak")) {
        descriptor_send(d, "You don't know how to move quietly.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return true;
    }

    ch->sneaking = !ch->sneaking;
    if (ch->sneaking)
        descriptor_send(d, "You start moving quietly, trying not to draw attention.\r\n");
    else
        descriptor_send(d, "You stop sneaking around.\r\n");
    if (ch->sneaking && !being_is_immortal(ch) && ch->base.kind == THING_PC) {
        const skill_def_t *learn_sk = skill_find(ch->char_class, "sneak", true);
        if (learn_sk)
            skill_learn_from_doing(ch, learn_sk);
    }
    return true;
}
