/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "vitals.h"

#include <stdlib.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "descriptor.h"
#include "player_repo.h"
#include "room.h"

void vitals_tick_run(long pulse_num) {
    (void)pulse_num;
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b || being_is_immortal(b))
            continue;

        if (b->progress.hunger > 0)
            b->progress.hunger--;
        if (b->progress.thirst > 0)
            b->progress.thirst--;

        if (b->progress.hunger == 0 || b->progress.thirst == 0) {
            b->progress.hp--;
            if (b->progress.hp < 1)
                b->progress.hp = 1;
        }

        /* Drowning (Sneezy → Tobin feature audit, "Water, drowning,
         * flight"): the original's procCharDrowning deals 1d10 to a PC
         * underwater without AFF_WATERBREATH every 3.6 real seconds via
         * checkDrowning()/reconcileDamage() -- genuinely lethal. Same
         * 1d10 roll here, just on this tick's own slower ~60s cadence
         * instead, so it's already far gentler in practice without
         * softening the roll itself. AFFECT_FLYING also exempts (same
         * "flight bypasses drowning" rule the original's canFly() ->
         * checkFalling() chain uses). Unlike hunger/thirst above, this
         * one CAN kill (user, AskUserQuestion) -- routed through
         * combat_drown_pc() (combat.c) rather than clamped at 1 HP. */
        if (b->base.roomp && sector_is_underwater(b->base.roomp->sector)
            && !being_has_affect(b, AFFECT_WATERBREATH)
            && !being_has_affect(b, AFFECT_FLYING)) {
            int dmg = 1 + rand() % 10;
            b->progress.hp -= dmg;
            if (b->progress.hp <= 0) {
                combat_drown_pc(b);
                continue; /* `b` is freed -- do not touch it or save below */
            }
            descriptor_send(d, "<r>You struggle against the water, your lungs burning!<z>\r\n");
        }

        player_progress_save(b->player_id, &b->progress);
    }
}
