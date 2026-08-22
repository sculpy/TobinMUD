/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
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
#include "thing.h"

/* `turn <undead>` -- the Cleric's turn-undead ability (spell/skill audit,
 * "Generic / cross-class": "Turn undead (command `turn`)"). Ported from
 * Sneezy's TBeing::doTurn() (misc/magic_skills.cc): a Cleric channels holy
 * will against a minion of darkness -- one of the undead, or a demon/devil
 * (Sneezy: isUndead() || isDiabolic()). On a successful skill roll it sears
 * the creature for level-scaled holy damage and sends it fleeing in terror;
 * on a failure the attempt fizzles but still provokes the fight.
 *
 * Faithful trims: Sneezy's own doTurn() has an elaborate graded outcome
 * ladder (instant destruction / blind / stun at very low rolls) but it is
 * entirely `#if 0`'d out in the real source -- the LIVE upstream behavior
 * is exactly the simple attempt->damage-or-fizzle this implements. Demons
 * resist harder than the undead (Sneezy: +50 to the roll-against percent),
 * modeled here as a flat proficiency penalty. The holy-symbol grope Sneezy
 * leaves as a code comment ("check here if cleric has a symbol") was never
 * actually implemented upstream, so turn needs no reagent here either.
 *
 * Same "extra action layered on the automatic round" + "can also OPEN a
 * fight" shape as cmd_kick.c (see that header for the full rationale). */
bool cmd_turn(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "turn undead")) {
        descriptor_send(d, "You don't know how to turn the undead.\r\n");
        return true;
    }

    /* Resolve the target: current opponent if already fighting, else the
     * named creature (which we may then open a fight against). */
    being_t *target = ch->fighting;
    bool opening = false;
    if (!target) {
        char tok[64];
        if (sscanf(args, "%63s", tok) != 1) {
            descriptor_send(d, "Attempt to turn which minion of darkness?\r\n");
            return true;
        }
        if (ch->position == POSITION_SLEEPING) {
            descriptor_send(d, "You can't channel your faith in your sleep!\r\n");
            return true;
        }
        if (being_has_affect(ch, AFFECT_FEAR)) {
            descriptor_send(d, "You're too afraid to focus!\r\n");
            return true;
        }
        target = combat_find_room_target(ch, tok);
        if (!target) {
            descriptor_send(d, "They aren't here.\r\n");
            return true;
        }
        opening = true;
    }

    if (target == ch) {
        descriptor_send(d, "Now that would be quite pointless...\r\n");
        return true;
    }

    /* Only the undead and demonkind can be turned. */
    mob_lore_t cat = (target->base.kind == THING_MOB)
                         ? mob_race_lore_category(target->mob_race)
                         : LORE_PEOPLE;
    bool is_demon = (cat == LORE_DEMON);
    if (cat != LORE_UNDEAD && !is_demon) {
        descriptor_send(d, "They are not a minion of darkness!\r\n");
        return true;
    }

    /* Commit to the fight now that the target is valid. */
    if (opening) {
        if (ch->position != POSITION_STANDING && ch->position != POSITION_MOUNTED) {
            ch->position = POSITION_STANDING;
            descriptor_send(d, "You rise to your feet.\r\n");
        }
        ch->feigning = false;
        ch->fighting = target;
        target->fighting = ch;
        ch->sneaking = false;
        target->sneaking = false;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "turn undead", imm);
    int prof = sk ? skill_learn_from_doing(ch, sk) : 100;
    int effective = prof;
    if (is_demon)
        effective -= 40; /* Sneezy: diabolics resist a turn far harder */
    if (effective < 0)
        effective = 0;
    bool success = imm || !sk || skill_roll_success(effective);

    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[192], capbuf[128];
    if (!success) {
        snprintf(msg, sizeof(msg),
                 "You call upon your faith to turn %s, but your will falters.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg),
                     "%s tries to turn you, but falters.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg),
             "You raise your holy will -- %s recoils, seared and terrified!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg),
                 "%s raises a searing holy will -- you recoil in terror!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    /* Holy damage scaled by the caster's level (Sneezy: reconcileDamage of
     * roughly getLevel()); demons take the sear but shrug off more of it. */
    int lvl = ch->progress.level;
    int dmg = lvl + rand() % (lvl + 1);
    if (is_demon)
        dmg = (dmg * 2) / 3;
    if (dmg < 1)
        dmg = 1;

    /* Send it fleeing (Sneezy: addFeared on a successful turn). */
    being_apply_affect(target, AFFECT_FEAR, 3);
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    return true;
}
