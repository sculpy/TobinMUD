/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"

/* `pick <direction|container>` -- the Thief's long-defined-but-unused
 * "pick lock" skill (skill.c). Real upstream (task/task_picklock.cc) is
 * a multi-pulse TASK: `bSuccess()` re-rolled every game pulse for as
 * long as the thief keeps at it, requires a held TOOL_LOCKPICK item,
 * and jams the lock on a bad enough roll. Tobin has no lockpick tool
 * item type and no multi-tick "keep trying" task-continuation
 * infrastructure for a mortal-level command (settrap/bandage/etc. are
 * all single-attempt, same v1 scope) -- ported as a single proficiency-
 * scaled attempt instead, same "one command, one roll" shape every
 * other Sneezy-audit skill in this batch uses, with the real jam-vs-
 * pick distinction dropped along with the tool requirement (disclosed
 * scope-down, not a faked mechanic). Works on either a locked door
 * (EXIT_COND_LOCKED, room.h) or a locked container (CONT_LOCKED,
 * obj.h) that isn't flagged CONT_PICKPROOF -- no key required either
 * way, unlike `unlock`. */

static int parse_dir(const char *tok) {
    size_t len = strlen(tok);
    if (len == 0)
        return -1;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    return -1;
}

/* Same file-local-static find_container() shape as cmd_lock.c/cmd_open.c/
 * cmd_trap.c's own duplicated copies -- carried/worn first, then the
 * room floor. */
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

bool cmd_pick(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "pick lock")) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: pick <direction|container>\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    const skill_def_t *sk = skill_find(ch->char_class, "pick lock", imm);
    int prof = imm || !sk ? 100 : skill_learn_from_doing(ch, sk);

    int dir = parse_dir(tok);
    if (dir >= 0) {
        room_t *r = ch->base.roomp;
        if (r->exit_door[dir] == 0) {
            descriptor_send(d, "There is no door there.\r\n");
            return true;
        }
        if (!(r->exit_cond[dir] & EXIT_COND_CLOSED)) {
            descriptor_send(d, "It's not even closed.\r\n");
            return true;
        }
        if (!(r->exit_cond[dir] & EXIT_COND_LOCKED)) {
            descriptor_send(d, "It's already unlocked.\r\n");
            return true;
        }
        descriptor_send(d, "You begin fiddling with the lock.\r\n");
        char roommsg[128];
        snprintf(roommsg, sizeof(roommsg), "%s begins fiddling with a lock.\r\n", ch->base.name);
        descriptor_room_echo(r, ch, roommsg);
        if (!skill_roll_success(prof)) {
            descriptor_send(d, "You fiddle with the lock, but it doesn't budge.\r\n");
            return true;
        }
        r->exit_cond[dir] &= ~EXIT_COND_LOCKED;
        room_repo_save_exit(r->vnum, dir, r->exits[dir], r->exit_door[dir], r->exit_cond[dir]);
        descriptor_send(d, "The lock quickly yields to your skills.\r\n*Click*\r\n");
        return true;
    }

    obj_t *cont = find_container(ch, tok);
    if (!cont) {
        descriptor_send(d, "You don't see that here.\r\n");
        return true;
    }
    const char *label = cont->base.short_descr[0] ? cont->base.short_descr : cont->base.name;
    if (!(cont->val[1] & CONT_CLOSED)) {
        descriptor_send(d, "It's not even closed.\r\n");
        return true;
    }
    if (!(cont->val[1] & CONT_LOCKED)) {
        descriptor_send(d, "It's already unlocked.\r\n");
        return true;
    }
    if (cont->val[1] & CONT_PICKPROOF) {
        descriptor_send(d, "This lock is far too intricate to pick.\r\n");
        return true;
    }
    char roommsg[320];
    snprintf(roommsg, sizeof(roommsg), "%s begins fiddling with a lock on %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, roommsg);
    char selfmsg[320];
    snprintf(selfmsg, sizeof(selfmsg), "You begin fiddling with the lock on %s.\r\n", label);
    descriptor_send(d, selfmsg);
    if (!skill_roll_success(prof)) {
        descriptor_send(d, "You fiddle with the lock, but it doesn't budge.\r\n");
        return true;
    }
    cont->val[1] &= ~CONT_LOCKED;
    char okmsg[192];
    snprintf(okmsg, sizeof(okmsg), "The lock on %s quickly yields to your skills.\r\n*Click*\r\n", label);
    descriptor_send(d, okmsg);
    return true;
}
