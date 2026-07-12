/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>

#include <stdlib.h>

#include "being.h"
#include "cmd.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"
#include "trigger.h"
#include "world.h"

/* Fires `to`'s room "enter" triggers, then every mob-in-`to`'s "greet"
 * triggers, for `ch` just having walked in (user, 2026-07-11: "implement
 * mob object and room scripting ... interaction with mobs objs and room
 * via scripts"). Called after the arrival broadcast so any trigger flavor
 * text reads as following "X has arrived", not before it. */
static void run_room_and_greet_triggers(being_t *ch, room_t *to) {
    trigger_t trigs[8];
    int n = trigger_repo_load_for("room", to->vnum, "enter", trigs, 8);
    for (int i = 0; i < n; i++)
        trigger_run(&trigs[i], ch, to, NULL);

    for (thing_t *t = to->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *mob = (being_t *)t;
        trigger_t mtrigs[8];
        int mn = trigger_repo_load_for("mob", mob->base.id, "greet", mtrigs, 8);
        if (mn == 0)
            continue;
        /* short_descr may start with a color tag -- skip it before
         * capitalizing, same bug class fixed elsewhere (cmd_look.c/
         * cmd_object.c/cmd_scan.c/mob_ai.c/trigger.c/combat.c). */
        char capbuf[128];
        snprintf(capbuf, sizeof(capbuf), "%s", mob->base.short_descr);
        size_t ci = 0;
        while (capbuf[ci] == '<' && capbuf[ci + 1] != '\0' && capbuf[ci + 2] == '>')
            ci += 3;
        if (capbuf[ci])
            capbuf[ci] = (char)toupper((unsigned char)capbuf[ci]);
        for (int i = 0; i < mn; i++)
            trigger_run(&mtrigs[i], ch, to, capbuf[0] ? capbuf : NULL);
    }
}

/* Substitutes `$d` (a direction word) and `$p` (the mover's gender_possess()
 * pronoun) into a poofin/poofout template -- see cmd_poof.c's doc comment
 * for the token contract. Used by do_move() below. */
static void apply_poof_tokens(const char *tmpl, const char *dir_word, gender_t gender,
                               char *out, size_t outsz) {
    size_t oi = 0;
    for (const char *p = tmpl; *p && oi + 1 < outsz; p++) {
        if (p[0] == '$' && p[1] == 'd') {
            oi += (size_t)snprintf(out + oi, outsz - oi, "%s", dir_word);
            p++;
        } else if (p[0] == '$' && p[1] == 'p') {
            oi += (size_t)snprintf(out + oi, outsz - oi, "%s", gender_possess(gender));
            p++;
        } else {
            out[oi++] = *p;
        }
    }
    out[oi < outsz ? oi : outsz - 1] = '\0';
}

/* north/east/south/west/up/down -- the first movement commands in the
 * port. Directions are the original dirTypeT's first six slots (see
 * room.h); like classic Diku (and the original's command table), these sit
 * at the very top of COMMANDS[] so the single letters n/e/s/w/u/d always
 * mean movement ("s" is south, not say; "w" is west, not who). */

