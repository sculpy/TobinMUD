/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `rescue <ally>` (spell/skill functional-completeness audit, 2026-07-27:
 * Warrior roster entry "Swap places with an ally in combat, pulling their
 * attacker onto yourself.", skill.c level 1, SKILL_TIER_CLASS). Sneezy's
 * real doRescue() is a hit-roll-gated swap with its own separate lag
 * pool; scoped here to this port's usual one skill_roll_success() roll
 * (same shape as bash/kick/trip). `ally` must be in the same room,
 * currently fighting someone OTHER than the rescuer, and that someone
 * must be the rescuer's own opponent-to-be -- on success, every
 * `fighting` pointer swaps so the attacker now targets the rescuer
 * instead, and the ally is freed to a non-fighting position (Sneezy: the
 * rescued party stops actively fighting; scoped down to POSITION_STANDING
 * here since Tobin has no separate "assist" positional state). Refuses a
 * self-target and an ally who isn't actually threatened. */
bool cmd_rescue(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Rescue whom?\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "rescue")) {
        descriptor_send(d, "You don't know how to rescue anyone.\r\n");
        return true;
    }

    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);
    size_t len = strlen(tok);

    being_t *ally = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (t == &ch->base)
            continue;
        if (thing_name_matches(t->name, tok, len)) {
            seen++;
            if (seen == ordinal) {
                ally = (being_t *)t;
                break;
            }
        }
    }
    if (!ally) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (ally == ch->fighting) {
        descriptor_send(d, "You're already fighting them yourself!\r\n");
        return true;
    }
    /* Spell/skill functional-completeness audit (2026-07-27): a
     * berserking ally can't be rescued at all (roster's own "much
     * harder to rescue ... while raging" description, cmd_berserk.c). */
    if (being_has_affect(ally, AFFECT_BERSERK)) {
        descriptor_send(d, "They're in too much of a berserk rage to be pulled out of the fight!\r\n");
        return true;
    }
    being_t *attacker = ally->fighting;
    if (!attacker || attacker->fighting != ally) {
        descriptor_send(d, "They aren't in any danger you can rescue them from.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't rescue anyone while you're fighting someone else!\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "rescue", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    /* Same LAG_3-equivalent as bash/trip -- a full committed action. */
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to rescue %s, but can't get to them in time!\r\n",
                 being_display_name(ally));
        descriptor_send(d, msg);
        return true;
    }

    ally->fighting = NULL;
    if (ally->position == POSITION_FIGHTING || ally->position == POSITION_ENGAGED)
        ally->position = POSITION_STANDING;
    attacker->fighting = ch;
    ch->fighting = attacker;

    snprintf(msg, sizeof(msg), "You leap in to rescue %s, pulling %s's attention onto yourself!\r\n",
             being_display_name(ally), being_display_name(attacker));
    descriptor_send(d, msg);
    if (ally->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s rescues you, taking your attacker's attention!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(ally->desc, msg);
    }
    if (attacker->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s leaps in and rescues %s -- you're now facing them instead!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), being_display_name(ally));
        descriptor_send(attacker->desc, msg);
    }
    return true;
}
