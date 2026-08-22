/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
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

/* `taunt <target>` (spell/skill functional-completeness audit continued,
 * level 22: Warrior roster "Provoke a target into focusing their
 * aggression on you."). Real upstream (disc_warrior_brawling.cc's
 * doTaunt()) only works against a mob you're ALREADY fighting, and its
 * whole effect is a temporary debuff to that mob's "wimp switch" score --
 * making an AI less likely to swap off you onto a weaker-looking ally
 * mid-fight (misc/ai_reactions.cc's aiWimpSwitch()). Tobin has no such
 * multi-attacker mob-AI target-switching subsystem at all -- `fighting`
 * is a strict mutual one-to-one pointer pair (being.h), so a mob can
 * never be "deciding" between several attackers in the first place.
 * Ported instead as a direct, useful match for the roster's own plain-
 * English description: an aggro PULL, same shape as cmd_rescue.c's
 * fighting-pointer swap, but framed the opposite way around -- rescue
 * frees a threatened ally from a fight they're already in; taunt lets
 * you steal a mob's attention away from whatever/whoever it's currently
 * fighting (ally or stranger) onto yourself instead, with no requirement
 * that you were already involved. Restricted to mob targets (aggro is a
 * mob-AI concept; redirecting a PC's fight without consent would be a
 * PvP mechanic Tobin doesn't otherwise have -- see rescue's own no-PvP-
 * implication precedent). */
bool cmd_taunt(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Taunt whom?\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "taunt")) {
        descriptor_send(d, "You don't know how to taunt anyone.\r\n");
        return true;
    }

    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);
    size_t len = strlen(tok);

    being_t *target = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        if (thing_name_matches(t->name, tok, len)) {
            seen++;
            if (seen == ordinal) {
                target = (being_t *)t;
                break;
            }
        }
    }
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't taunt anyone while you're fighting someone else!\r\n");
        return true;
    }
    being_t *attacker = target->fighting;
    if (!attacker) {
        descriptor_send(d, "They aren't fighting anyone for you to provoke.\r\n");
        return true;
    }
    if (attacker == ch) {
        descriptor_send(d, "They're already fighting you!\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "taunt", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* Same LAG_3-equivalent as rescue -- a full committed action. */
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You taunt %s, but they ignore you completely!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }

    attacker->fighting = NULL;
    if (attacker->position == POSITION_FIGHTING || attacker->position == POSITION_ENGAGED)
        attacker->position = POSITION_STANDING;
    target->fighting = ch;
    ch->fighting = target;

    snprintf(msg, sizeof(msg), "You taunt %s ruthlessly, drawing their ire onto yourself!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (attacker->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s taunts %s, pulling its attention off you!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), being_display_name(target));
        descriptor_send(attacker->desc, msg);
    }
    return true;
}
