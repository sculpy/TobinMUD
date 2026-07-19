/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "regen.h"

#include "being.h"
#include "descriptor.h"

/* Placeholder regen formula: 1 HP baseline + 1 per 20 points of
 * constitution above ATTR_BASE. Not the original's level/CON/room-driven
 * hitGain() curve (misc/limits.cc) -- revisit once a real growth curve is
 * designed, same caveat as being_calc_max_hp(). */
static int regen_amount(const being_t *b) {
    int bonus = (b->attrs.constitution - ATTR_BASE) / 20;
    int amount = 1 + bonus;
    if (amount < 1)
        amount = 1;
    /* Rest and sleep speed healing (original hitGain() weights by position);
     * sitting a little, standing none. */
    if (b->position == POSITION_SLEEPING)
        amount *= 3;
    else if (b->position == POSITION_RESTING)
        amount *= 2;
    else if (b->position == POSITION_SITTING)
        amount += amount / 2;
    return amount;
}

void regen_tick_run(long pulse_num) {
    (void)pulse_num;
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b || b->fighting)
            continue;
        being_heal(b, regen_amount(b));
        /* Vitality (Sneezy → Tobin feature audit, "Vitality stat +
         * Terrain movement cost"): same weight-by-position amount as HP,
         * per TODO.md's own note this item closed out ("the regen tick
         * (weight by position, like HP already does)"). Still gated on
         * not fighting -- resting up after combat is when both HP and
         * legs recover. */
        being_heal_vit(b, regen_amount(b));
    }
}
