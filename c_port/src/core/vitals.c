/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "vitals.h"

#include "being.h"
#include "descriptor.h"
#include "player_repo.h"

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

        player_progress_save(b->player_id, &b->progress);
    }
}
