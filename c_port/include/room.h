/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_ROOM_H
#define TOBIN_ROOM_H

#include <stddef.h>

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

/* Sector-type names, a straight port of the original's sectorTypeT
 * (misc/enum.h, 61 entries, SECT_SUBARCTIC=0 .. SECT_DEAD_WOODS=60).
 * Out-of-range values render as "unknown". */
#define MAX_SECTOR_TYPES 61
const char *sector_name(int sector);

/* Lowercase base color-tag letter (see colorstring.h) for a sector, grouped
 * by what the sector name implies (lava/fire -> red, city/road/building/
 * mountain/cave/solid rock -> white, ocean/river/beach -> blue, arctic/
 * atmosphere -> cyan, desert -> yellow, swamp/forest/jungle/grassland/
 * plains/hills -> green, astral -> purple, anything else -> white).
 * Deliberately never returns 'k'/'K' (gray/black, user spec) -- unreadable
 * on a black terminal background. `look` uses the uppercase (bright) tag
 * for the room name and this lowercase (dim) one for the description --
 * see cmd_look.c. */
char sector_color(int sector);

/* Renders the set ROOM_* flag bits (original misc/room.h, 22 bits) into
 * buf as space-separated names ("always-lit indoors ..."), or "none".
 * Returns buf for convenience. */
const char *room_flag_names(int flags, char *buf, size_t size);

/* Per-bit access to the ROOM_* flag table, for the flag-toggle submenu in
 * the room builder. `bit` in [0, room_flag_count()); out of range -> "?". */
int room_flag_count(void);
const char *room_flag_name(int bit);

/* Bit 3 of ROOM_FLAG_NAMES (room.c) -- matches the upstream ROOM_INDOORS
 * bit position verbatim. Named here since room_ground_type() needs to test
 * it directly, not just display it. */
#define ROOM_FLAG_INDOORS (1 << 3)

/* Bit 2 of ROOM_FLAG_NAMES (room.c) -- matches the upstream ROOM_NO_MOB bit
 * position verbatim. Named here since mob_ai.c's wander logic needs to
 * test it directly. */
#define ROOM_FLAG_NO_MOB (1 << 2)

/* Sector-based ground-surface word (Sneezy's TRoom::describeGroundType(),
 * misc/create_rooms.cc) -- "street", "road", "water", "mud", "sand",
 * "floor" (indoors), or "ground" (default). Backs the `$$g`/`$g` token in
 * object descriptions (see obj_apply_ground_token(), obj.h). Simplified
 * from the original: no weather-prefix component ("snow-covered ground",
 * "rain-slick street", ...) -- Tobin has no weather system yet. */
const char *room_ground_type(const struct room *r);

/* Exit door types (original doorTypeT, misc/room.h) -- None/Door/Trapdoor/
 * ... Chosen when editing a room exit. */
#define MAX_DOOR_TYPES 11
int door_type_count(void);
const char *door_type_name(int t);

/* Exit condition bits (original exit_bits, MAX_DOOR_CONDITIONS) --
 * Closed/Locked/Secret/... A per-exit bitmask, edited by toggle. */
#define MAX_EXIT_CONDITIONS 11
int exit_cond_count(void);
const char *exit_cond_name(int bit);
/* Renders the set condition bits into buf, space-separated, or "none". */
const char *exit_cond_names(int flags, char *buf, size_t size);

/* Named bits for the three conditions door mechanics actually act on
 * (open/close/movement-blocking/hiding) -- see cmd_move.c, cmd_open.c,
 * cmd_look.c, cmd_exits.c. The rest (Trapped, Caved-In, ...) are still
 * builder-editable via redit's toggle submenu but have no behavior yet. */
#define EXIT_COND_CLOSED (1 << 0)
#define EXIT_COND_LOCKED (1 << 1)
#define EXIT_COND_SECRET (1 << 2)
/* Trap mechanics (user 2026-07-11: "...then weapon depth, trap
 * mechanics" -- sequenced after weapon depth). A Thief's "set trap
 * (door)"/"disarm trap" skills (skill.c) toggle this bit on a closed
 * door via `settrap`/`disarmtrap` (cmd_trap.c); walking through a
 * trapped door (cmd_move.c) springs it -- one-shot damage, then the
 * bit clears -- unless the mover knows "detect trap" and spots it
 * first. Was already a named-but-inert bit (EXIT_COND_NAMES, room.c)
 * before this. */
#define EXIT_COND_TRAPPED (1 << 5)

typedef struct room {
    thing_t base;               /* first member -- see thing.h */
    int vnum;
    char description[ROOM_DESCRIPTION_MAX];
    int sector;
    int room_flag;              /* original's room_flag bitmask -- carried +
                                 * shown to immortals; no behavior yet */
    int capacity;               /* room `capacity` column (moblim); builder-set */
    int height;                 /* room `height` column; builder-set */
    int exits[ROOM_NUM_EXITS];  /* destination vnum per direction, -1 = no exit */
    int exit_door[ROOM_NUM_EXITS]; /* doorTypeT per exit (0 = DOOR_NONE) */
    int exit_cond[ROOM_NUM_EXITS]; /* exit condition bitmask per exit */
} room_t;

room_t *room_create(int vnum, const char *name, const char *description, int sector);
void room_destroy(room_t *r);

#endif
