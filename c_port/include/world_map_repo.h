/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_WORLD_MAP_REPO_H
#define TOBIN_WORLD_MAP_REPO_H
#include <stdbool.h>
#include "room.h"
/* One row of the whole-world room+exit graph, loaded straight from the
 * database -- NOT the in-memory world cache (world.c), which only holds
 * rooms actually visited since boot. Backs both `mapexport` (dump every
 * room+exit to a file) and `maprecalc` (derive x/y/z from the roomexit
 * graph) -- see cmd_mapexport.c/cmd_maprecalc.c. */
typedef struct {
    int vnum;
    char name[128];
    int exits[ROOM_NUM_EXITS]; /* destination vnum, -1 = no exit */
    int x, y, z;                /* only populated/meaningful for maprecalc */
} world_map_room_t;
/* Loads every room and its exits directly from the database: one query
 * for `room`, one for `roomexit`, merged by ascending vnum (both queried
 * ORDER BY vnum) -- NOT a per-room query loop, which would be ~20000
 * round trips against the real live world. Returns a malloc'd array of
 * *out_count entries sorted by vnum (caller frees with free()), or NULL
 * on failure (no DB connection, or an empty `room` table). */
world_map_room_t *world_map_repo_load_all(int *out_count);
/* Writes back just the x/y/z columns for every room in `rooms`, inside
 * one transaction (all-or-nothing) -- used by `maprecalc` after it
 * derives new coordinates. Returns false (and rolls back) on any
 * failure partway through. */
bool world_map_repo_save_coords(const world_map_room_t *rooms, int count);
#endif
