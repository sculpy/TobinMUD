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

/* `throatslit <target>` (spell/skill functional-completeness audit,
 * 2026-07-27: Thief roster entry "A lethal sneak attack targeting the
 * throat.", skill.c level 1, SKILL_TIER_COMBAT). Checked the real
 * upstream's doThroatSlit()/throatSlitHit() (disc/disc_thief_murder.cc)
 * first: it requires a wielded piercing/slicing weapon and rolls a
 * separate willKill() check that can outright kill the victim in one
 * hit -- Tobin has no per-weapon damage-dice table or a willKill()-
 * style "would this hit be lethal" pre-check to port faithfully (same
 * gap noted in cmd_backstab.c's own header comment). Scoped down to the
 * same "opener before either side is fighting" shape as backstab, but
 * hitting even harder (x6 vs backstab's x4, matching the roster's own
 * "lethal" framing as the more dangerous of the two openers) --
 * plain combat_apply_skill_damage(), no separate instant-death roll.
 * A failed roll still reveals the attacker and starts a normal fight at
 * no damage, same "attempt made, detection guaranteed" spirit as a
 * botched backstab. */
bool cmd_throatslit(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Slit whose throat?\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "throatslit")) {
        descriptor_send(d, "You don't know how to do that.\r\n");
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

    const skill_def_t *sk = skill_find(ch->char_class, "throatslit", imm);
    bool success = imm || target->position == POSITION_SLEEPING
                   || (sk && skill_roll_success(skill_learn_from_doing(ch, sk)));

    ch->fighting = target;
    target->fighting = ch;
    ch->sneaking = false;
    target->sneaking = false;
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You lunge for %s's throat, but they sense you coming!\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s lunges for your throat, but you sense them coming!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    snprintf(msg, sizeof(msg), "You slice open %s's throat!\r\n", being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s slices open your throat!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_send(target->desc, msg);
    }

    int dmg = 6 * (1 + (ch->attrs.strength - ATTR_BASE) / 4 + rand() % 4);
    if (dmg < 6)
        dmg = 6;
    combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    return true;
}
