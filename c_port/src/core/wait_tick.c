#include "wait_tick.h"

#include "descriptor.h"

void wait_tick_run(long pulse_num) {
    (void)pulse_num;
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        if (d->character && d->character->wait_pulses > 0)
            d->character->wait_pulses--;
    }
}
