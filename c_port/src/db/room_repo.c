#include "room_repo.h"

#include <stdlib.h>

#include "db.h"

room_t *room_repo_load(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    room_t *r = NULL;
    if (db_query(db, "select vnum, name, description, sector from room where vnum=%i", vnum)
        && db_fetch_row(db)) {
        r = room_create(vnum, db_get(db, "name"), db_get(db, "description"), atoi(db_get(db, "sector")));
    }
    db_close(db);
    if (!r)
        return NULL;

    /* Exits are a separate table; direction indices are read as-is from the
     * DB and stored positionally -- Phase 1 has no `move` command yet, so
     * the exact dirTypeT mapping isn't load-bearing until that lands. */
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
