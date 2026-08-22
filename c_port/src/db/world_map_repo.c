/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "world_map_repo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"

world_map_room_t *world_map_repo_load_all(int *out_count) {
    *out_count = 0;
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;
    if (!db_query(db, "select count(*) as n from room") || !db_fetch_row(db)) {
        db_close(db);
        return NULL;
    }
    int cap = atoi(db_get(db, "n"));
    if (cap <= 0) {
        db_close(db);
        return NULL;
    }
    world_map_room_t *rooms = (world_map_room_t *)calloc((size_t)cap, sizeof(world_map_room_t));
    if (!rooms) {
        db_close(db);
        return NULL;
    }
    if (!db_query(db, "select vnum, name from room order by vnum")) {
        free(rooms);
        db_close(db);
        return NULL;
    }
    int n = 0;
    while (n < cap && db_fetch_row(db)) {
        rooms[n].vnum = atoi(db_get(db, "vnum"));
        snprintf(rooms[n].name, sizeof(rooms[n].name), "%s", db_get(db, "name"));
        for (int i = 0; i < ROOM_NUM_EXITS; i++)
            rooms[n].exits[i] = -1;
        n++;
    }
    /* Merge the roomexit rows onto the vnum-sorted room array with a
     * two-pointer walk (both result sets are ORDER BY vnum already) --
     * `ri` only ever advances forward, since roomexit's own vnum stream
     * is non-decreasing; several consecutive rows can share one vnum
     * (up to ROOM_NUM_EXITS of them, one per direction). */
    if (!db_query(db, "select vnum, direction, destination from roomexit order by vnum")) {
        free(rooms);
        db_close(db);
        return NULL;
    }
    int ri = 0;
    while (db_fetch_row(db)) {
        int vnum = atoi(db_get(db, "vnum"));
        int dir = atoi(db_get(db, "direction"));
        int dest = atoi(db_get(db, "destination"));
        while (ri < n && rooms[ri].vnum < vnum)
            ri++;
        if (ri < n && rooms[ri].vnum == vnum && dir >= 0 && dir < ROOM_NUM_EXITS)
            rooms[ri].exits[dir] = dest;
    }
    db_close(db);
    *out_count = n;
    return rooms;
}

bool world_map_repo_save_coords(const world_map_room_t *rooms, int count) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    if (!db_begin(db)) {
        db_close(db);
        return false;
    }
    bool ok = true;
    for (int i = 0; i < count && ok; i++) {
        ok = db_query(db, "update room set x=%i, y=%i, z=%i where vnum=%i",
                      rooms[i].x, rooms[i].y, rooms[i].z, rooms[i].vnum);
    }
    if (!ok) {
        db_rollback(db);
        db_close(db);
        return false;
    }
    bool committed = db_commit(db);
    db_close(db);
    return committed;
}