static bool do_move(descriptor_t *d, int dir) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (ch->fighting) {
        /* Same rule as the original's doMove: no walking out of a fight. */
        descriptor_send(d, "No way! You are fighting for your life!\r\n");
        return true;
    }
    if (ch->position != POSITION_STANDING) {
        /* Must be on your feet to travel (original doMove position gate). */
        descriptor_send(d, "You are in no position to move -- try standing up first.\r\n");
        return true;
    }

    room_t *from = ch->base.roomp;
    int dest = from->exits[dir];
    room_t *to = NULL;
    if (dest >= 0) {
        to = world_get_room(dest);
        if (!to) {
            to = room_repo_load(dest);
            if (to)
                world_register_room(to);
        }
    }
    if (!to) {
        descriptor_send(d, "You can't go that way.\r\n");
        return true;
    }
    if (from->exit_door[dir] != 0 && (from->exit_cond[dir] & EXIT_COND_CLOSED)) {
        descriptor_send(d, "The door is closed.\r\n");
        return true;
    }

    /* Trap mechanics (user 2026-07-11, sequenced after weapon depth): a
     * Thief's "detect trap" skill spots and safely steps around a
     * trapped door, leaving it rigged for the next person -- stepping
     * around it doesn't spring it. Everyone else springs it: one-shot,
     * the trap is gone (both in memory and the DB) once it actually
     * goes off, matching a real trap being a single rigged mechanism,
     * not a renewable hazard. */
    if (from->exit_door[dir] != 0 && (from->exit_cond[dir] & EXIT_COND_TRAPPED)) {
        if (being_knows_skill(ch, "detect trap")) {
            descriptor_send(d, "You spot a trap rigged to the door and carefully step around it.\r\n");
        } else {
            int dmg = 5 + rand() % 10;
            limb_t limb = (limb_t)(rand() % LIMB_COUNT);
            being_hurt_limb(ch, limb, dmg);
            char trap_msg[128];
            /* Damage numbers (user 2026-07-12): hidden from a plain
             * mortal, kept for an immortal (balancing/testing), same
             * rule as combat.c's melee messages. */
            if (being_is_immortal(ch))
                snprintf(trap_msg, sizeof(trap_msg),
                         "A trap rigged to the door springs! It catches your %s for %d damage!\r\n",
                         limb_name(limb), dmg);
            else
                snprintf(trap_msg, sizeof(trap_msg),
                         "A trap rigged to the door springs! It catches your %s!\r\n",
                         limb_name(limb));
            descriptor_send(d, trap_msg);
            from->exit_cond[dir] &= ~EXIT_COND_TRAPPED;
            room_repo_save_exit(from->vnum, dir, from->exits[dir], from->exit_door[dir], from->exit_cond[dir]);
        }
    }

    /* "exits to the north" for compass directions (user-specified
     * phrasing), "exits upward/downward" where "to the up" won't parse. */
    static const char *const EXIT_PHRASES[ROOM_NUM_EXITS] = {
        "exits to the north", "exits to the east", "exits to the south",
        "exits to the west", "exits upward", "exits downward",
        "exits to the northeast", "exits to the northwest",
        "exits to the southeast", "exits to the southwest",
    };
    char msg[256];
    char body[BEING_BAMF_LEN + 32];
    if (ch->poofout[0]) {
        apply_poof_tokens(ch->poofout, DIR_NAMES[dir], ch->gender, body, sizeof(body));
        snprintf(msg, sizeof(msg), "%s %s.\r\n", ch->base.name, body);
    } else {
        snprintf(msg, sizeof(msg), "%s %s.\r\n", ch->base.name, EXIT_PHRASES[dir]);
    }
    descriptor_room_echo(from, ch, msg);

    thing_set_room(&ch->base, to);

    if (ch->poofin[0]) {
        apply_poof_tokens(ch->poofin, DIR_NAMES[REV_DIR[dir]], ch->gender, body, sizeof(body));
        snprintf(msg, sizeof(msg), "%s %s.\r\n", ch->base.name, body);
    } else {
        snprintf(msg, sizeof(msg), "%s has arrived.\r\n", ch->base.name);
    }
    descriptor_room_echo(to, ch, msg);

    run_room_and_greet_triggers(ch, to);

    return cmd_dispatch(d, "look");
}

bool cmd_north(descriptor_t *d, const char *args) { (void)args; return do_move(d, 0); }
bool cmd_east(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 1); }
bool cmd_south(descriptor_t *d, const char *args) { (void)args; return do_move(d, 2); }
bool cmd_west(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 3); }
bool cmd_up(descriptor_t *d, const char *args)    { (void)args; return do_move(d, 4); }
bool cmd_down(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 5); }
bool cmd_northeast(descriptor_t *d, const char *args) { (void)args; return do_move(d, 6); }
bool cmd_northwest(descriptor_t *d, const char *args) { (void)args; return do_move(d, 7); }
bool cmd_southeast(descriptor_t *d, const char *args) { (void)args; return do_move(d, 8); }
bool cmd_southwest(descriptor_t *d, const char *args) { (void)args; return do_move(d, 9); }
