#include "cmd_internal.h"

#include <stdio.h>

#include "cmd.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* north/east/south/west/up/down -- the first movement commands in the
 * port. Directions are the original dirTypeT's first six slots (see
 * room.h); like classic Diku (and the original's command table), these sit
 * at the very top of COMMANDS[] so the single letters n/e/s/w/u/d always
 * mean movement ("s" is south, not say; "w" is west, not who). */

static bool do_move(descriptor_t *d, int dir) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (ch->fighting) {
        /* Same rule as the original's doMove: no walking out of a fight. */
        descriptor_send(d, "No way! You are fighting for your life!\r\n");
        return true;
    }

    room_t *from = ch->base.roomp;
    int dest = from->exits[dir];
    room_t *to = NULL;
    if (dest >= 0) {
        to = world_get_room(dest);
        if (!to) {
            to = room_repo_load(dest);
            if (to)
                world_register_room(to);
        }
    }
    if (!to) {
        descriptor_send(d, "You can't go that way.\r\n");
        return true;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "%s leaves %s.\r\n", ch->base.name, DIR_NAMES[dir]);
    descriptor_room_echo(from, ch, msg);

    thing_set_room(&ch->base, to);

    snprintf(msg, sizeof(msg), "%s has arrived.\r\n", ch->base.name);
    descriptor_room_echo(to, ch, msg);

    return cmd_dispatch(d, "look");
}

bool cmd_north(descriptor_t *d, const char *args) { (void)args; return do_move(d, 0); }
bool cmd_east(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 1); }
bool cmd_south(descriptor_t *d, const char *args) { (void)args; return do_move(d, 2); }
bool cmd_west(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 3); }
bool cmd_up(descriptor_t *d, const char *args)    { (void)args; return do_move(d, 4); }
bool cmd_down(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 5); }
