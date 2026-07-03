#include "room.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const DIR_NAMES[ROOM_NUM_EXITS] = {
    "north", "east", "south", "west", "up", "down",
    "northeast", "northwest", "southeast", "southwest",
};

/* Original: constants.cc rev_dirs (all ten entries). */
const int REV_DIR[ROOM_NUM_EXITS] = { 2, 3, 0, 1, 5, 4, 9, 8, 7, 6 };

room_t *room_create(int vnum, const char *name, const char *description, int sector) {
    room_t *r = calloc(1, sizeof(*r));
    if (!r)
        return NULL;

    r->base.kind = THING_ROOM;
    r->base.id = vnum;
    snprintf(r->base.name, sizeof(r->base.name), "%s", name ? name : "");
    r->vnum = vnum;
    snprintf(r->description, sizeof(r->description), "%s", description ? description : "");
    r->sector = sector;

    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        r->exits[i] = -1;

    return r;
}

void room_destroy(room_t *r) {
    free(r);
}
