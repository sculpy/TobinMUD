/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
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
 * springs it on `open`.
 *
 * `set trap (arrow)` (2026-08-22, once cmd_shoot.c's ammo subsystem
 * existed to hang it on): `settrap arrow [item]` rigs a carried loose
 * arrow (obj.h's ARROW_TRAPPED, val[0] -- arrows have no other val[]
 * use). cmd_shoot.c springs it on a landed hit, same flat
 * random-limb damage as the door trap, then the arrow is destroyed as
 * ammo normally is either way.
 *
 * `set trap (mine)` (2026-08-22, user decision -- build both, scoped
 * down from upstream's crafting-task version): `settrap mine` rigs
 * the CURRENT ROOM's own floor (room.h's `mine_trapped`, a Tobin-only
 * field -- not part of the original's verbatim room_flag bit layout).
 * No item, no direction -- unlike a door trap, a landmine has no
 * specific exit to sit on. cmd_move.c springs it on arrival from ANY
 * direction, same flat random-limb damage and "detect trap" dodge as
 * the door trap.
 *
 * `set trap (grenade)` (2026-08-22, same decision): `settrap grenade
 * [item]` rigs a carried loose grenade-keyword item (obj.h's
 * GRENADE_TRAPPED, same val[0] bit as ARROW_TRAPPED -- ammo and
 * throwables never share one object, so reusing the bit is safe).
 * cmd_throw.c's new `throw` command springs it on a landed hit.
 *
 * Both are disclosed scope-downs from upstream's own mine/grenade,
 * which are built via a multi-item crafting task (`hasTrapComps()`,
 * trap.cc) with a dozen elemental damage-type choices -- ported here
 * as the same single flat-damage rig every other Tobin trap already
 * is, not the crafting minigame. */

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

/* Case-insensitive "does haystack contain needle" (strcasestr is
 * GNU-only; do it by hand -- same shape as cmd_shoot.c/cmd_who.c's own
 * file-local copies). */
static bool trap_sc_contains(const char *haystack, const char *needle) {
    if (!haystack || !needle)
        return false;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}
static bool trap_obj_kw(const obj_t *o, const char *kw) {
    return trap_sc_contains(o->base.name, kw) || trap_sc_contains(o->base.short_descr, kw);
}
/* A loose (carried, not held/worn -- arrows are never worn) arrow among
 * ch's own inventory matching `tok`, or -- with tok empty/"arrow" itself
 * -- the first loose arrow found. Room floor is NOT searched: unlike a
 * door or a room's own container, an arrow trap only makes sense on
 * ammunition you are about to nock and fire yourself. */
static obj_t *find_arrow(being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!trap_obj_kw(o, "arrow"))
            continue;
        if (len == 0 || strncasecmp(tok, "arrow", len) == 0
            || thing_name_matches(t->name, tok, len))
            return o;
    }
    return NULL;
}

/* Same shape as find_arrow() above, matching a "grenade"-keyword loose
 * carried item instead. */
