/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
 #include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"
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

    /* `feign death` (level-25 audit batch) -- attacking breaks the act,
     * same as the being.h doc comment promises. */
    d->character->feigning = false;

    /* `fear` (level-5+ audit list) -- a feared being can't turn around
     * and swing back while it's active, same "can't act" spirit as the
     * sleeping check above. */
    if (being_has_affect(d->character, AFFECT_FEAR)) {
        descriptor_send(d, "You're too afraid to fight!\r\n");
        return true;
    }

    /* `transfix` (Druid, Tier-2 port 2026-08-16) -- a transfixed being is
     * mesmerized and rooted in place, unable to act, same "can't act"
     * gate shape as the fear check just above. */
    if (being_has_affect(d->character, AFFECT_TRANSFIX)) {
        descriptor_send(d, "You're transfixed -- you can only stand and stare!\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(d->character, args);
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }

    /* POSITION_MOUNTED is exempt -- fighting from horseback doesn't
     * silently dismount you (Mount / riding system, Sneezy → Tobin
     * feature audit; Sneezy's own combat allows fighting mounted too). */
    if (d->character->position != POSITION_STANDING && d->character->position != POSITION_MOUNTED) {
        d->character->position = POSITION_STANDING;
        descriptor_send(d, "You scramble to your feet.\r\n");
    }

    being_break_hiding(d->character);
    d->character->fighting = target;
    target->fighting = d->character;
    d->character->sneaking = false;
    target->sneaking = false;
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
