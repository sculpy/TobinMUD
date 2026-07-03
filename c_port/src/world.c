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
