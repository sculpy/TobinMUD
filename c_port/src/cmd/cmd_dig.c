/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "cmd.h"
#include "log.h"
#include "room.h"
#include "room_repo.h"
#include "world.h"
#include "zone.h"
#include "zone_repo.h"

/* Same abbreviation-prefix matching as cmd_open.c's own parse_dir() --
 * duplicated locally rather than shared, same precedent as every other
 * small per-file helper in this codebase. */
static int parse_dir(const char *tok) {
    size_t len = strlen(tok);
    if (!len)
        return -1;
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    }
    return -1;
}

/* `dig <direction>` (BUILD_MIN_LEVEL) -- builder-walk: if there's no exit
 * that way yet, creates a brand new room, wires this room's exit to it and
 * its own exit back (REV_DIR), then walks the builder through exactly
 * like a normal move (cmd_dispatch() of the direction word itself, so
 * every bit of do_move()'s own logic -- arrival triggers, poofin/poofout,
 * the fighting/position gates -- applies unchanged). User, TODO.md: "dig
 * -- builder-walk: moving into a nonexistent exit auto-creates the room +
 * reverse exit... Needs a next-free-vnum strategy." The new room's vnum
 * comes from room_repo_next_free_vnum() within the CURRENT room's own
 * zone range -- same zone_can_edit() ownership gate `edroom`/`edtrigger`
 * already enforce, so a builder can't dig into someone else's zone (or an
 * unzoned room, which has no range to allocate from at all). */
bool cmd_dig(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[24] = "";
    sscanf(args, "%23s", tok);
    int dir = parse_dir(tok);
    if (dir < 0) {
        descriptor_send(d, "Usage: dig <direction>\r\n");
        return true;
    }

    room_t *from = ch->base.roomp;
    int zone_nr = room_repo_get_zone(from->vnum);
    if (!zone_can_edit(ch, zone_nr)) {
        descriptor_send(d, "You aren't assigned to build in this zone.\r\n");
        return true;
    }
    if (from->exits[dir] >= 0) {
        descriptor_send(d, "There's already an exit that way.\r\n");
        return true;
    }
    if (zone_nr < 0) {
        descriptor_send(d, "This room isn't part of any zone -- dig only works within a zone's vnum range.\r\n");
        return true;
    }

    zone_t zone;
    if (!zone_repo_load_one(zone_nr, &zone)) {
        descriptor_send(d, "Couldn't load this room's zone.\r\n");
        return true;
    }

    int new_vnum = room_repo_next_free_vnum(zone.bottom, zone.top);
    if (new_vnum < 0) {
        descriptor_send(d, "No free room number left in this zone's range.\r\n");
        return true;
    }

    room_t *new_room = room_create(new_vnum, "An Unfinished Room",
        "You are standing in a freshly dug room, waiting to be given real shape. "
        "An immortal can describe it with `edit room`.\r\n",
        from->sector);
    if (!new_room) {
        descriptor_send(d, "Something went wrong -- the new room couldn't be created.\r\n");
        return true;
    }
    room_repo_save(new_room);

    from->exits[dir] = new_vnum;
    room_repo_save_exit(from->vnum, dir, new_vnum, 0, 0);
    new_room->exits[REV_DIR[dir]] = from->vnum;
    room_repo_save_exit(new_vnum, REV_DIR[dir], from->vnum, 0, 0);

    world_register_room(new_room);

    char msg[128];
    snprintf(msg, sizeof(msg), "You dig a passage %s into new territory -- room %d.\r\n",
             DIR_NAMES[dir], new_vnum);
    descriptor_send(d, msg);
    game_log(LOG_EDIT, "%s dug room %d (%s of room %d, zone %d).",
             ch->base.name, new_vnum, DIR_NAMES[dir], from->vnum, zone_nr);

    return cmd_dispatch(d, DIR_NAMES[dir]);
}
