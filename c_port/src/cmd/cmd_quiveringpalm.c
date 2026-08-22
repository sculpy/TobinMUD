/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "room.h"
#include "skill.h"

/* `quiveringpalm` (Monk, level 42 advanced spec-proc) -- the fabled death
 * touch. Faithful port of upstream quiveringPalm() (cmd_quivpalm.cc): a
 * successful skill roll deals 100 + the victim's max HP, a guaranteed kill
 * against any normal (<=lvl 60) foe -- their bones and organs shatter
 * inside. Two disclosed divergences forced by Tobin's model:
 *   - No mana cost: upstream spends 100 mana ("chi"), but Tobin monks have
 *     no mana pool at all (being.c; cmd_chi.c), so the recast cooldown
 *     (AFFECT_QUIVERING_PALM_COOLDOWN) is the sole balancing gate.
 *   - No humanoid/peaceful flags: Tobin mobs carry neither, so those
 *     upstream guards drop; the immortal-target and self-target refusals
 *     and the both-arms-hurt refusal (bothArmsHurt()) are kept.
 * Target is the monk's current opponent, or a named being in the room
 * (combat_find_room_target(), same acquisition as cmd_attack.c). */
bool cmd_quiveringpalm(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "quivering palm")) {
        descriptor_send(d, "You don't know the secret of the quivering palm.\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_QUIVERING_PALM_COOLDOWN)) {
        descriptor_send(d, "You are not yet centered enough to attempt this maneuver again.\r\n");
        return true;
    }
    if (ch->limbs[LIMB_LEFT_ARM].hp <= 0 && ch->limbs[LIMB_RIGHT_ARM].hp <= 0) {
        descriptor_send(d, "At least one of your arms needs to work to try to quiver!\r\n");
        return true;
    }
    being_t *target = NULL;
    while (*args == ' ')
        args++;
    if (*args)
        target = combat_find_room_target(ch, args);
    else
        target = ch->fighting;
    if (!target) {
        descriptor_send(d, "Use the fabled quivering palm on whom?\r\n");
        return true;
    }
    if (target == ch) {
        descriptor_send(d, "This is not an approved method for committing ritual suicide.\r\n");
        return true;
    }
    if (being_is_immortal(target)) {
        descriptor_send(d, "You decide not to waste your concentration on an immortal.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "quivering palm", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, COMBAT_ROUND_PULSES);
    descriptor_send(d, "You begin to work on the vibrations...\r\n");

    char msg[192], capbuf[128];
    if (!success) {
        being_apply_affect(ch, AFFECT_QUIVERING_PALM_COOLDOWN, QUIV_COOLDOWN_FAIL_ROUNDS);
        snprintf(msg, sizeof(msg), "You touch %s, but the vibrations fade ineffectively.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "%s touches you, but nothing seems to happen.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_notify(target->desc, msg);
        }
        return true;
    }

    being_apply_affect(ch, AFFECT_QUIVERING_PALM_COOLDOWN, QUIV_COOLDOWN_ROUNDS);
    int dmg = 100 + target->progress.max_hp;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, LIMB_BODY);
    if (defeated)
        snprintf(msg, sizeof(msg), "%s is slain instantly by the dreaded quivering palm!\r\n",
                 being_display_name(target));
    else
        snprintf(msg, sizeof(msg), "%s is heinously wounded by the dreaded quivering palm!\r\n",
                 being_display_name(target));
    descriptor_send(d, msg);
    if (!defeated && target->desc) {
        snprintf(msg, sizeof(msg), "As %s touches you, your bones and organs shatter inside!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_notify(target->desc, msg);
    }
    return true;
}
