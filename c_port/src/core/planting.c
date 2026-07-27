/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "planting.h"

#include <stdio.h>

#include "being.h"
#include "descriptor.h"
#include "obj.h"
#include "obj_plant.h"
#include "room.h"
#include "thing.h"

/* Cancels an in-progress planting attempt, telling `ch` and the room why
 * (moved away, lost the seeds, etc.) and clearing all planting_* state on
 * the being so planting_tick_run() stops acting on them. */
static void abort_planting(descriptor_t *d, being_t *ch, const char *why) {
    char msg[256];
    snprintf(msg, sizeof(msg), "You stop planting seeds%s.\r\n", why);
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        char cap[128];
        being_display_name_cap(ch, cap, sizeof(cap));
        snprintf(msg, sizeof(msg), "%s stops planting seeds.\r\n", cap);
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    ch->planting_seed = NULL;
    ch->planting_ticks_left = 0;
    ch->planting_type = 0;
    ch->planting_room = NULL;
}

/* True iff `seed` is still directly in `ch`'s own carried inventory (not
 * dropped, given away, or stolen out from under the task). */
static bool still_carrying(const being_t *ch, const struct obj *seed) {
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next)
        if (t == &seed->base)
            return true;
    return false;
}

/* Periodic hook that advances every connected player's in-progress planting
 * action by one tick (dig hole -> sow seeds -> cover hole), aborting it if
 * the player wandered off, lost the seeds, or got pulled into combat; the
 * final tick hands off to obj_plant_create() to actually spawn the plant. */
void planting_tick_run(long pulse_num) {
    (void)pulse_num;

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *ch = d->character;
        if (!ch || ch->planting_ticks_left <= 0)
            continue;

        if (ch->base.roomp != ch->planting_room) {
            abort_planting(d, ch, " -- you've moved on");
            continue;
        }
        /* planting_seed goes NULL on purpose once the sow step consumes it
         * (below) -- only check it's still held while there's still a
         * real seed pointer to check, i.e. before that step. */
        if (ch->planting_seed && !still_carrying(ch, ch->planting_seed)) {
            abort_planting(d, ch, " -- you no longer have the seeds");
            continue;
        }
        if (ch->fighting) {
            descriptor_send(d, "You can't properly plant seeds while under attack.\r\n");
            ch->planting_seed = NULL;
            ch->planting_ticks_left = 0;
            ch->planting_type = 0;
            ch->planting_room = NULL;
            continue;
        }

        ch->planting_ticks_left--;
        char msg[256];

        if (ch->planting_ticks_left == 2) {
            descriptor_send(d, "You dig a little hole for some seeds.\r\n");
            char cap[128];
            being_display_name_cap(ch, cap, sizeof(cap));
            snprintf(msg, sizeof(msg), "%s digs a little hole.\r\n", cap);
            descriptor_room_echo(ch->base.roomp, ch, msg);
        } else if (ch->planting_ticks_left == 1) {
            descriptor_send(d, "You put some seeds into your hole.\r\n");
            char cap[128];
            being_display_name_cap(ch, cap, sizeof(cap));
            snprintf(msg, sizeof(msg), "%s puts some seeds into the hole.\r\n", cap);
            descriptor_room_echo(ch->base.roomp, ch, msg);
            /* Whole seed sack consumed at the sow step -- Tobin's obj_t has
             * no partial-uses counter for a TOOL item (unlike DRUG/
             * MAGIC_DEVICE's val[]-based charges), so this is a disclosed
             * simplification of the original's per-use `getToolUses()`
             * countdown. */
            obj_t *seed = ch->planting_seed;
            ch->planting_seed = NULL;
            obj_destroy(seed);
        } else {
            descriptor_send(d, "You cover up the hole.\r\n");
            char cap[128];
            being_display_name_cap(ch, cap, sizeof(cap));
            snprintf(msg, sizeof(msg), "%s covers up the hole.\r\n", cap);
            descriptor_room_echo(ch->base.roomp, ch, msg);

            obj_plant_create(ch->planting_room, ch->planting_type);
            descriptor_send(d, "You finish planting your seeds.\r\n");
            snprintf(msg, sizeof(msg), "%s finishes planting seeds.\r\n", cap);
            descriptor_room_echo(ch->base.roomp, ch, msg);

            ch->planting_type = 0;
            ch->planting_room = NULL;
        }
    }
}
