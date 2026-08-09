/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "obj.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"

/* `settrap <direction|container>` / `disarmtrap <direction|container>` --
 * trap mechanics (user 2026-07-11: "...then weapon depth, trap
 * mechanics", sequenced right after weapon depth). Wires up the Thief's
 * long-defined-but-unused "set trap (door)"/"disarm trap" skills
 * (skill.c) to the EXIT_COND_TRAPPED bit (room.h) -- a real bit that
 * already existed, named, builder-editable, but with no behavior before
 * this. Setting a trap requires the door to be closed (you can't rig an
 * open door); cmd_move.c's do_move() is where a trap actually springs on
 * whoever walks through it (or is spotted and avoided by "detect
 * trap").
 *
 * `set trap (container)` (missing-skill audit, 2026-08-09) extends the
 * exact same command to a carried/room container object instead of a
 * door, using the real upstream CONT_TRAPPED bit (obj.h, ported
 * verbatim from misc/obj.h's own `1 << 4`) -- cmd_open.c's do_container()
 * springs it on `open`. Deliberately NOT ported here: `set trap
 * (arrow)`/`(mine)`/`(grenade)` -- real upstream's own arrow/mine/
 * grenade trap targets are ammo-quiver, room-floor, and thrown-explosive
 * mechanics respectively (misc/trap.h's trap_targ_t), none of which
 * Tobin has a subsystem for (no ranged/quiver system, no room-floor trap
 * object type, no thrown-weapon command) -- same disclosed-gap pattern
 * as `sling shot`/`stunning arrow` elsewhere in this audit, not a partial
 * or faked implementation. */

/* Matches `tok` against a direction name (n/north/northeast/...),
 * abbreviation-friendly like every other direction-taking command. */
static int parse_dir(const char *tok) {
    size_t len = strlen(tok);
    if (len == 0)
        return -1;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    return -1;
}

/* A container matching `tok` among your own carried/worn items, then the
 * room floor -- same search order/file-local-static convention cmd_open.c
 * and cmd_lock.c's own duplicated find_container() already use. */
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

/* Runs the `settrap` command: a Thief who knows "set trap (door)" rigs
 * a trap on a closed door in the given direction, or -- if they know
 * "set trap (container)" and `args` doesn't parse as a direction -- a
 * trap on a closed container instead. */
bool cmd_settrap(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    int dir = parse_dir(args);
    if (dir < 0) {
        char tok[64];
        if (sscanf(args, "%63s", tok) == 1) {
            obj_t *cont = find_container(ch, tok);
            if (cont) {
                if (!being_knows_skill(ch, "set trap (container)")) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                if (!(cont->val[1] & CONT_CLOSEABLE)) {
                    descriptor_send(d, "That doesn't close, so there's nothing to rig.\r\n");
                    return true;
                }
                if (!(cont->val[1] & CONT_CLOSED)) {
                    descriptor_send(d, "You should close it first, I'm afraid.\r\n");
                    return true;
                }
                if (cont->val[1] & CONT_TRAPPED) {
                    descriptor_send(d, "It's already trapped.\r\n");
                    return true;
                }
                bool imm = being_is_immortal(ch);
                const skill_def_t *sk = skill_find(ch->char_class, "set trap (container)", imm);
                if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
                    descriptor_send(d, "You fumble rigging the trap -- it doesn't take.\r\n");
                    return true;
                }
                cont->val[1] |= CONT_TRAPPED;
                const char *label = cont->base.short_descr[0] ? cont->base.short_descr : cont->base.name;
                char msg[256];
                snprintf(msg, sizeof(msg), "You rig a trap on %s.\r\n", label);
                descriptor_send(d, msg);
                return true;
            }
        }
        descriptor_send(d, "Usage: settrap <direction|container>\r\n");
        return true;
    }

    if (!being_knows_skill(ch, "set trap (door)")) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    room_t *r = ch->base.roomp;
    if (r->exit_door[dir] == 0) {
        descriptor_send(d, "There's no door that way.\r\n");
        return true;
    }
    if (!(r->exit_cond[dir] & EXIT_COND_CLOSED)) {
        descriptor_send(d, "The door must be closed before you can trap it.\r\n");
        return true;
    }
    if (r->exit_cond[dir] & EXIT_COND_TRAPPED) {
        descriptor_send(d, "It's already trapped.\r\n");
        return true;
    }

    /* Per-skill proficiency (Sneezy-style learn-by-doing, user 2026-07-17)
     * -- a fumbled rig wastes the attempt but leaves the door untrapped,
     * same "attempt made, effect not guaranteed" spirit as cast/pray. */
    bool imm = being_is_immortal(ch);
    const skill_def_t *sk = skill_find(ch->char_class, "set trap (door)", imm);
    if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
        descriptor_send(d, "You fumble rigging the trap -- it doesn't take.\r\n");
        return true;
    }

    r->exit_cond[dir] |= EXIT_COND_TRAPPED;
    room_repo_save_exit(r->vnum, dir, r->exits[dir], r->exit_door[dir], r->exit_cond[dir]);
    descriptor_send(d, "You rig a trap on the door.\r\n");
    return true;
}

/* Runs the `disarmtrap` command: a Thief who knows "disarm trap"
 * safely removes a trap from a door in the given direction, or a
 * trapped container if `args` doesn't parse as a direction. */
bool cmd_disarmtrap(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    int dir = parse_dir(args);
    if (dir < 0) {
        char tok[64];
        if (sscanf(args, "%63s", tok) == 1) {
            obj_t *cont = find_container(ch, tok);
            if (cont) {
                if (!being_knows_skill(ch, "disarm trap")) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                if (!(cont->val[1] & CONT_TRAPPED)) {
                    descriptor_send(d, "There's no trap there.\r\n");
                    return true;
                }
                bool imm = being_is_immortal(ch);
                const skill_def_t *sk = skill_find(ch->char_class, "disarm trap", imm);
                if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
                    descriptor_send(d, "You fumble disarming the trap -- it's still rigged.\r\n");
                    return true;
                }
                cont->val[1] &= ~CONT_TRAPPED;
                descriptor_send(d, "You carefully disarm the trap.\r\n");
                return true;
            }
        }
        descriptor_send(d, "Usage: disarmtrap <direction|container>\r\n");
        return true;
    }

    if (!being_knows_skill(ch, "disarm trap")) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    room_t *r = ch->base.roomp;
    if (!(r->exit_cond[dir] & EXIT_COND_TRAPPED)) {
        descriptor_send(d, "There's no trap there.\r\n");
        return true;
    }

    /* Per-skill proficiency (Sneezy-style learn-by-doing, user 2026-07-17)
     * -- a fumbled disarm leaves the trap rigged; it does not spring on
     * the disarmer (kept deliberately non-punishing, v1 scope). */
    bool imm = being_is_immortal(ch);
    const skill_def_t *sk = skill_find(ch->char_class, "disarm trap", imm);
    if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
        descriptor_send(d, "You fumble disarming the trap -- it's still rigged.\r\n");
        return true;
    }

    r->exit_cond[dir] &= ~EXIT_COND_TRAPPED;
    room_repo_save_exit(r->vnum, dir, r->exits[dir], r->exit_door[dir], r->exit_cond[dir]);
    descriptor_send(d, "You carefully disarm the trap.\r\n");
    return true;
}
