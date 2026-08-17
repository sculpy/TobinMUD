/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdlib.h>

#include "being.h"
#include "room.h"
#include "skill.h"

/* `hide` (Thief, level 31 advanced) -- full concealment, a step past
 * sneak (which only muffles your movement echo) and feign death (which
 * only dodges NEW mob aggro). A successful skill roll drops you out of the
 * room person-listing for other viewers (cmd_look.c, same immortal/detect
 * gate as AFFECT_INVISIBLE) and is skipped by mob_try_aggress()
 * (mob_ai.c). Hiding is a held stillness: moving or attacking breaks it (being_break_hiding()). Faithful in spirit to upstream doHide
 * (disc_thief_stealth.cc) as a stationary, chance-based stealth roll;
 * Tobin approximates upstream's dedicated sense-hidden counter with the
 * existing detect-invisible gate, there being no separate sense affect.
 * Same one-roll / learn-by-doing shape as cmd_kneestrike.c. */
bool cmd_hide(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "hide")) {
        descriptor_send(d, "You don't know how to hide yourself.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't slip into hiding in the middle of a fight!\r\n");
        return true;
    }
    if (ch->hiding) {
        descriptor_send(d, "You are already hidden.\r\n");
        return true;
    }
    const skill_def_t *sk = skill_find(ch->char_class, "hide", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    if (!success) {
        descriptor_send(d, "You try to hide, but can't find good enough cover.\r\n");
        return true;
    }
    ch->hiding = true;
    descriptor_send(d, "You melt into the shadows, hidden from view.\r\n");
    return true;
}
