/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "world.h"

#include <stdlib.h>

typedef struct room_entry {
    room_t *room;
    struct room_entry *next;
} room_entry_t;

static room_entry_t *g_rooms = NULL;

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

room_t *world_get_room(int vnum) {
    for (room_entry_t *e = g_rooms; e; e = e->next) {
        if (e->room->vnum == vnum)
            return e->room;
    }
    return NULL;
}

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

void world_for_each_room(void (*visit)(room_t *r)) {
    room_entry_t *e = g_rooms;
    while (e) {
        room_entry_t *next = e->next; /* visit() could in principle re-register/replace e->room */
        visit(e->room);
        e = next;
    }
}
