/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "mob_ai.h"
#include "room.h"
#include "skill.h"
#include "thing.h"
#include "world.h"

/* How many hops out track will read a trail -- matches mob_ai.c's own
 * HUNT_MAX_DEPTH (that constant is file-static there, not exported). */
#define TRACK_MAX_DEPTH 24

/* `track <name>` (Unimplemented skills/spells backlog, Session 158 audit:
 * Thief, skill.c level 25). Real upstream (disc_thief_stealth.cc's
 * doTrack) reads a quarry's trail and points the hunter one step toward
 * them. Tobin already has the exact pathfinding primitive this needs --
 * mob_path_next_dir() (mob_ai.c), the BFS a hunting mob uses to chase a
 * target one hop per round -- so track reuses it: find the named being,
 * then report the first-hop direction from here toward their room.
 *
 * Search scope is every room currently loaded in memory (world_for_
 * each_room) -- Tobin loads the world lazily (world.h), so this covers
 * wherever players are and anywhere that's been visited, a disclosed
 * scope-cut from the original's full-world trail. The world_for_each_
 * room() callback carries no context arg, so the search hands off through
 * a file-static (single-threaded MUD, same pattern the pulse callbacks
 * use). */

static struct {
    const char *tok;
    size_t tok_len;
    being_t *self;
    being_t *found;
} track_search;

static void track_scan_room(room_t *r) {
    if (track_search.found)
        return;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        being_t *b = (being_t *)t;
        if (b == track_search.self)
            continue;
        if (thing_name_matches(t->name, track_search.tok, track_search.tok_len)) {
            track_search.found = b;
            return;
        }
    }
}

bool cmd_track(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "track")) {
        descriptor_send(d, "You don't know how to track a trail.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Track whom? (track <name>)\r\n");
        return true;
    }

    /* Learn-by-doing on every real attempt, win or lose. */
    if (!being_is_immortal(ch) && ch->base.kind == THING_PC) {
        const skill_def_t *sk = skill_find(ch->char_class, "track", false);
        if (sk)
            skill_learn_from_doing(ch, sk);
    }

    track_search.tok = tok;
    track_search.tok_len = strlen(tok);
    track_search.self = ch;
    track_search.found = NULL;
    world_for_each_room(track_scan_room);

    being_t *quarry = track_search.found;
    if (!quarry || !quarry->base.roomp) {
        descriptor_send(d, "You search for a trail, but find no sign of them.\r\n");
        return true;
    }

    char msg[160];
    if (quarry->base.roomp == ch->base.roomp) {
        snprintf(msg, sizeof(msg), "%s is right here with you!\r\n", being_display_name(quarry));
        descriptor_send(d, msg);
        return true;
    }

    /* `concealment` (Thief, level 30, passive -- Session 158 backlog). A
     * quarry who knows concealment automatically covers their trail: the
     * scent goes cold and track can't follow it cross-room (they're still
     * plainly visible if you're in the SAME room -- concealment hides the
     * trail, not the person, so this sits after the co-located check
     * above). An immortal tracker sees through it, same all-seeing
     * convention as search/hide/invisible. Exercising it trains the
     * concealed being's own skill. */
    if (!being_is_immortal(ch) && !being_is_immortal(quarry)
        && being_knows_skill(quarry, "concealment")) {
        if (quarry->base.kind == THING_PC) {
            const skill_def_t *csk = skill_find(quarry->char_class, "concealment", false);
            if (csk)
                skill_learn_from_doing(quarry, csk);
        }
        snprintf(msg, sizeof(msg), "You pick up %s's trail, but it goes cold -- they've covered their tracks.\r\n",
                 being_display_name(quarry));
        descriptor_send(d, msg);
        return true;
    }

    int dir = mob_path_next_dir(ch->base.roomp, quarry->base.roomp->vnum, TRACK_MAX_DEPTH);
    if (dir < 0 || dir >= ROOM_NUM_EXITS) {
        snprintf(msg, sizeof(msg), "You catch %s's scent, but can't work out a way to reach them.\r\n",
                 being_display_name(quarry));
        descriptor_send(d, msg);
        return true;
    }

    snprintf(msg, sizeof(msg), "You pick up %s's trail -- it leads %s.\r\n",
             being_display_name(quarry), DIR_NAMES[dir]);
    descriptor_send(d, msg);
    return true;
}
