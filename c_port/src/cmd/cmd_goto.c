/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "cmd.h"
#include "obj.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* Substitutes `goto`'s bamfin/bamfout tokens into `tmpl`, rendering into
 * `out`: `$g`/`$$g` (room's ground-surface word, obj_apply_ground_token())
 * and `$p` (gender_possess() pronoun) -- same idea as cmd_move.c's
 * poofin/poofout tokens, but no `$d` (a teleport has no direction) -- plus
 * `<N>`/`<n>` (the mover's name, same convention as a player's `title`,
 * cmd_who.c's title_with_name()), which may appear anywhere in the
 * message. Returns true iff `<N>`/`<n>` was found and substituted, so the
 * caller knows the name is already embedded and shouldn't prefix it again
 * (user 2026-07-11: "bamfin|out should modify goto messaging"; follow-up:
 * "<N> should work in this as well as $g"; "and $p"). */
static bool apply_bamf_tokens(const char *tmpl, room_t *room, gender_t gender,
                               const char *name, char *out, size_t outsz) {
    char stage[BEING_BAMF_LEN + 64];
    obj_apply_ground_token(tmpl, room, stage, sizeof(stage));

    size_t o = 0;
    bool named = false;
    for (size_t i = 0; stage[i] != '\0' && o + 1 < outsz;) {
        if (stage[i] == '<' && (stage[i + 1] == 'N' || stage[i + 1] == 'n')
            && stage[i + 2] == '>') {
            for (const char *p = name; *p && o + 1 < outsz; p++)
                out[o++] = *p;
            i += 3;
            named = true;
        } else if (stage[i] == '$' && stage[i + 1] == 'p') {
            for (const char *p = gender_possess(gender); *p && o + 1 < outsz; p++)
                out[o++] = *p;
            i += 2;
        } else {
            out[o++] = stage[i++];
        }
    }
    out[o < outsz ? o : outsz - 1] = '\0';
    return named;
}

/* Broadcasts `tmpl` (bamfout for a departure, bamfin for an arrival) to
 * everyone else in `room`, falling back to the default puff-of-smoke
 * wording when empty. */
static void announce_bamf(being_t *ch, room_t *room, const char *tmpl, bool arriving) {
    char body[BEING_BAMF_LEN + 96];
    char msg[256];
    if (tmpl[0]) {
        bool named = apply_bamf_tokens(tmpl, room, ch->gender, ch->base.name, body, sizeof(body));
        if (named)
            snprintf(msg, sizeof(msg), "%s\r\n", body);
        else
            snprintf(msg, sizeof(msg), "%s %s.\r\n", ch->base.name, body);
    } else {
        snprintf(msg, sizeof(msg), "%s %s in a puff of smoke.\r\n",
                 ch->base.name, arriving ? "appears" : "disappears");
    }
    descriptor_room_echo(room, ch, msg);
}

/* Teleports `d->character` to room `r`, with the usual bamfout/bamfin
 * announcements and a closing `look` -- the immortal-only vnum/player
 * forms of `goto` (see cmd_goto() below). The mortal-usable landmark
 * forms (guildmaster/rent/surplus) do NOT use this -- see the follow-up
 * comment above goto_bfs() for why. */
static bool goto_room(descriptor_t *d, room_t *r) {
    room_t *from = d->character->base.roomp;
    if (from)
        announce_bamf(d->character, from, d->character->bamfout, false);

    thing_set_room(&d->character->base, r);
    descriptor_send(d, "You vanish in a puff of smoke.\r\n");

    announce_bamf(d->character, r, d->character->bamfin, true);

    return cmd_dispatch(d, "look");
}

/* Mortal-usable `goto` landmarks (user 2026-07-12, first round: "add a
 * goto class function that mortals can do to help find thier
 * guildmasters"; second round: "goto guildmaster should give them
 * directions, not transfer. also add a goto rent, goto surplus for now
 * with goto expanding for mortals") -- these give a walking direction
 * list to the nearest matching landmark instead of teleporting, unlike
 * the immortal vnum/player forms below. `rent`/`surplus` target one
 * fixed room each (557 "The Roaring Lion Inn", 563 "Surplus" -- both
 * user-specified 2026-07-12); `guildmaster` targets the nearest room
 * containing a mob keyworded "guildmaster" whose class matches the
 * caller's own -- so it needs a real shortest-path search, not just "any
 * match anywhere" like the original teleporting version had.
 *
 * Most of the ~20,500-room map isn't resident in world.c's cache at any
 * given moment -- rooms are loaded lazily, on demand (a zone reset
 * touching them, a player walking in, an immortal `goto <vnum>`), same
 * "get-or-load" pattern as zone.c's zone_get_room() and cmd_goto()'s own
 * immortal vnum branch below. goto_bfs() below MUST do the same lazy
 * load as it discovers each room via exits -- an earlier draft instead
 * pre-indexed only whatever was already resident, which meant the
 * search silently dead-ended at the edge of loaded territory and
 * reported "you don't know where to find" for perfectly real,
 * connected, just-not-yet-loaded guildmasters. `goto_get_room()` below
 * is that get-or-load step; `world_get_room()` (world.c) is a linear
 * scan over a linked list, so it's still not cheap to call per BFS edge,
 * but it only runs once per NEWLY discovered vnum (the search's own
 * open-addressed visited-set stops it from ever repeating one), not
 * once per edge examined. */
#define GOTO_HASH_SIZE 65536 /* power of 2; ~20.5k real rooms -> ~31% load factor */
#define GOTO_MAX_PATH 200    /* hard cap on reported hop count, plenty for any real route */

typedef struct {
    int vnum;      /* -1 = empty slot (not yet reached by this search) */
    bool visited;
    int prev_vnum;
    int dir_from_prev; /* direction taken FROM prev_vnum TO this room, -1 at the start room */
} goto_node_t;

static goto_node_t g_goto_nodes[GOTO_HASH_SIZE];
static int g_goto_queue[GOTO_HASH_SIZE];

static unsigned goto_hash_slot(int vnum) {
    unsigned idx = ((unsigned)vnum * 2654435761u) & (GOTO_HASH_SIZE - 1);
    while (g_goto_nodes[idx].vnum != -1 && g_goto_nodes[idx].vnum != vnum)
        idx = (idx + 1) & (GOTO_HASH_SIZE - 1);
    return idx;
}

/* Get-or-load a room by vnum -- same 3-step pattern as every other
 * room-lookup call site in the codebase (zone.c's zone_get_room(),
 * descriptor.c, cmd_goto()'s own immortal vnum branch below). */
static room_t *goto_get_room(int vnum) {
    room_t *r = world_get_room(vnum);
    if (!r) {
        r = room_repo_load(vnum);
        if (r)
            world_register_room(r);
    }
    return r;
}

/* Breadth-first search outward from `start_vnum` over room exits until
 * `is_goal` returns true for some reached room, or the reachable graph
 * is exhausted. On success, fills `dirs`/`*dir_count` with the
 * start->goal direction sequence and returns the goal room; NULL if
 * nothing reachable satisfies `is_goal`. */
static room_t *goto_bfs(int start_vnum, bool (*is_goal)(room_t *r),
                         int *dirs, int *dir_count) {
    for (int i = 0; i < GOTO_HASH_SIZE; i++)
        g_goto_nodes[i].vnum = -1;

    unsigned sidx = goto_hash_slot(start_vnum);
    g_goto_nodes[sidx].vnum = start_vnum;
    g_goto_nodes[sidx].visited = true;
    g_goto_nodes[sidx].prev_vnum = -1;
    g_goto_nodes[sidx].dir_from_prev = -1;

    int head = 0, tail = 0;
    g_goto_queue[tail++] = start_vnum;

    room_t *goal = NULL;
    while (head < tail && !goal) {
        int cur_vnum = g_goto_queue[head++];
        room_t *cur = goto_get_room(cur_vnum);
        if (!cur)
            continue;
        if (is_goal(cur)) {
            goal = cur;
            break;
        }
        for (int dir = 0; dir < ROOM_NUM_EXITS; dir++) {
            int dest = cur->exits[dir];
            if (dest < 0)
                continue;
            unsigned didx = goto_hash_slot(dest);
            if (g_goto_nodes[didx].vnum == dest && g_goto_nodes[didx].visited)
                continue;
            g_goto_nodes[didx].vnum = dest;
            g_goto_nodes[didx].visited = true;
            g_goto_nodes[didx].prev_vnum = cur_vnum;
            g_goto_nodes[didx].dir_from_prev = dir;
            if (tail < GOTO_HASH_SIZE)
                g_goto_queue[tail++] = dest;
        }
    }

    if (!goal)
        return NULL;

    int rev[GOTO_MAX_PATH];
    int n = 0;
    int cur_vnum = goal->vnum;
    while (n < GOTO_MAX_PATH) {
        unsigned idx = goto_hash_slot(cur_vnum);
        int dir = g_goto_nodes[idx].dir_from_prev;
        if (dir < 0)
            break;
        rev[n++] = dir;
        cur_vnum = g_goto_nodes[idx].prev_vnum;
    }
    for (int i = 0; i < n; i++)
        dirs[i] = rev[n - 1 - i];
    *dir_count = n;
    return goal;
}

static void goto_format_directions(const int *dirs, int count, char *out, size_t outsz) {
    size_t n = 0;
    for (int i = 0; i < count && n < outsz; i++)
        n += (size_t)snprintf(out + n, outsz - n, "%s%s",
                              DIR_NAMES[dirs[i]], i + 1 < count ? ", " : "");
}

/* Runs goto_bfs() from the caller's current room and sends a direction
 * listing (or an "already there"/"not found" message) instead of
 * teleporting. `label` describes the destination in the message ("a
 * guildmaster of your discipline", "the inn", "the surplus store"). */
static bool goto_send_directions(descriptor_t *d, bool (*is_goal)(room_t *r),
                                  const char *label, const char *not_found_msg) {
    room_t *start = d->character->base.roomp;
    if (!start) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    int dirs[GOTO_MAX_PATH];
    int dir_count = 0;
    room_t *goal = goto_bfs(start->vnum, is_goal, dirs, &dir_count);

    if (!goal) {
        descriptor_send(d, not_found_msg);
        return true;
    }
    if (dir_count == 0) {
        char msg[160];
        snprintf(msg, sizeof(msg), "You're already there -- %s is right here!\r\n", label);
        descriptor_send(d, msg);
        return true;
    }

    char dirbuf[2300];
    goto_format_directions(dirs, dir_count, dirbuf, sizeof(dirbuf));

    char msg[2500];
    snprintf(msg, sizeof(msg), "To reach %s: %s (%d room%s away).\r\n",
             label, dirbuf, dir_count, dir_count == 1 ? "" : "s");
    descriptor_send(d, msg);
    return true;
}

/* `goto guildmaster`: nearest room containing a mob keyworded
 * "guildmaster" whose class matches the caller's own -- same match rule
 * cmd_practice.c's own room-scoped find_guildmaster() uses, just
 * evaluated per-room during the BFS instead of world-wide in one pass,
 * since the teleporting version's old world_for_each_mob() scan gave no
 * way to know which match was actually closest. */
static player_class_t g_gm_search_class;

static bool goto_is_guildmaster_room(room_t *r) {
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *m = (being_t *)t;
        if (!m->mob_class_known || m->char_class != g_gm_search_class)
            continue;
        if (thing_name_matches(m->base.name, "guildmaster", strlen("guildmaster")))
            return true;
    }
    return false;
}

static bool goto_guildmaster(descriptor_t *d) {
    g_gm_search_class = d->character->char_class;
    return goto_send_directions(d, goto_is_guildmaster_room,
        "a guildmaster of your discipline",
        "You don't know where to find a guildmaster of your discipline.\r\n");
}

/* `goto rent`/`goto surplus`: one fixed target room each (user
 * 2026-07-12: "surplus room is 563"; "room 557 (The Roaring Lion Inn)"
 * for rent). */
#define GOTO_RENT_ROOM_VNUM 557
#define GOTO_SURPLUS_ROOM_VNUM 563

static int g_goto_target_vnum;

static bool goto_is_target_vnum(room_t *r) {
    return r->vnum == g_goto_target_vnum;
}

static bool goto_rent(descriptor_t *d) {
    g_goto_target_vnum = GOTO_RENT_ROOM_VNUM;
    return goto_send_directions(d, goto_is_target_vnum, "the inn",
        "You don't know how to get there from here.\r\n");
}

static bool goto_surplus(descriptor_t *d) {
    g_goto_target_vnum = GOTO_SURPLUS_ROOM_VNUM;
    return goto_send_directions(d, goto_is_target_vnum, "the surplus store",
        "You don't know how to get there from here.\r\n");
}

/* `goto <vnum>` or `goto <player>`: immortal-only teleport. A number goes
 * straight to that room; a name goes to that online being's current room
 * (players now; mobs once they exist). Mirrors the original doGoto's
 * vnum-or-name behavior (cmd/cmd_goto.cc). Room lookup mirrors enter_world()'s
 * lazy load-and-register pattern.
 *
 * Mortal-usable in three landmark forms (guildmaster/rent/surplus, all
 * above) -- the command's table-level gate was lowered to mortal so
 * those reach them, but any other form (vnum or player name) still
 * refuses a mortal caller outright. */
bool cmd_goto(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Usage: goto guildmaster|rent|surplus   |   goto <room vnum | player name> (immortals)\r\n");
        return true;
    }

    {
        char first[24];
        sscanf(args, "%23s", first);
        size_t flen = strlen(first);
        if (flen && strncasecmp(first, "guildmaster", flen) == 0)
            return goto_guildmaster(d);
        if (flen && strncasecmp(first, "rent", flen) == 0)
            return goto_rent(d);
        if (flen && strncasecmp(first, "surplus", flen) == 0)
            return goto_surplus(d);
    }

    if (!being_is_immortal(d->character)) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    room_t *r = NULL;

    if (isdigit((unsigned char)args[0])) {
        int vnum = atoi(args);
        r = world_get_room(vnum);
        if (!r) {
            r = room_repo_load(vnum);
            if (r)
                world_register_room(r);
        }
        if (!r) {
            char msg[80];
            snprintf(msg, sizeof(msg), "No room with vnum %d exists.\r\n", vnum);
            descriptor_send(d, msg);
            return true;
        }
    } else {
        /* Teleport to an online being by name (case-insensitive prefix). */
        char tok[64];
        sscanf(args, "%63s", tok);
        size_t len = strlen(tok);
        being_t *target = NULL;
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (it->character && it->character != d->character
                && it->character->base.roomp
                && strncasecmp(it->character->base.name, tok, len) == 0) {
                target = it->character;
                break;
            }
        }
        if (!target) {
            char msg[128];
            snprintf(msg, sizeof(msg), "No one named '%s' is in the game.\r\n", tok);
            descriptor_send(d, msg);
            return true;
        }
        r = target->base.roomp;
    }

    return goto_room(d, r);
}
