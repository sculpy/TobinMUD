/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "room_repo.h"

#include <stdlib.h>

#include "db.h"

room_t *room_repo_load(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    room_t *r = NULL;
    if (db_query(db, "select vnum, name, description, sector, room_flag, "
                     "capacity, height from room where vnum=%i", vnum)
        && db_fetch_row(db)) {
        r = room_create(vnum, db_get(db, "name"), db_get(db, "description"), atoi(db_get(db, "sector")));
        if (r) {
            r->room_flag = atoi(db_get(db, "room_flag"));
            r->capacity = atoi(db_get(db, "capacity"));
            r->height = atoi(db_get(db, "height"));
        }
    }
    db_close(db);
    if (!r)
        return NULL;

    /* Exits are a separate table; direction indices 0-9 are the original
     * dirTypeT order (see room.h) -- all ten load, including the
     * diagonals restored in Session 21. `type` is the doorTypeT and
     * `condition_flag` the exit condition bitmask (builder-editable). */
    db_conn_t *db2 = db_open(DB_TOBIN);
    if (db2 && db_query(db2, "select direction, destination, type, condition_flag, "
                             "key_num from roomexit where vnum=%i", vnum)) {
        while (db_fetch_row(db2)) {
            int dir = atoi(db_get(db2, "direction"));
            if (dir >= 0 && dir < ROOM_NUM_EXITS) {
                r->exits[dir] = atoi(db_get(db2, "destination"));
                r->exit_door[dir] = atoi(db_get(db2, "type"));
                r->exit_cond[dir] = atoi(db_get(db2, "condition_flag"));
                r->exit_key[dir] = atoi(db_get(db2, "key_num"));
            }
        }
    }
    db_close(db2);

    return r;
}

bool room_repo_exists(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool found = db_query(db, "select vnum from room where vnum=%i", vnum) && db_fetch_row(db);
    db_close(db);
    return found;
}

int room_repo_next_free_vnum(int bottom, int top) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int result = -1;
    if (db_query(db, "select vnum from room where vnum between %i and %i order by vnum", bottom, top)) {
        int expected = bottom;
        while (db_fetch_row(db)) {
            int v = atoi(db_get(db, "vnum"));
            if (v > expected) {
                result = expected;
                break;
            }
            expected = v + 1;
        }
        if (result < 0 && expected <= top)
            result = expected;
    }

    db_close(db);
    return result;
}

int room_repo_get_zone(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;
    int zone = -1;
    if (db_query(db, "select zone from room where vnum=%i", vnum) && db_fetch_row(db)) {
        const char *raw = db_get(db, "zone");
        if (raw[0]) /* db_get() returns "" for a NULL column -- distinguish
                       an unzoned room from a legitimate zone_nr of 0 */
            zone = atoi(raw);
    }
    db_close(db);
    return zone;
}

bool room_repo_save(const room_t *r) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into room (vnum, x, y, z, name, description, zone, room_flag, "
        "sector, teletime, teletarg, telelook, river_speed, river_dir, "
        "capacity, height, spec) "
        "values (%i, 0, 0, 0, '%s', '%s', NULL, %i, %i, 0, 0, 0, 0, 0, %i, %i, 0) "
        "on duplicate key update name='%s', description='%s', sector=%i, "
        "room_flag=%i, capacity=%i, height=%i",
        r->vnum, r->base.name, r->description, r->room_flag, r->sector,
        r->capacity, r->height,
        r->base.name, r->description, r->sector, r->room_flag,
        r->capacity, r->height);

    db_close(db);
    return ok;
}

bool room_repo_save_exit(int vnum, int dir, int dest, int door_type, int condition) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into roomexit (vnum, direction, name, description, type, "
        "condition_flag, lock_difficulty, weight, key_num, destination) "
        "values (%i, %i, '', '', %i, %i, 0, 0, 0, %i) "
        "on duplicate key update destination=%i, type=%i, condition_flag=%i",
        vnum, dir, door_type, condition, dest, dest, door_type, condition);

    db_close(db);
    return ok;
}

bool room_repo_delete_exit(int vnum, int dir) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from roomexit where vnum=%i and direction=%i", vnum, dir);

    db_close(db);
    return ok;
}
