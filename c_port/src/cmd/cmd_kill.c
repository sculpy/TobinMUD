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

    combat_instakill(d->character, target);
    return true;
}