static obj_t *find_grenade(being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!trap_obj_kw(o, "grenade"))
            continue;
        if (len == 0 || strncasecmp(tok, "grenade", len) == 0
            || thing_name_matches(t->name, tok, len))
            return o;
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
            obj_t *arrow = find_arrow(ch, tok);
            if (arrow) {
                if (!being_knows_skill(ch, "set trap (arrow)")) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                if (arrow->val[0] & ARROW_TRAPPED) {
                    descriptor_send(d, "It's already rigged.\r\n");
                    return true;
                }
                bool imm = being_is_immortal(ch);
                const skill_def_t *sk = skill_find(ch->char_class, "set trap (arrow)", imm);
                if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
                    descriptor_send(d, "You fumble rigging the arrowhead -- it doesn't take.\r\n");
                    return true;
                }
                arrow->val[0] |= ARROW_TRAPPED;
                const char *label = arrow->base.short_descr[0] ? arrow->base.short_descr : arrow->base.name;
                char msg[256];
                snprintf(msg, sizeof(msg), "You carefully rig a trap into %s.\r\n", label);
                descriptor_send(d, msg);
                return true;
            }
            obj_t *grenade = find_grenade(ch, tok);
            if (grenade) {
                if (!being_knows_skill(ch, "set trap (grenade)")) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                if (grenade->val[0] & GRENADE_TRAPPED) {
                    descriptor_send(d, "It's already rigged.\r\n");
                    return true;
                }
                bool imm = being_is_immortal(ch);
                const skill_def_t *sk = skill_find(ch->char_class, "set trap (grenade)", imm);
                if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
                    descriptor_send(d, "You fumble rigging the grenade -- it doesn't take.\r\n");
                    return true;
                }
                grenade->val[0] |= GRENADE_TRAPPED;
                const char *label = grenade->base.short_descr[0] ? grenade->base.short_descr : grenade->base.name;
                char msg[256];
                snprintf(msg, sizeof(msg), "You carefully rig a trap into %s.\r\n", label);
                descriptor_send(d, msg);
                return true;
            }
            size_t tlen = strlen(tok);
            if (tlen > 0 && strncasecmp(tok, "mine", tlen) == 0) {
                if (!being_knows_skill(ch, "set trap (mine)")) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                if (ch->base.roomp->mine_trapped) {
                    descriptor_send(d, "This room is already rigged.\r\n");
                    return true;
                }
                bool imm = being_is_immortal(ch);
                const skill_def_t *sk = skill_find(ch->char_class, "set trap (mine)", imm);
                if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
                    descriptor_send(d, "You fumble rigging the mine -- it doesn't take.\r\n");
                    return true;
                }
                ch->base.roomp->mine_trapped = true;
                room_repo_save_mine_trap(ch->base.roomp->vnum, true);
                descriptor_send(d, "You carefully bury a mine in the floor.\r\n");
                return true;
            }
        }
        descriptor_send(d, "Usage: settrap <direction|container|arrow|mine|grenade>\r\n");
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
            obj_t *arrow = find_arrow(ch, tok);
            if (arrow) {
                if (!being_knows_skill(ch, "disarm trap")) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                if (!(arrow->val[0] & ARROW_TRAPPED)) {
                    descriptor_send(d, "There's no trap there.\r\n");
                    return true;
                }
                bool imm = being_is_immortal(ch);
                const skill_def_t *sk = skill_find(ch->char_class, "disarm trap", imm);
                if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
                    descriptor_send(d, "You fumble disarming the arrowhead -- it's still rigged.\r\n");
                    return true;
                }
                arrow->val[0] &= ~ARROW_TRAPPED;
                descriptor_send(d, "You carefully disarm the trap.\r\n");
                return true;
            }
            obj_t *grenade = find_grenade(ch, tok);
            if (grenade) {
                if (!being_knows_skill(ch, "disarm trap")) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                if (!(grenade->val[0] & GRENADE_TRAPPED)) {
                    descriptor_send(d, "There's no trap there.\r\n");
                    return true;
                }
                bool imm = being_is_immortal(ch);
                const skill_def_t *sk = skill_find(ch->char_class, "disarm trap", imm);
                if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
                    descriptor_send(d, "You fumble disarming the grenade -- it's still rigged.\r\n");
                    return true;
                }
                grenade->val[0] &= ~GRENADE_TRAPPED;
                descriptor_send(d, "You carefully disarm the trap.\r\n");
                return true;
            }
            size_t tlen = strlen(tok);
            if (tlen > 0 && strncasecmp(tok, "mine", tlen) == 0) {
                if (!being_knows_skill(ch, "disarm trap")) {
                    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
                    return true;
                }
                if (!ch->base.roomp->mine_trapped) {
                    descriptor_send(d, "There's no trap there.\r\n");
                    return true;
                }
                bool imm = being_is_immortal(ch);
                const skill_def_t *sk = skill_find(ch->char_class, "disarm trap", imm);
                if (!imm && sk && !skill_roll_success(skill_learn_from_doing(ch, sk))) {
                    descriptor_send(d, "You fumble disarming the mine -- it's still rigged.\r\n");
                    return true;
                }
                ch->base.roomp->mine_trapped = false;
                room_repo_save_mine_trap(ch->base.roomp->vnum, false);
                descriptor_send(d, "You carefully disarm the trap.\r\n");
                return true;
            }
        }
        descriptor_send(d, "Usage: disarmtrap <direction|container|arrow|mine|grenade>\r\n");
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
