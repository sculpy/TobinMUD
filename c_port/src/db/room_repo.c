#include "room_repo.h"

#include <stdlib.h>

#include "db.h"

room_t *room_repo_load(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    room_t *r = NULL;
    if (db_query(db, "select vnum, name, description, sector, room_flag from room where vnum=%i", vnum)
        && db_fetch_row(db)) {
        r = room_create(vnum, db_get(db, "name"), db_get(db, "description"), atoi(db_get(db, "sector")));
        if (r)
            r->room_flag = atoi(db_get(db, "room_flag"));
    }
    db_close(db);
    if (!r)
        return NULL;

    /* Exits are a separate table; direction indices 0-9 are the original
     * dirTypeT order (see room.h) -- all ten load, including the
     * diagonals restored in Session 21. */
    db_conn_t *db2 = db_open(DB_TOBIN);
    if (db2 && db_query(db2, "select direction, destination from roomexit where vnum=%i", vnum)) {
        while (db_fetch_row(db2)) {
            int dir = atoi(db_get(db2, "direction"));
            int dest = atoi(db_get(db2, "destination"));
            if (dir >= 0 && dir < ROOM_NUM_EXITS)
                r->exits[dir] = dest;
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

bool room_repo_save(const room_t *r) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into room (vnum, x, y, z, name, description, zone, room_flag, "
        "sector, teletime, teletarg, telelook, river_speed, river_dir, "
        "capacity, height, spec) "
        "values (%i, 0, 0, 0, '%s', '%s', NULL, %i, %i, 0, 0, 0, 0, 0, 0, 0, 0) "
        "on duplicate key update name='%s', description='%s', sector=%i, room_flag=%i",
        r->vnum, r->base.name, r->description, r->room_flag, r->sector,
        r->base.name, r->description, r->sector, r->room_flag);

    db_close(db);
    return ok;
}

bool room_repo_save_exit(int vnum, int dir, int dest) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into roomexit (vnum, direction, name, description, type, "
        "condition_flag, lock_difficulty, weight, key_num, destination) "
        "values (%i, %i, '', '', 0, 0, 0, 0, 0, %i) "
        "on duplicate key update destination=%i",
        vnum, dir, dest, dest);

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
