/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `whirlwind` (Warrior, level 25, level-25 audit batch: "A spinning
 * attack that can strike every opponent in the room."). Tobin's
 * `fighting` pointer is a strict mutual 1:1 pair (being.h) -- there's no
 * "everyone hostile in the room" concept to iterate the way the real
 * upstream's multi-attacker-aware combat does. Scoped down to hit the
 * caster's current opponent plus every OTHER mob in the room (PCs are
 * deliberately excluded -- turning a self-buff-flavored AoE into an
 * unconsented PvP move would be a mechanic Tobin doesn't otherwise have,
 * same "no unconsented PvP" precedent cmd_taunt.c's own doc comment
 * already established). One skill_roll_success() roll per target, same
 * "no multi-stage gauntlet" convention cmd_disarm.c's doc comment
 * established for this whole audit pass. */
bool cmd_whirlwind(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!ch->fighting) {
        descriptor_send(d, "You're not fighting anyone to whirlwind.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "whirlwind")) {
        descriptor_send(d, "You don't know how to whirlwind.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "whirlwind", imm);
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    descriptor_send(d, "You spin in a whirlwind of steel!\r\n");
    char msg[160];
    if (ch->base.roomp) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s spins in a whirlwind of steel!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }

    int hits = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB && t->kind != THING_PC)
            continue;
        being_t *victim = (being_t *)t;
        if (victim == ch)
            continue;
        /* Every mob in the room is fair game; a PC only if it's the
         * caster's own current opponent (no unconsented PvP -- see this
         * function's own doc comment). */
        if (victim->base.kind == THING_PC && victim != ch->fighting)
            continue;
        bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
        if (!success)
            continue;
        int dmg = 3 + ch->progress.level / 4 + (rand() % 6);
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = victim->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, victim, dmg, limb);
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "Your spinning blade catches %s %s!\r\n",
                 being_display_name(victim), intensity);
        descriptor_send(d, msg);
        if (!defeated && victim->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s's spinning blade catches you %s!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)), intensity);
            descriptor_notify(victim->desc, msg);
        }
        hits++;
    }
    if (!hits)
        descriptor_send(d, "...but your blade meets nothing but air.\r\n");
    return true;
}
