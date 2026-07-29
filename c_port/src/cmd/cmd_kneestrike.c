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

/* `kneestrike` (Warrior, level 25, level-25 audit batch: "A knee strike
 * -- unusable while crawling."). A single bonus-damage attack against
 * the caster's current opponent, same "one skill_roll_success() roll,
 * bonus combat_apply_skill_damage() hit" shape every other simple
 * combat skill in this audit uses (bash/kick precedent, cmd_disarm.c's
 * own doc comment). The roster's "unusable while crawling" note is
 * about the WARRIOR's own position, not the target's -- refuses if the
 * caster themselves is in POSITION_CRAWLING. */
bool cmd_kneestrike(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Kneestrike whom? You're not fighting anyone.\r\n");
        return true;
    }
    if (ch->position == POSITION_CRAWLING) {
        descriptor_send(d, "You can't manage a knee strike while crawling.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "kneestrike")) {
        descriptor_send(d, "You don't know how to kneestrike anyone.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "kneestrike", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You drive your knee at %s, but miss!\r\n", being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }

    int dmg = 3 + ch->progress.level / 4 + (rand() % 6);
    limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
    int limb_hp_before = target->limbs[limb].hp;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, limb);
    const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
    snprintf(msg, sizeof(msg), "You drive your knee into %s %s!\r\n", being_display_name(target), intensity);
    descriptor_send(d, msg);
    if (!defeated && target->desc) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "%s drives their knee into you %s!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), intensity);
        descriptor_notify(target->desc, msg);
    }
    return true;
}
