/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "room.h"
#include "room_repo.h"
#include "thing.h"

/* `open`/`close <direction>`: the door-type/condition data an exit already
 * carries (set via `edroom`) finally does something. Each exit's door is
 * independent, per-direction state (exit_door[dir]/exit_cond[dir]) --
 * matching how `edroom`'s own auto-created reverse exit already works
 * (no door, own condition bitmask, never mirrored from the forward exit).
 * Opening/closing one side does NOT affect the other side's exit; that's
 * a deliberate simplification consistent with the existing schema, not an
 * oversight -- see STATUS.md. Locked doors can only be opened once the
 * Locked bit is cleared (currently only `edroom`'s toggle submenu can do
 * that -- a real `lock`/`unlock` command needs a key, which needs the
 * object system; deferred, see TODO.md). */

static int parse_dir(const char *tok) {
    size_t len = strlen(tok);
    if (len == 0)
        return -1;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    return -1;
}

static bool do_door(descriptor_t *d, const char *args, bool opening) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[16];
    if (sscanf(args, "%15s", tok) != 1) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Usage: %s <direction>\r\n", opening ? "open" : "close");
        descriptor_send(d, msg);
        return true;
    }

    room_t *r = ch->base.roomp;
    int dir = parse_dir(tok);
    if (dir < 0 || r->exits[dir] < 0) {
        descriptor_send(d, "You don't see an exit that way.\r\n");
        return true;
    }
    if (r->exit_door[dir] == 0) {
        descriptor_send(d, "There is no door there.\r\n");
        return true;
    }

    bool closed = (r->exit_cond[dir] & EXIT_COND_CLOSED) != 0;
    if (opening) {
        if (!closed) {
            descriptor_send(d, "It's already open.\r\n");
            return true;
        }
        if (r->exit_cond[dir] & EXIT_COND_LOCKED) {
            descriptor_send(d, "It's locked.\r\n");
            return true;
        }
        r->exit_cond[dir] &= ~EXIT_COND_CLOSED;
    } else {
        if (!closed) {
            r->exit_cond[dir] |= EXIT_COND_CLOSED;
        } else {
            descriptor_send(d, "It's already closed.\r\n");
            return true;
        }
    }

    room_repo_save_exit(r->vnum, dir, r->exits[dir], r->exit_door[dir], r->exit_cond[dir]);

    /* door_type_name() returns its display form capitalized ("Door",
     * "Gate", ...); lowercase it for mid-sentence use here. */
    char door[16];
    snprintf(door, sizeof(door), "%s", door_type_name(r->exit_door[dir]));
    for (char *p = door; *p; p++)
        *p = (char)tolower((unsigned char)*p);

    char msg[96];
    snprintf(msg, sizeof(msg), "You %s the %s to the %s.\r\n",
             opening ? "open" : "close", door, DIR_NAMES[dir]);
    descriptor_send(d, msg);

    char echo[128];
    snprintf(echo, sizeof(echo), "%s %s the %s to the %s.\r\n", ch->base.name,
             opening ? "opens" : "closes", door, DIR_NAMES[dir]);
    descriptor_room_echo(r, ch, echo);
    return true;
}

bool cmd_open(descriptor_t *d, const char *args) { return do_door(d, args, true); }
bool cmd_close(descriptor_t *d, const char *args) { return do_door(d, args, false); }
