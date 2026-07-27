/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "skill.h"
#include "thing.h"

/* `backstab <target>` (spell/skill functional-completeness audit,
 * 2026-07-27: Thief roster entry "A devastating sneak attack against an
 * unaware or from-behind target.", skill.c level 1, SKILL_TIER_COMBAT).
 * Sneezy's real doBackstab() requires a bladed piercing weapon and a
 * multiplier off the weapon's own dice; Tobin has no per-weapon damage
 * dice table yet (same gap noted in cmd_bash.c's own header comment), so
 * this reuses combat_apply_skill_damage()'s STR-flavored placeholder
 * formula like every other skill-combat command, just scaled up (x4) to
 * read as the single hardest-hitting opener in the roster, matching
 * backstab's real-game reputation. The "unaware" gate is what makes this
 * distinct from bash/kick/trip: it only works to OPEN a fight (Sneezy:
 * bonus is lost entirely once the target is alerted), so it's refused
 * outright if either side is already fighting anyone -- initiates combat
 * itself on success (cmd_attack.c's own fighting-pointer-swap shape)
 * rather than requiring `attack` first. A failed roll still reveals the
 * attacker and starts a normal fight at no damage, same "attempt made,
 * detection guaranteed" spirit as a botched Sneezy backstab. */
bool cmd_backstab(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Backstab whom?\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "backstab")) {
        descriptor_send(d, "You don't know how to backstab.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You're already in a fight -- the element of surprise is gone.\r\n");
        return true;
    }

    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);
    size_t len = strlen(tok);

    being_t *target = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (t == &ch->base)
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
    if (target->fighting) {
        descriptor_send(d, "They're already alert and fighting -- you can't catch them off guard.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "backstab", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    ch->fighting = target;
    target->fighting = ch;
    ch->sneaking = false;
    target->sneaking = false;
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You lunge at %s, but they sense you coming!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s lunges at you, but you sense them coming!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You plunge your blade into %s's back!\r\n",
             being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s plunges a blade into your back!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 4 * (1 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 4);
    if (dmg < 4)
        dmg = 4;
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    return true;
}
