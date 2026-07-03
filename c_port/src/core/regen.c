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
    return amount < 1 ? 1 : amount;
}

void regen_tick_run(long pulse_num) {
    (void)pulse_num;
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b || b->fighting)
            continue;
        being_heal(b, regen_amount(b));
    }
}
