/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
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

/* `shoulderthrow` (Monk, missing-skill audit, 2026-08-09). Real
 * upstream help text: "heave an opponent off its feet and toss it onto
 * the ground where it is more vulnerable... requires the Jumandoist be
 * able to successfully hit the victim... must be of roughly the same
 * stature to succeed." Same single-roll bonus-damage-attack shape as
 * `kneestrike`/`bash` (this batch's established precedent for a simple
 * combat skill), with the "more vulnerable on the ground" part ported
 * as knocking the target into POSITION_SITTING -- same mechanism
 * cmd_bash.c's own knockdown already established, reused rather than
 * inventing a second one. The real "same stature" size-gate has no
 * height/size stat to check in Tobin (no body-size field on being_t) --
 * disclosed simplification, dropped rather than faked. */
bool cmd_shoulderthrow(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->fighting) {
        descriptor_send(d, "Shoulder throw whom? You're not fighting anyone.\r\n");
        return true;
    }
    if (ch->position == POSITION_CRAWLING || ch->position == POSITION_SITTING) {
        descriptor_send(d, "You need solid footing to attempt a shoulder throw.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "shoulder throw")) {
        descriptor_send(d, "You don't know how to shoulder throw anyone.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    if (target->position != POSITION_STANDING && target->position != POSITION_FIGHTING
        && target->position != POSITION_ENGAGED && !imm) {
        descriptor_send(d, "They're already down -- there's nothing left to throw.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "shoulder throw", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, COMBAT_ROUND_PULSES);

    char msg[192];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to shoulder throw %s, but they slip free!\r\n", being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }

    int dmg = 3 + ch->progress.level / 4 + (rand() % 6);
    limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
    int limb_hp_before = target->limbs[limb].hp;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, limb);
    const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
    snprintf(msg, sizeof(msg), "You heave %s off their feet and throw them to the ground %s!\r\n",
             being_display_name(target), intensity);
    descriptor_send(d, msg);
    if (!defeated) {
        target->position = POSITION_SITTING;
        if (target->desc) {
            char capbuf[128];
            snprintf(msg, sizeof(msg), "%s heaves you off your feet and throws you to the ground %s!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)), intensity);
            descriptor_notify(target->desc, msg);
        }
    }
    return true;
}
