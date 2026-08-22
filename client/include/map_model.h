/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
#ifndef TOBINCLIENT_MAP_MODEL_H
#define TOBINCLIENT_MAP_MODEL_H
#include <stdbool.h>
#include <stddef.h>
/* Portable (no OS calls) graph-walked map model, fed by the server's
 * GMCP Room.Info exits object (c_port/src/net/gmcp.c) as a player
 * moves -- NOT absolute-coordinate, since Tobin's room_t has no
 * in-memory x/y/z (see TODO.md's mapping-support scoping notes). Just
 * "room X has an exit to room Y in direction D", same shape a Mudlet-
 * style mapper builds from. */
#define MAP_NUM_EXITS 10
/* Same fixed 10-direction order as the server's own DIR_NAMES
 * (c_port/src/core/room.c) -- duplicated by hand since the client has
 * no build dependency on the server's sources; keep the two in sync if
 * Tobin ever grows an 11th direction. */
extern const char *const MAP_DIR_NAMES[MAP_NUM_EXITS];
/* Returns the direction index for `dir_name` (case-sensitive, matches
 * what the server sends), or -1 if unrecognized. */
int map_dir_index(const char *dir_name);
typedef struct {
    int vnum;
    char name[128];
    int exits[MAP_NUM_EXITS]; /* destination vnum per direction, -1 = no/unknown exit */
    int x, y, z;    /* maprecalc-derived layout position -- meaningless unless has_pos */
    bool has_pos;   /* false for a room learned before this field existed (an old
                       map.dat entry not yet re-visited/re-exported) -- the real-GDI
                       map view (TODO.md) skips drawing a node with no known position
                       rather than piling every unknown room up at (0,0,0). */
} map_room_t;
/* Tobin's whole `room` table is ~20k rows (checked live, 2026-08-21);
 * rounded up so a future level-59+ full-world export (TODO.md, still
 * unbuilt) can load into the same table a normal player's graph-walk
 * fills incrementally. A plain fixed array, not a hash table -- lookups
 * only happen once per room entry (a human's reaction-time scale), so
 * a linear scan over a few thousand entries costs nothing that matters. */
#define MAP_ROOM_MAX 24000
typedef struct {
    map_room_t rooms[MAP_ROOM_MAX];
    int count;
} map_model_t;
void map_model_init(map_model_t *m);
/* Adds a new room, or overwrites an existing one (matched by vnum)
 * with fresh name/exits -- a later sighting always wins (a door can be
 * dug or destroyed after first discovery). Returns false only when the
 * room is new and the table is already full (MAP_ROOM_MAX reached). */
bool map_model_upsert(map_model_t *m, int vnum, const char *name, const int exits[MAP_NUM_EXITS],
                       int x, int y, int z, bool has_pos);
map_room_t *map_model_find(map_model_t *m, int vnum);
/* Loads the whole table from a simple tab-delimited text file
 * (VNUM<TAB>NAME<TAB>e0,e1,...,e9 per line, -1 for no exit). Additive/
 * best-effort: a missing or unreadable file just leaves `m` at
 * whatever it already had (typically a freshly map_model_init()'d,
 * empty table) -- there is no first-run seeding here, unlike
 * triggers.txt/aliases.txt, since an empty map is a completely normal
 * starting state, not a missed feature. */
void map_model_load(map_model_t *m, const char *path);
/* Overwrites `path` with the whole current table. Returns false if the
 * file couldn't be opened for writing. */
bool map_model_save(const map_model_t *m, const char *path);
#endif
