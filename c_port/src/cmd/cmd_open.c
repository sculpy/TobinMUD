/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj.h"
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

/* A container matching `tok` among your own carried/worn items, then the room
 * floor -- the same search order `put`/`get <container>` use. */
static obj_t *find_container(being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    thing_t *chains[2] = {
        ch->base.stuff_head,
        ch->base.roomp ? ch->base.roomp->base.stuff_head : NULL,
    };
    for (int c = 0; c < 2; c++) {
        for (thing_t *t = chains[c]; t; t = t->stuff_next) {
            if (t->kind != THING_OBJ)
                continue;
            obj_t *o = (obj_t *)t;
            if (obj_is_container(o) && thing_name_matches(t->name, tok, len))
                return o;
        }
    }
    return NULL;
}

/* open/close a container object via its val[1] CONT_* flags. Open/closed state
 * is on the in-world instance and is NOT persisted (room-floor objects don't
 * survive a restart; player_inventory stores only vnum/slot) -- it resets to
 * the prototype default on reload, same deferral as the rest of containers.
 * See STATUS.md. */
static bool do_container(descriptor_t *d, being_t *ch, obj_t *o, bool opening) {
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    if (!(o->val[1] & CONT_CLOSEABLE)) {
        descriptor_send(d, "That doesn't open and close.\r\n");
        return true;
    }
    bool closed = (o->val[1] & CONT_CLOSED) != 0;
    if (opening) {
        if (!closed) {
            descriptor_send(d, "It's already open.\r\n");
            return true;
        }
        if (o->val[1] & CONT_LOCKED) {
            descriptor_send(d, "It's locked.\r\n");
            return true;
        }
        o->val[1] &= ~CONT_CLOSED;
    } else {
        if (closed) {
            descriptor_send(d, "It's already closed.\r\n");
            return true;
        }
        o->val[1] |= CONT_CLOSED;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "You %s %s.\r\n", opening ? "open" : "close", label);
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        snprintf(msg, sizeof(msg), "%s %s %s.\r\n", ch->base.name,
                 opening ? "opens" : "closes", label);
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}

static bool do_openclose(descriptor_t *d, const char *args, bool opening) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Usage: %s <direction|container>\r\n", opening ? "open" : "close");
        descriptor_send(d, msg);
        return true;
    }

    room_t *r = ch->base.roomp;
    int dir = parse_dir(tok);

    /* A real exit with a door in that direction -> operate the door. */
    if (dir >= 0 && r->exits[dir] >= 0 && r->exit_door[dir] != 0) {
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
            if (closed) {
                descriptor_send(d, "It's already closed.\r\n");
                return true;
            }
            r->exit_cond[dir] |= EXIT_COND_CLOSED;
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

    /* Otherwise, try a container by that name. */
    obj_t *cont = find_container(ch, tok);
    if (cont)
        return do_container(d, ch, cont, opening);

    /* Neither a door nor a container. Keep the original direction-specific
     * wording when the token names a real direction (the doors smoke test
     * relies on it); only a non-direction token with no matching container
     * gets the generic message. */
    if (dir >= 0)
        descriptor_send(d, r->exits[dir] >= 0 ? "There is no door there.\r\n"
                                              : "You don't see an exit that way.\r\n");
    else
        descriptor_send(d, "You don't see that here.\r\n");
    return true;
}

bool cmd_open(descriptor_t *d, const char *args) { return do_openclose(d, args, true); }
bool cmd_close(descriptor_t *d, const char *args) { return do_openclose(d, args, false); }
