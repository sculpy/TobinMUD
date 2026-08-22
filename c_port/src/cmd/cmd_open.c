/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "combat.h"
#include "obj.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"
#include "world.h"

/* `open`/`close <direction>`: the door-type/condition data an exit already
 * carries (set via `edroom`) finally does something.
 *
 * **Door state now syncs across both sides** (TODO.md priority item,
 * user 2026-07-30: "synchronize door states so opening/closing a door
 * affects both sides" -- an explicit, deliberate reversal of an earlier
 * documented decision, not a bug fix; see sync_reverse_door()'s own doc
 * comment for the exact scope). Each exit still CARRIES its own
 * independent door_type/exit_cond storage (`edroom`'s auto-created
 * reverse exit still gets its own doorless bitmask by default, unchanged)
 * -- only the CLOSED bit is actively kept in lockstep once BOTH sides
 * genuinely have a door of their own, treating that as "the same
 * physical door" viewed from two rooms. LOCKED and SECRET are
 * deliberately NOT synced: LOCKED is cmd_lock.c's own separate concern
 * (open/close never touches it on either side, so there's nothing of
 * this file's own to mirror), and a door can be secret from one side but
 * obvious from the other by design (a hidden panel, say), independent
 * of open/closed state. Locked doors can only be opened once the Locked
 * bit is cleared -- via `edroom`'s toggle submenu, or for real, with a
 * key: see cmd_lock.c's `lock`/`unlock`. */

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
 * floor -- the same search order `put`/`get <container>` use. Supports the
 * "N.keyword" ordinal prefix (user 2026-07-18: "make it true as part of
 * everything that can exist"), same convention as cmd_object.c's own
 * find_obj() -- parsed once here, per chain in sequence (not pooled across
 * both), same as find_obj's own single-chain-per-call contract. */
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
        /* `set trap (container)` (missing-skill audit, 2026-08-09): mirrors
         * cmd_move.c's own EXIT_COND_TRAPPED door-spring exactly -- "detect
         * trap" spots and safely steps around it (leaving it rigged for
         * whoever opens it next), everyone else springs it, one-shot. */
        if (o->val[1] & CONT_TRAPPED) {
            if (being_knows_skill(ch, "detect trap")) {
                descriptor_send(d, "You spot a trap rigged inside and carefully avoid it.\r\n");
                /* Learn-by-doing: using the skill trains it toward its discipline
                 * ceiling (skill_learn_from_doing() self-throttles via its own
                 * cooldown). PCs only; immortals already read as maxed. */
                if (!being_is_immortal(ch) && ch->base.kind == THING_PC) {
                    const skill_def_t *learn_sk = skill_find(ch->char_class, "detect trap", true);
                    if (learn_sk)
                        skill_learn_from_doing(ch, learn_sk);
                }
            } else {
                int dmg = 5 + rand() % 10;
                limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
                int limb_hp_before = ch->limbs[limb].hp;
                being_hurt_limb(ch, limb, dmg);
                char trap_msg[200];
                snprintf(trap_msg, sizeof(trap_msg),
                         "A trap rigged inside %s springs! It catches your %s %s!\r\n",
                         label, limb_name(limb), describe_dam(dmg, limb_hp_before, NULL));
                descriptor_send(d, trap_msg);
                o->val[1] &= ~CONT_TRAPPED;
            }
        }
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

/* If room `r`'s exit `dir` (leading to `r->exits[dir]`) has a genuine
 * door on the OTHER side too -- the destination room's reverse-direction
 * exit (REV_DIR[dir]) points back to `r->vnum` and itself has a door_type
 * set -- mirrors the new CLOSED state onto that side and persists
 * it, then echoes the change to anyone standing there. A missing reverse
 * exit, a reverse exit pointing somewhere else, or a doorless reverse
 * exit are all left completely alone (no door forced onto a side that
 * doesn't have one) -- see this file's top comment for the SECRET-bit
 * exclusion. Loads the destination room via room_repo_load() if it isn't
 * already in memory (same "lazy load, register, fall through" pattern
 * enter_world()/descriptor_copyover_adopt() already use elsewhere) so a
 * door syncs correctly even if nobody's currently standing on the far
 * side to have loaded it first. */
static void sync_reverse_door(room_t *r, int dir, bool opening) {
    int dest_vnum = r->exits[dir];
    if (dest_vnum < 0)
        return;

    room_t *far = world_get_room(dest_vnum);
    if (!far) {
        far = room_repo_load(dest_vnum);
        if (far)
            world_register_room(far);
    }
    if (!far)
        return;

    int rev = REV_DIR[dir];
    if (far->exits[rev] != r->vnum || far->exit_door[rev] == 0)
        return;

    /* Only the CLOSED bit -- exactly what the near side's own code above
     * touches (LOCKED is cmd_lock.c's separate concern, never modified by
     * open/close on either side). */
    if (opening)
        far->exit_cond[rev] &= ~EXIT_COND_CLOSED;
    else
        far->exit_cond[rev] |= EXIT_COND_CLOSED;

    room_repo_save_exit(far->vnum, rev, far->exits[rev], far->exit_door[rev], far->exit_cond[rev]);

    char door[16];
    snprintf(door, sizeof(door), "%s", door_type_name(far->exit_door[rev]));
    for (char *p = door; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    char echo[128];
    snprintf(echo, sizeof(echo), "The %s to the %s %s.\r\n",
             door, DIR_NAMES[rev], opening ? "opens" : "closes");
    descriptor_room_echo(far, NULL, echo);
}

/* Shared implementation behind `open`/`close`: resolves the argument as a
 * direction (a door, with the "door [<direction>]" alternate syntax
 * handled below), or falls through to a carried/room container, then
 * dispatches to the door or do_container() open/close logic accordingly. */
static bool do_openclose(descriptor_t *d, const char *args, bool opening) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Usage: %s <direction|door [direction]|container>\r\n", opening ? "open" : "close");
        descriptor_send(d, msg);
        return true;
    }

    room_t *r = ch->base.roomp;
    int dir = parse_dir(tok);

    /* "door <direction>", or bare "door" (user report: "open dootr
     * doesnt work" -- Tobin had only ever ported the bare-direction
     * half of Sneezy's documented syntax, lib/help/open: "open door
     * north", "open door east", ...). Tried only once the first token
     * fails to parse as a direction outright, so it can never shadow a
     * real direction abbreviation -- "door" and "down" share a prefix,
     * and parse_dir() above already gets first crack at every token, so
     * "open d"/"open do" still mean down exactly as before. A bare
     * "door" with no direction opens the room's one door if it has
     * exactly one, matching the original's own "try to determine what
     * you mean" disambiguation for an ambiguous target. */
    if (dir < 0) {
        size_t tok_len = strlen(tok);
        if (tok_len && strncasecmp(tok, "door", tok_len) == 0) {
            char tok2[64];
            if (sscanf(args + tok_len, "%63s", tok2) == 1) {
                dir = parse_dir(tok2);
            } else {
                int found = -1, count = 0;
                for (int i = 0; i < ROOM_NUM_EXITS; i++) {
                    if (r->exits[i] >= 0 && r->exit_door[i] != 0) {
                        found = i;
                        count++;
                    }
                }
                if (count != 1) {
                    descriptor_send(d, count == 0 ? "There is no door here.\r\n"
                                                  : "Which door? Try 'open door <direction>'.\r\n");
                    return true;
                }
                dir = found;
            }
        }
    }

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
        sync_reverse_door(r, dir, opening);

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
