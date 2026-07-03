#ifndef TOBIN_ROOM_H
#define TOBIN_ROOM_H

#include "thing.h"

/* C replacement for the TRoom slice of misc/thing.h + the `room` DB table
 * (db/sneezy/room.sql). Phase 1 keeps only the fields needed for `look`. */

#define ROOM_DESCRIPTION_MAX 4096
/* The original dirTypeT's full set, IN ITS ORDER: north(0), east(1),
 * south(2), west(3), up(4), down(5), northeast(6), northwest(7),
 * southeast(8), southwest(9) -- confirmed against constants.cc's rev_dirs
 * table. All 10 carried as of Session 21 (the seed DB's diagonal exit
 * rows load again instead of being dropped). */
#define ROOM_NUM_EXITS 10

/* "north", "east", ... indexed by direction; and each direction's reverse
 * (north->south etc), a straight port of the original's rev_dirs. */
extern const char *const DIR_NAMES[ROOM_NUM_EXITS];
extern const int REV_DIR[ROOM_NUM_EXITS];

typedef struct room {
    thing_t base;               /* first member -- see thing.h */
    int vnum;
    char description[ROOM_DESCRIPTION_MAX];
    int sector;
    int room_flag;              /* original's room_flag bitmask -- carried +
                                 * shown to immortals; no behavior yet */
    int exits[ROOM_NUM_EXITS];  /* destination vnum per direction, -1 = no exit */
} room_t;

room_t *room_create(int vnum, const char *name, const char *description, int sector);
void room_destroy(room_t *r);

#endif
