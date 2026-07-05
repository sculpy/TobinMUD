/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "room.h"
#include "room_repo.h"
#include "world.h"

/* `exits`: lists the current room's exits with each destination's name --
 * the classic Diku autoexit display, requested Tier 3 (Session 21).
 * Destinations are lazily loaded (same pattern as movement/goto), so this
 * also warms the room cache along travel routes. Affects nothing else;
 * `look`'s one-line "Obvious exits:" summary stays as the quick view. */
bool cmd_exits(descriptor_t *d, const char *args) {
    (void)args;

    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    room_t *r = d->character->base.roomp;

    char out[1024];
    int n = snprintf(out, sizeof(out), "\r\nObvious exits:\r\n");
    bool any = false;
    for (int i = 0; i < ROOM_NUM_EXITS && (size_t)n < sizeof(out); i++) {
        if (r->exits[i] < 0)
            continue;
        any = true;
        room_t *to = world_get_room(r->exits[i]);
        if (!to) {
            to = room_repo_load(r->exits[i]);
            if (to)
                world_register_room(to);
        }
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-9s - %s\r\n",
                      DIR_NAMES[i], to ? to->base.name : "somewhere unknown");
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  none!\r\n");

    descriptor_send(d, out);
    return true;
}
