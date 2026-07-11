/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "mob_ai.h"
#include "obj.h"
#include "trigger.h"

/* Immortal-only debug/testing tool (Session 43 continued), same precedent
 * as `hurtlimb` (cmd_hurtlimb.c): mob_ai_tick()'s wander/scavenge chances
 * (20%/25%), obj_pool_decay_tick()'s puddle shrinkage, and
 * trigger_random_tick()'s "random" scripted triggers only actually fire on
 * the real ~60s pulse cadence, far too slow to wait on in an automated
 * smoke test. `aitick [count]` forces `count` (default 1, capped at 100)
 * consecutive world ticks synchronously, so a test can force overwhelming
 * odds of a wander/scavenge/random-trigger firing (e.g. `aitick 30` for a
 * ~99.9% chance) or fully decay a pool without waiting on real time at all. */
bool cmd_aitick(descriptor_t *d, const char *args) {
    int count = 1;
    if (*args)
        count = atoi(args);
    if (count < 1)
        count = 1;
    if (count > 100)
        count = 100;

    for (int i = 0; i < count; i++) {
        mob_ai_tick(0);
        obj_pool_decay_tick(0);
        trigger_random_tick(0);
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "Ran %d mob AI tick(s).\r\n", count);
    descriptor_send(d, msg);
    return true;
}
