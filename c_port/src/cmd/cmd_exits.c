/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
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

    /* Weather & light levels (Sneezy → Tobin feature audit): same darkness
     * gate as bare `look` (cmd_look.c) -- gating only one of the two would
     * let a player just route around the restriction with the other. */
    if (room_is_dark_for((struct room *)r, d->character)) {
        descriptor_send(d, "It is pitch black... you cannot see a thing.\r\n");
        return true;
    }

    char out[1024];
    int n = snprintf(out, sizeof(out), "\r\nObvious exits:\r\n");
    bool any = false;
    for (int i = 0; i < ROOM_NUM_EXITS && (size_t)n < sizeof(out); i++) {
        if (r->exits[i] < 0)
            continue;
        if (r->exit_cond[i] & EXIT_COND_SECRET)
            continue; /* undiscovered -- still walkable if you know the direction */
        any = true;
        room_t *to = world_get_room(r->exits[i]);
        if (!to) {
            to = room_repo_load(r->exits[i]);
            if (to)
                world_register_room(to);
        }
        /* A doored exit's direction is shown in red so it stands out from
         * the plain openings (user 2026-08-16), matching `look`'s own
         * [Exits:] line. */
        const char *dcol = r->exit_door[i] != 0 ? "<r>" : "";
        const char *dend = r->exit_door[i] != 0 ? "<z>" : "";
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s%-9s%s - %s\r\n",
                      dcol, DIR_NAMES[i], dend, to ? to->base.name : "somewhere unknown");
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  none!\r\n");

    descriptor_send(d, out);
    return true;
}
