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

/* `lock`/`unlock <direction>` and `lock`/`unlock <container>` -- the real
 * missing half of cmd_open.c's door/container mechanics, unblocked now that
 * the object system exists (see TODO.md "Keys unlocking doors"). Matching
 * rule, confirmed against the original engine's has_key()/keyCheck()
 * (misc/movement.cc): a required key is identified by the KEY object's OWN
 * vnum, NOT by any val[] field on the key itself -- a door's `roomexit.
 * key_num` / a container's `val[2]` names the vnum a carried object must
 * have to work. 1,141 real seeded doors and dozens of seeded containers
 * already carry working key_num/val[2] data (confirmed live), so this is
 * immediately testable against real content, not just new data. */

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
 * floor -- same search order/ordinal support as cmd_open.c's own (separate,
 * duplicated by the same file-local-static convention that file's cap_first
 * comment documents) find_container(). */
static obj_t *find_container(being_t *ch, const char *tok) {
    const char *rest;
    int ordinal = thing_parse_ordinal(tok, &rest);
    size_t len = strlen(rest);
    thing_t *chains[2] = {
        ch->base.stuff_head,
        ch->base.roomp ? ch->base.roomp->base.stuff_head : NULL,
    };
    for (int c = 0; c < 2; c++) {
        int seen = 0;
        for (thing_t *t = chains[c]; t; t = t->stuff_next) {
            if (t->kind != THING_OBJ)
                continue;
            obj_t *o = (obj_t *)t;
            if (obj_is_container(o) && thing_name_matches(t->name, rest, len)) {
                seen++;
                if (seen == ordinal)
                    return o;
            }
        }
    }
    return NULL;
}

/* Any object carried/worn/held by `ch` whose own vnum is `key_vnum` -- the
 * real key-matching rule (see file header). Walks the same single
 * containment chain worn/held items live in (obj.h's stuff_head comment),
 * so a ring on a finger or a key held in a hand both count, matching the
 * original's inventory+held check. Returns it (for its short_descr in the
 * unlock flavor message) or NULL. */
static obj_t *find_key(being_t *ch, int key_vnum) {
    if (key_vnum <= 0)
        return NULL;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->vnum == key_vnum)
            return o;
    }
    return NULL;
}

static bool do_container_lock(descriptor_t *d, being_t *ch, obj_t *o, bool locking) {
    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    int key_vnum = o->val[2];

    if (locking) {
        if (!(o->val[1] & CONT_CLOSED)) {
            descriptor_send(d, "You should close it first, I'm afraid.\r\n");
            return true;
        }
        if (key_vnum < 1) {
            descriptor_send(d, "That doesn't seem to have a lock.\r\n");
            return true;
        }
        if (!find_key(ch, key_vnum)) {
            descriptor_send(d, "You don't seem to have the proper key.\r\n");
            return true;
        }
        if (o->val[1] & CONT_LOCKED) {
            descriptor_send(d, "It is locked already.\r\n");
            return true;
        }
        o->val[1] |= CONT_LOCKED;
    } else {
        if (key_vnum < 1) {
            descriptor_send(d, "Odd -- you can't seem to find a keyhole.\r\n");
            return true;
        }
        obj_t *key = find_key(ch, key_vnum);
        if (!key) {
            descriptor_send(d, "You don't seem to have the proper key.\r\n");
            return true;
        }
        if (!(o->val[1] & CONT_LOCKED)) {
            descriptor_send(d, "Oh -- it wasn't locked, after all.\r\n");
            return true;
        }
        o->val[1] &= ~CONT_LOCKED;
        char msg[320];
        const char *keylabel = key->base.short_descr[0] ? key->base.short_descr : key->base.name;
        snprintf(msg, sizeof(msg), "You unlock %s with %s.\r\n*Click*\r\n", label, keylabel);
        descriptor_send(d, msg);
        if (ch->base.roomp) {
            snprintf(msg, sizeof(msg), "%s unlocks %s.\r\n", ch->base.name, label);
            descriptor_room_echo(ch->base.roomp, ch, msg);
        }
        return true;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "You lock %s.\r\n*Click*\r\n", label);
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        snprintf(msg, sizeof(msg), "%s locks %s with a *click*.\r\n", ch->base.name, label);
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}

static bool do_door_lock(descriptor_t *d, being_t *ch, room_t *r, int dir, bool locking) {
    if (r->exits[dir] < 0 || r->exit_door[dir] == 0) {
        descriptor_send(d, "There is no door there.\r\n");
        return true;
    }
    int key_vnum = r->exit_key[dir];
    bool closed = (r->exit_cond[dir] & EXIT_COND_CLOSED) != 0;
    bool locked = (r->exit_cond[dir] & EXIT_COND_LOCKED) != 0;

    if (!closed) {
        descriptor_send(d, "You have to close it first, I'm afraid.\r\n");
        return true;
    }
    if (key_vnum < 1) {
        descriptor_send(d, locking ? "There doesn't seem to be any keyhole.\r\n"
                                    : "You can't seem to spot any keyhole.\r\n");
        return true;
    }
    obj_t *key = find_key(ch, key_vnum);
    if (!key) {
        descriptor_send(d, "You don't have the proper key.\r\n");
        return true;
    }
    if (locking && locked) {
        descriptor_send(d, "It's already locked!\r\n");
        return true;
    }
    if (!locking && !locked) {
        descriptor_send(d, "It's already unlocked, it seems.\r\n");
        return true;
    }

    if (locking)
        r->exit_cond[dir] |= EXIT_COND_LOCKED;
    else
        r->exit_cond[dir] &= ~EXIT_COND_LOCKED;
    room_repo_save_exit(r->vnum, dir, r->exits[dir], r->exit_door[dir], r->exit_cond[dir]);

    char door[16];
    snprintf(door, sizeof(door), "%s", door_type_name(r->exit_door[dir]));
    for (char *p = door; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    const char *keylabel = key->base.short_descr[0] ? key->base.short_descr : key->base.name;

    char msg[256];
    if (locking)
        snprintf(msg, sizeof(msg), "You lock the %s to the %s.\r\n*Click*\r\n", door, DIR_NAMES[dir]);
    else
        snprintf(msg, sizeof(msg), "You unlock the %s to the %s with %s.\r\n*Click*\r\n",
                 door, DIR_NAMES[dir], keylabel);
    descriptor_send(d, msg);

    char echo[192];
    snprintf(echo, sizeof(echo), "%s %s the %s to the %s.\r\n", ch->base.name,
             locking ? "locks" : "unlocks", door, DIR_NAMES[dir]);
    descriptor_room_echo(r, ch, echo);
    return true;
}

static bool do_lockunlock(descriptor_t *d, const char *args, bool locking) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Usage: %s <direction|container>\r\n", locking ? "lock" : "unlock");
        descriptor_send(d, msg);
        return true;
    }

    int dir = parse_dir(tok);
    if (dir >= 0)
        return do_door_lock(d, ch, ch->base.roomp, dir, locking);

    obj_t *cont = find_container(ch, tok);
    if (cont)
        return do_container_lock(d, ch, cont, locking);

    descriptor_send(d, "You don't see that here.\r\n");
    return true;
}

bool cmd_lock(descriptor_t *d, const char *args) { return do_lockunlock(d, args, true); }
bool cmd_unlock(descriptor_t *d, const char *args) { return do_lockunlock(d, args, false); }
