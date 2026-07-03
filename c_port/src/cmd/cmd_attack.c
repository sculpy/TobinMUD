#include "cmd_internal.h"

#include <stdio.h>

#include "combat.h"
#include "pulse.h"

/* Initiates (or re-targets) an attack against another PLAYING character in
 * the same room -- player-vs-player only, no NPCs/mobs exist yet. Damage
 * itself is NOT dealt here: this just sets up the fight and applies the
 * initial wait; combat_process_run() (src/core/combat.c) resolves the
 * actual exchanges once every COMBAT_ROUND_PULSES. Also reached directly by
 * cmd_kill.c for mortals -- `kill` behaves identically to `attack` unless
 * the attacker is immortal (see cmd_kill.c). */
bool cmd_attack(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Attack whom?\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(d->character, args);
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }

    d->character->fighting = target;
    target->fighting = d->character;
    being_set_wait(d->character, COMBAT_ROUND_PULSES);

    char msg[128];
    snprintf(msg, sizeof(msg), "You attack %s!\r\n", target->base.name);
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s attacks you!\r\n", d->character->base.name);
        descriptor_send(target->desc, msg);
    }

    return true;
}
