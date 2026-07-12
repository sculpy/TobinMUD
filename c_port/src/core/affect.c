/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "affect.h"

#include <stdio.h>

#include "being.h"
#include "descriptor.h"

static const char *const AFFECT_NAMES[AFFECT_COUNT] = {
    "(none)",
    "Sanctuary",
};

/* Looks up the display name for an affect type, e.g. for the `affects`
 * command or an expiry message. Falls back to "(unknown)" for a bad
 * value rather than reading out of bounds. */
const char *affect_name(affect_type_t type) {
    if (type < 0 || type >= AFFECT_COUNT)
        return "(unknown)";
    return AFFECT_NAMES[type];
}

/* Checks whether `b` currently has a given buff/debuff active by
 * scanning their small fixed list of active affects. */
bool being_has_affect(const struct being *b, affect_type_t type) {
    if (!b || type == AFFECT_NONE)
        return false;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++)
        if (b->affects[i].type == type)
            return true;
    return false;
}

/* Starts a buff/debuff on `b`, or -- if it's already active -- just
 * resets its remaining duration back to `rounds` (casting the same
 * spell on yourself again refreshes it instead of stacking a second
 * copy). If none of `b`'s affect slots already hold this type and none
 * are free, this is a no-op -- the fixed-size list is deliberately
 * small (MAX_ACTIVE_AFFECTS) rather than growable. */
void being_apply_affect(struct being *b, affect_type_t type, int rounds) {
    if (!b || type == AFFECT_NONE || rounds <= 0)
        return;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (b->affects[i].type == type) {
            b->affects[i].rounds_left = rounds;
            return;
        }
    }
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (b->affects[i].type == AFFECT_NONE) {
            b->affects[i].type = type;
            b->affects[i].rounds_left = rounds;
            return;
        }
    }
    /* All slots full of OTHER affects -- deliberately dropped, not
     * queued; a being juggling more than MAX_ACTIVE_AFFECTS at once is
     * an edge case this v1 doesn't need to solve. */
}

/* Ends a buff/debuff on `b` right away, if they have it -- used when
 * something explicitly cancels an affect (as opposed to it simply
 * running out, which affect_tick_run() handles). */
void being_remove_affect(struct being *b, affect_type_t type) {
    if (!b || type == AFFECT_NONE)
        return;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (b->affects[i].type == type) {
            b->affects[i].type = AFFECT_NONE;
            b->affects[i].rounds_left = 0;
            return;
        }
    }
}

/* Runs on a timer (see main.c) for every connected being: counts each
 * of their active buffs/debuffs down by one round, and clears (with a
 * "wears off" message) any that just hit zero. Applies regardless of
 * whether the being is currently fighting -- a buff keeps wearing off
 * even outside combat. */
void affect_tick_run(long pulse_num) {
    (void)pulse_num;
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b)
            continue;
        for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
            if (b->affects[i].type == AFFECT_NONE)
                continue;
            b->affects[i].rounds_left--;
            if (b->affects[i].rounds_left <= 0) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Your %s wears off.\r\n", affect_name(b->affects[i].type));
                descriptor_send(d, msg);
                b->affects[i].type = AFFECT_NONE;
                b->affects[i].rounds_left = 0;
            }
        }
    }
}
