/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "drug.h"
#include "fly.h"
#include "gametime.h"
#include "mob_ai.h"
#include "obj.h"
#include "obj_plant.h"
#include "planting.h"
#include "trigger.h"
#include "vitals.h"
#include "weather.h"

/* Immortal-only debug/testing tool (Session 43 continued), same precedent
 * as `hurtlimb` (cmd_crit.c): mob_ai_tick()'s wander/scavenge chances
 * (20%/25%, also now the lamplighter's light/extinguish check),
 * obj_pool_decay_tick()'s puddle shrinkage, obj_light_burn_tick()'s
 * fuel burn-down, (Sneezy → Tobin feature audit, "Object maintenance")
 * obj_decay_tick()'s room-floor decay countdowns (corpses, severed
 * limbs, ...), trigger_random_tick()'s "random" scripted triggers,
 * (same audit, "Weather & light levels") gametime_tick()'s clock advance
 * + weather_tick_run()'s sky transitions, and (Planting)
 * obj_plant_growth_tick()'s crop aging/fruit yield only actually fire on
 * the real ~60s pulse cadence, far too slow to wait on in an automated
 * smoke test; planting_tick_run() (the dig/sow/cover task itself) is
 * faster (~3s) but still worth forcing.
 *
 * `vitals_tick_force_world_only()` deliberately does NOT drain hunger/
 * thirst or apply starvation/drowning HP damage to any connected player
 * here (vitals.h has the full incident writeup: forcing hundreds of
 * ticks at once via `aitick` used to silently starve and nearly kill
 * whichever OTHER players happened to be online at the time, not just
 * whatever the immortal running it meant to test) -- `aitick` is
 * artificial WORLD state acceleration only, never a player-vitals
 * effect, by design.
 *
 * `aitick [count]` forces `count` (default 1, capped at 100) consecutive
 * world ticks synchronously, so a test can force overwhelming odds of a
 * wander/scavenge/random-trigger firing (e.g. `aitick 30` for a ~99.9%
 * chance), fully decay a pool/burn down a light, all without waiting on
 * real time at all. Also forces along any `wait`-paused trigger script
 * (trigger_pending_force_all()) -- those otherwise resume on their own
 * ~1s real-time cadence, still too slow for a test that wants to walk
 * through a multi-`wait` script deterministically. */
bool cmd_aitick(descriptor_t *d, const char *args) {
    int count = 1;
    if (*args)
        count = atoi(args);
    if (count < 1)
        count = 1;
    if (count > 100)
        count = 100;

    for (int i = 0; i < count; i++) {
        /* Resolve whatever was ALREADY pending (from a real tick, or a
         * previous iteration of this loop) before running this iteration's
         * OWN random-trigger pass -- reversed, a script's own `wait` would
         * get force-resolved in the very same pass that just scheduled it,
         * collapsing the pause into a no-op (caught live: `aitick 1` was
         * showing a two-line wait/say script's whole output at once). */
        trigger_pending_force_all();
        mob_ai_tick(0);
        obj_pool_decay_tick(0);
        obj_light_burn_tick(0);
        obj_decay_tick(0);
        trigger_random_tick(0);
        vitals_tick_force_world_only(0);
        drug_tick_run(0);
        gametime_tick(0);
        weather_tick_run(0);
        planting_tick_run(0);
        fly_tick_run(0);
        obj_plant_growth_tick(0);
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "Ran %d mob AI tick(s).\r\n", count);
    descriptor_send(d, msg);
    return true;
}
