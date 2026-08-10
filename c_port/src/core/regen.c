/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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

/* Periodic hook (registered with the pulse scheduler) that heals HP and
 * vitality for every connected, non-fighting player by regen_amount(). */
void regen_tick_run(long pulse_num) {
    (void)pulse_num;
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b)
            continue;
        /* Client HP/Mana/Move gauge only ever got pushed on actual
         * damage (combat.c) -- meaning it sat frozen while the player
         * was healing/regenerating outside a fight (user, 2026-08-06:
         * "status bar needs updating per tick"). Fired at the end of
         * each branch below, AFTER this tick's own healing, not before
         * it -- notifying first sent the gauge last tick's numbers,
         * showing the player permanently one tick behind (user,
         * 2026-08-08: "status bar in client is one tick behind"). */
        if (b->fighting) {
            /* User 2026-08-03: Vitality should trickle back some even
             * mid-fight, not just after -- HP still only recovers at
             * rest (combat pacing untouched, so no being_heal() here),
             * but a fighter shouldn't sit flatlined at 0 Vitality for
             * the whole fight once vit_fatigue_accum drains it
             * (combat.c). Base un-multiplied amount only -- position
             * stays STANDING while fighting, so no rest/sleep bonus
             * applies, and no 5/4 "resting up" bump either. */
            being_heal_vit(b, regen_amount(b));
            being_notify_vitals_changed(b);
            continue;
        }
        /* Captured before this tick's healing lands, so the auto-stand
         * check below can tell "just now finished recovering" (was short
         * of full, now full) apart from "sat down while already at full
         * HP/vit and is choosing to stay down" -- without this, a fully
         * healed character who sits (e.g. to fight from the ground for
         * `groundfighting`/similar non-standing-position skills) gets
         * stood back up on the very next regen tick, before anyone can
         * ever land a hit on them while down (missing-skill audit,
         * 2026-08-09: groundfighting's learn-by-doing hook was
         * unreachable in live play for exactly this reason). */
        bool was_short_hp = b->progress.hp < b->progress.max_hp;
        bool was_short_vit = b->progress.vit < b->progress.max_vit;
        being_heal(b, regen_amount(b));
        /* Vitality (Sneezy → Tobin feature audit, "Vitality stat +
         * Terrain movement cost"): same weight-by-position amount as HP,
         * per TODO.md's own note this item closed out ("the regen tick
         * (weight by position, like HP already does)"). Still gated on
         * not fighting -- resting up after combat is when both HP and
         * legs recover. Bumped 25% over the shared HP amount (user,
         * 2026-08-03: "vitality gains too slow, adjust it up 25%") --
         * HP's own rate is untouched, only vitality's. */
        int vit_amount = regen_amount(b) * 5 / 4;
        if (vit_amount < 1)
            vit_amount = 1;
        being_heal_vit(b, vit_amount);

        /* Mana (user 2026-08-06: "add mana to prompt" -- once the pool
         * itself shipped, it needs a regen tick same as HP/Vitality).
         * being_heal_mana() is already a no-op for max_mana == 0, so
         * this line costs nothing for the classes that don't have one. */
        being_heal_mana(b, regen_amount(b));

        /* User 2026-08-03: "when completely rested ... you should
         * automatically stand" -- reaching full HP AND vitality while
         * resting/sitting/sleeping (not already standing) stands the
         * character back up on their own, same spirit as meditate.c's
         * own yoginsa/meditation version of this below. Skipped while
         * `meditating` (found live 2026-08-06): a Mage using `meditate`
         * for Mana almost always already has full HP/Vitality (those
         * regen far faster and aren't what they're meditating for), so
         * this used to fire on literally the FIRST tick and force them
         * standing before meditate_tick_run() ever got a chance to
         * restore any mana at all -- meditate.c's own topped_off check
         * (the resource that command actually cares about) is the only
         * one that should decide when a meditating being stands back
         * up. Also skipped while too drunk to stand (`alcoholism`
         * pass-out mechanic, missing-skill audit batch C, 2026-08-09,
         * vitals.c's own passOut check -- same > 14 threshold): found
         * live the same way the meditating exemption was -- an idle
         * PC's HP/Vitality are typically already topped off, so the
         * very next regen tick after passing out (often within a
         * couple of real seconds) satisfied "was short, now full" from
         * ordinary Vitality trickle-back and stood the character right
         * back up, defeating the whole point of passing out. Same
         * "the subsystem that put you down decides when you get up"
         * principle as the meditating case, just for involuntary
         * unconsciousness instead of a voluntary rest. */
        if (!b->meditating
            && b->progress.drunk <= 14
            && b->position != POSITION_STANDING
            && b->progress.hp >= b->progress.max_hp
            && b->progress.vit >= b->progress.max_vit
            && (was_short_hp || was_short_vit)) {
            b->position = POSITION_STANDING;
            descriptor_send(d, "You feel fully rested and stand up.\r\n");
        }
        being_notify_vitals_changed(b);
    }
}
