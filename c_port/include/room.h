#ifndef TOBIN_ROOM_H
#define TOBIN_ROOM_H

#include "thing.h"

/* C replacement for the TRoom slice of misc/thing.h + the `room` DB table
 * (db/sneezy/room.sql). Phase 1 keeps only the fields needed for `look`. */

#define ROOM_DESCRIPTION_MAX 4096
#define ROOM_NUM_EXITS 6 /* north/south/east/west/up/down, matches original dirTypeT */

typedef struct room {
    thing_t base;               /* first member -- see thing.h */
    int vnum;
    char description[ROOM_DESCRIPTION_MAX];
    int sector;
    int exits[ROOM_NUM_EXITS];  /* destination vnum per direction, -1 = no exit */
} room_t;

room_t *room_create(int vnum, const char *name, const char *description, int sector);
void room_destroy(room_t *r);

#endif
