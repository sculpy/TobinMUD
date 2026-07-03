#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "room.h"
#include "room_repo.h"
#include "world.h"

/* `edit <field> <args>` -- the room builder, a port of the original's
 * TPerson::doEdit() interface (misc/create_rooms.cc, "Original edit code
 * from Silly, May 1992"): field names matched by prefix, `edit name <text>`
 * inline, `edit description` dropping into the string editor, and
 * `edit exit` that auto-creates a missing destination room (copying the
 * current room's sector, like CreateOneRoom + the "small duplicate" setup)
 * and then "Fixing opposite directions" -- the reverse exit is created
 * automatically if absent, reported if present.
 *
 * Deliberate trims from the original (fields Tobin's room_t doesn't have):
 * flags, extra descriptions, height, capacity, river, teleport, room
 * specs, and the whole door half of exits (edit exit takes just
 * <dir> <toroom>, not the original's 7-arg door form). `edit exit <dir> -1`
 * deletes an exit (the original deleted on an invalid target room).
 * Persistence deviation: every change writes to MariaDB immediately --
 * there is no separate rsave-to-zonefile step, the DB is the world.
 *
 * Gate: level 56+ (BUILD_MIN_LEVEL), the same content-editing tier as
 * hedit -- standing in for the original's POWER_EDIT wiz-power and its
 * per-builder room-range ownership, which Tobin doesn't model. */

static int parse_dir(const char *tok) {
    if (isdigit((unsigned char)tok[0])) {
        int dir = atoi(tok);
        return (dir >= 0 && dir < ROOM_NUM_EXITS) ? dir : -1;
    }
    size_t len = strlen(tok);
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    }
    return -1;
}

static room_t *get_or_load_room(int vnum) {
    room_t *r = world_get_room(vnum);
    if (!r) {
        r = room_repo_load(vnum);
        if (r)
            world_register_room(r);
    }
    return r;
}

static void show_room_summary(descriptor_t *d, room_t *r) {
    char out[1024];
    int n = snprintf(out, sizeof(out),
                     "\r\nRoom Name: %s\r\nNumber: %d\r\nSector Type: %d\r\nExits:",
                     r->base.name, r->vnum, r->sector);
    bool any = false;
    for (int i = 0; i < ROOM_NUM_EXITS && (size_t)n < sizeof(out); i++) {
        if (r->exits[i] < 0)
            continue;
        any = true;
        n += snprintf(out + n, sizeof(out) - (size_t)n, " %s->%d", DIR_NAMES[i], r->exits[i]);
    }
    if ((size_t)n < sizeof(out))
        snprintf(out + n, sizeof(out) - (size_t)n,
                 "%s\r\n\r\nFields: name <text> | description | sector_type [n] | "
                 "exit <dir> <toroom | -1>\r\n",
                 any ? "" : " NONE");
    descriptor_send(d, out);
}

