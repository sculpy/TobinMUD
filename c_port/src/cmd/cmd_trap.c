/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <string.h>
#include <strings.h>

#include "room.h"
#include "room_repo.h"
#include "skill.h"

/* `settrap <direction>` / `disarmtrap <direction>` -- trap mechanics
 * (user 2026-07-11: "...then weapon depth, trap mechanics", sequenced
 * right after weapon depth). Wires up the Thief's long-defined-but-
 * unused "set trap (door)"/"disarm trap" skills (skill.c) to the
 * EXIT_COND_TRAPPED bit (room.h) -- a real bit that already existed,
 * named, builder-editable, but with no behavior before this. Setting a
 * trap requires the door to be closed (you can't rig an open door);
 * cmd_move.c's do_move() is where a trap actually springs on whoever
 * walks through it (or is spotted and avoided by "detect trap"). */

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

/* Runs the `settrap` command: a Thief who knows "set trap (door)"
 * rigs a trap on a closed door in the given direction. */
bool cmd_settrap(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "set trap (door)")) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    int dir = parse_dir(args);
    if (dir < 0) {
        descriptor_send(d, "Usage: settrap <direction>\r\n");
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
 * safely removes a trap from a door in the given direction. */
bool cmd_disarmtrap(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "disarm trap")) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    int dir = parse_dir(args);
    if (dir < 0) {
        descriptor_send(d, "Usage: disarmtrap <direction>\r\n");
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
