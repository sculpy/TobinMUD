/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "world.h"

#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "player_repo.h"

typedef struct room_entry {
    room_t *room;
    struct room_entry *next;
} room_entry_t;

static room_entry_t *g_rooms = NULL;

/* Registers a lazily-loaded room in the in-memory world (see world.h's
 * top-of-file note on Phase 1's on-demand load model). Takes ownership
 * of `r`; if a room with the same vnum is already registered, the old
 * one is destroyed and replaced rather than duplicated. */
void world_register_room(room_t *r) {
    if (!r)
        return;

    for (room_entry_t *e = g_rooms; e; e = e->next) {
        if (e->room->vnum == r->vnum) {
            if (e->room != r) {
                room_destroy(e->room);
                e->room = r;
            }
            return;
        }
    }

    room_entry_t *e = malloc(sizeof(*e));
    e->room = r;
    e->next = g_rooms;
    g_rooms = e;
}

/* Returns the already-registered room for `vnum`, or NULL if it hasn't
 * been loaded yet. */
room_t *world_get_room(int vnum) {
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        if (e->room->vnum == vnum)
            return e->room;
    }
    return NULL;
}

/* Count of rooms currently loaded into memory (`stats` command) -- see
 * world.h's doc comment. */
int world_count_loaded_rooms(void) {
    int count = 0;
    for (room_entry_t *e = g_rooms; e; e = e->next)
        count++;
    return count;
}

/* Searches every registered room for a linkdead PC (desc == NULL) whose
 * player_id matches, so a reconnect can resume the same live being_t
 * instead of loading a fresh one from the DB. Returns NULL if that
 * player isn't sitting linkdead anywhere. */
being_t *world_find_linkdead_pc(long player_id) {
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        for (thing_t *t = e->room->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC)
                continue;
            being_t *b = (being_t *)t;
            if (!b->desc && b->player_id == player_id)
                return b;
        }
    }
    return NULL;
}

/* Searches every registered room for any PC (active or linkdead) matching
 * player_id. Returns NULL if no such PC exists. Used at login to detect and
 * prevent duplicate character instances (same player logged in twice
 * simultaneously). Unlike world_find_linkdead_pc(), this includes active
 * connections (desc != NULL). */
being_t *world_find_active_pc(long player_id) {
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        for (thing_t *t = e->room->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC)
                continue;
            being_t *b = (being_t *)t;
            if (b->player_id == player_id)
                return b;
        }
    }
    return NULL;
}

/* Force-removes every linkdead PC in every registered room -- backs
 * `purge linkdead` (cmd_purge.c). Deliberately does NOT save first (see
 * world.h): discards the body the same way it would eventually be
 * discarded anyway. Returns how many were removed. */
int world_purge_linkdead(void) {
    int count = 0;
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        thing_t *t = e->room->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next; /* being_destroy() frees t -- save next first */
            if (t->kind == THING_PC) {
                being_t *b = (being_t *)t;
                if (!b->desc) {
                    being_destroy(b);
                    count++;
                }
            }
            t = next;
        }
    }
    return count;
}

/* Read-only count of linkdead PCs across every registered room -- same
 * scope as world_purge_linkdead() but never removes anything. Used by
 * `who` to report bodies left behind by lost connections. */
int world_count_linkdead(void) {
    int count = 0;
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        for (thing_t *t = e->room->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC)
                continue;
            being_t *b = (being_t *)t;
            if (!b->desc)
                count++;
        }
    }
    return count;
}

/* Force-removes every linkdead PC that's been linkdead at least
 * max_age_seconds, force-saving each one (player_save()) first -- unlike
 * world_purge_linkdead() above, since this runs unattended off a pulse
 * rather than an immortal's on-demand command. Returns how many were
 * removed. */
int world_purge_stale_linkdead(int max_age_seconds) {
    int count = 0;
    time_t now = time(NULL);
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        thing_t *t = e->room->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next; /* being_destroy() frees t -- save next first */
            if (t->kind == THING_PC) {
                being_t *b = (being_t *)t;
                if (!b->desc && now - b->linkdead_since >= max_age_seconds) {
                    player_save(b->player_id, b);
                    being_destroy(b);
                    count++;
                }
            }
            t = next;
        }
    }
    return count;
}

/* Pulse callback (registered in main.c) that drives world_purge_stale_
 * linkdead() using the configured threshold (TOBIN_LINKDEAD_PURGE_SECONDS,
 * default 300s) instead of a hardcoded one, so a smoke test can shorten
 * it. */
void linkdead_purge_tick(long pulse_num) {
    (void)pulse_num;
    world_purge_stale_linkdead(config_get()->linkdead_purge_seconds);
}

/* Calls `visit(m)` for every mob in every registered room -- the
 * iteration primitive mob_ai.c's pulse-driven wander/scavenge logic runs
 * on each tick. Saves each next-pointer before calling visit(), since
 * visit() may relocate the mob via thing_set_room() mid-walk. */
void world_for_each_mob(void (*visit)(being_t *m)) {
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        thing_t *t = e->room->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next; /* visit() may relocate t via thing_set_room() */
            if (t->kind == THING_MOB)
                visit((being_t *)t);
            t = next;
        }
    }
}

/* Calls `visit(o)` for every object in every registered room -- used by
 * obj.c's pool-decay pulse tick. Same safe-next-pointer iteration as
 * world_for_each_mob(), since decaying a pool to 0 size destroys it
 * mid-walk. */
void world_for_each_obj(void (*visit)(obj_t *o)) {
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        thing_t *t = e->room->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next; /* visit() may destroy t (obj_destroy()) */
            if (t->kind == THING_OBJ)
                visit((obj_t *)t);
            t = next;
        }
    }
}

/* Calls `visit(r)` for every registered room -- used by trigger.c's
 * random-tick pulse to roll each room's ambient "random" trigger. */
void world_for_each_room(void (*visit)(room_t *r)) {
    room_entry_t *e = g_rooms;
    while (e) {
        room_entry_t *next = e->next; /* visit() could in principle re-register/replace e->room */
        visit(e->room);
        e = next;
    }
}