bool cmd_edit(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    room_t *r = d->character->base.roomp;

    char field[32];
    if (sscanf(args, "%31s", field) != 1) {
        show_room_summary(d, r);
        return true;
    }
    const char *rest = args + strlen(field);
    while (*rest == ' ')
        rest++;
    size_t flen = strlen(field);

    /* Field prefix matching, like the original's bisect_arg over
     * room_fields[]. The four ported fields start with distinct letters
     * (n/d/s/e), so even one-letter prefixes are unambiguous. */
    if (strncasecmp("name", field, flen) == 0) {
        if (!*rest) {
            descriptor_send(d, "Usage: edit name <new room title>\r\n");
            return true;
        }
        snprintf(r->base.name, sizeof(r->base.name), "%s", rest);
        if (room_repo_save(r)) {
            char msg[160];
            snprintf(msg, sizeof(msg), "New Room Title: %s\r\n", r->base.name);
            descriptor_send(d, msg);
        } else {
            descriptor_send(d, "The DB rejected the change.\r\n");
        }
        return true;
    }

    if (strncasecmp("description", field, flen) == 0) {
        d->edit_room_vnum = r->vnum;
        d->edit_buf[0] = '\0';
        d->edit_len = 0;
        if (r->description[0]) {
            snprintf(d->edit_buf, sizeof(d->edit_buf), "%s", r->description);
            d->edit_len = (int)strlen(d->edit_buf);
        }
        char head[128];
        snprintf(head, sizeof(head),
                 "\r\n-- Editing room %d's description (current text below) --\r\n"
                 "Type lines to append. '.' alone saves, '~' alone aborts.\r\n",
                 r->vnum);
        descriptor_send(d, head);
        if (r->description[0]) {
            descriptor_send(d, r->description);
            if (r->description[strlen(r->description) - 1] != '\n')
                descriptor_send(d, "\r\n");
        }
        descriptor_send(d, "] ");
        d->edit_kind = EDIT_ROOM_DESC;
        return true;
    }

    if (strncasecmp("sector_type", field, flen) == 0) {
        if (!*rest) {
            char msg[96];
            snprintf(msg, sizeof(msg),
                     "Current sector type: %d. Usage: edit sector_type <number>\r\n",
                     r->sector);
            descriptor_send(d, msg);
            return true;
        }
        int s = atoi(rest);
        if (s < 0 || s > 99) {
            descriptor_send(d, "That sector choice is invalid, please try again.\r\n");
            return true;
        }
        r->sector = s;
        descriptor_send(d, room_repo_save(r) ? "Sector type set.\r\n"
                                             : "The DB rejected the change.\r\n");
        return true;
    }

    if (strncasecmp("exit", field, flen) == 0) {
        char dir_tok[16];
        int toroom;
        if (sscanf(rest, "%15s %d", dir_tok, &toroom) != 2) {
            descriptor_send(d, "Syntax : edit exit <dir> <toroom>   (-1 deletes the exit)\r\n");
            return true;
        }
        int dir = parse_dir(dir_tok);
        if (dir < 0) {
            descriptor_send(d, "Direction must be north/east/south/west/up/down (or 0-5).\r\n");
            return true;
        }

        if (toroom < 0) {
            if (r->exits[dir] < 0) {
                descriptor_send(d, "There is no exit that way to delete.\r\n");
                return true;
            }
            r->exits[dir] = -1;
            room_repo_delete_exit(r->vnum, dir);
            descriptor_send(d, "Deleting exit.\r\n");
            return true;
        }
        if (toroom == r->vnum) {
            descriptor_send(d, "An exit into the same room? Pick another target.\r\n");
            return true;
        }

        room_t *to = get_or_load_room(toroom);
        if (!to) {
            /* Original behavior: create the missing room as "a small
             * duplicate" of this one (sector copied), then link. */
            descriptor_send(d, "Exit room does not exist. Creating room....");
            to = room_create(toroom, "An unfinished room",
                             "This freshly dug room has not been described yet.\n",
                             r->sector);
            if (!to || !room_repo_save(to)) {
                room_destroy(to);
                descriptor_send(d, "Something went wrong, tell a coder!\r\n");
                return true;
            }
            world_register_room(to);
            descriptor_send(d, "Done.\r\n");
        }

        bool existed = r->exits[dir] >= 0;
        r->exits[dir] = toroom;
        room_repo_save_exit(r->vnum, dir, toroom);
        descriptor_send(d, existed ? "modifying exit\r\n" : "New exit\r\n");

        /* "Fixing opposite directions." -- straight from the original. */
        descriptor_send(d, "Fixing opposite directions.\r\n");
        int rev = REV_DIR[dir];
        if (to->exits[rev] >= 0) {
            if (to->exits[rev] == r->vnum) {
                descriptor_send(d, "Exit back into room already exists..."
                                   "And is back into the correct room.\r\n");
            } else {
                char msg[96];
                snprintf(msg, sizeof(msg),
                         "Exit back into room already exists..."
                         "And exits into incorrect room [%d].\r\n", to->exits[rev]);
                descriptor_send(d, msg);
            }
        } else {
            to->exits[rev] = r->vnum;
            room_repo_save_exit(to->vnum, rev, r->vnum);
            descriptor_send(d, "Making new exit back into this room.\r\n");
        }
        return true;
    }

    descriptor_send(d, "Unknown field. Fields: name, description, sector_type, exit.\r\n");
    return true;
}
