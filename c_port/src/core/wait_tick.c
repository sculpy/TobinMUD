/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "wait_tick.h"

#include "descriptor.h"

/* Periodic hook (registered with the pulse scheduler) that counts down each
 * connected player's command-lag timer (wait_pulses), e.g. from a slow
 * skill/spell -- once it hits 0 they can act again. */
void wait_tick_run(long pulse_num) {
    (void)pulse_num;
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        if (d->character && d->character->wait_pulses > 0)
            d->character->wait_pulses--;
    }
}
