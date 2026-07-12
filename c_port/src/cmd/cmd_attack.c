/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
 #include "cmd_internal.h"

#include <stdio.h>

#include "combat.h"
#include "pulse.h"

/* Initiates (or re-targets) an attack against another player OR a mobile
 * (Phase 2D) in the same room -- combat_find_room_target() matches both
 * kinds. Damage itself is NOT dealt here: this just sets up the fight and
 * applies the initial wait; combat_process_run() (src/core/combat.c)
 * resolves the actual exchanges once every COMBAT_ROUND_PULSES. Also
 * reached directly by cmd_kill.c for mortals -- `kill` behaves identically
 * to `attack` unless the attacker is immortal (see cmd_kill.c). */
bool cmd_attack(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Attack whom?\r\n");
        return true;
    }

    if (d->character->position == POSITION_SLEEPING) {
        descriptor_send(d, "You can't fight in your sleep!\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(d->character, args);
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }

    if (d->character->position != POSITION_STANDING) {
        d->character->position = POSITION_STANDING;
        descriptor_send(d, "You scramble to your feet.\r\n");
    }

    d->character->fighting = target;
    target->fighting = d->character;
    being_set_wait(d->character, COMBAT_ROUND_PULSES);

    /* target->base.name is a mob's raw keyword list ("lady stroll walk"),
     * not a display string, if target is a mob -- being_display_name()
     * picks short_descr instead (same bug class found in the 2026-07-11
     * capitalization audit; d->character below is always a connected PC,
     * never a mob, so it needs no such guard). */
    char msg[128];
    snprintf(msg, sizeof(msg), "You attack %s!\r\n", being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s attacks you!\r\n", d->character->base.name);
        descriptor_notify(target->desc, msg);
    }

    return true;
}
