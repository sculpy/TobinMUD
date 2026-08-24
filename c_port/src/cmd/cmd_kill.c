/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "combat.h"

/* For mortals, `kill` is identical to `attack` -- just falls through to it.
 * For an immortal (level >= IMMORTAL_LEVEL_MIN), `kill <target>` instead
 * bypasses the multi-round combat process entirely and kills the target
 * instantly. Mirrors the original's doKill() (misc/offense.cc), which
 * calls doHit() (a normal attack) unless the caller has the POWER_SLAY
 * wiz-power, in which case it's an instant TBeing::rawKill(). Tobin has no
 * wiz-power system yet, so this simplifies that gate to being_is_immortal()
 * -- the same level-51 threshold the original's POWER_SLAY holders are
 * drawn from (GOD_LEVEL1 == 51 in misc/defs.h). */
bool cmd_kill(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Kill whom?\r\n");
        return true;
    }

    if (!being_is_immortal(d->character))
        return cmd_attack(d, args);

    being_t *target = combat_find_room_target(d->character, args);
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }

    /* Immortal-vs-immortal guard (TODO backlog): can't instakill an equal-
     * or higher-ranked immortal PC. Compares TRUE rank (progress.true_level
     * when set, see cmd_mortal.c) on both sides, so a target who's toggled
     * mortal (`immort`) is still protected by their real rank, not their
     * currently-lowered one -- courtesy between staff, not a loophole. Mobs
     * aren't gated here; this is about immortal peers, not monsters. */
    if (target->base.kind == THING_PC) {
        int my_rank = d->character->progress.level;
        int their_rank = target->progress.true_level >= IMMORTAL_LEVEL_MIN
                             ? target->progress.true_level
                             : target->progress.level;
        if (their_rank >= my_rank) {
            descriptor_send(d, "You cannot slay an equal or higher-ranked immortal.\r\n");
            return true;
        }
    }

    combat_instakill(d->character, target);
    return true;
}
