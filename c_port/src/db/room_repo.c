/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "room_repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "db.h"

/* Loads a room by vnum, including all ten exits (roomexit rows, one per
 * direction) -- the main entry point for bringing a room into memory. */
int room_repo_find_vnum_by_name(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int vnum = -1;
    if (db_query(db, "select vnum from room where name like '%%%s%%' order by vnum limit 1", name)
        && db_fetch_row(db)) {
        vnum = atoi(db_get(db, "vnum"));
    }

    db_close(db);
    return vnum;
}

room_t *room_repo_load(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    room_t *r = NULL;
    if (db_query(db, "select vnum, name, description, sector, room_flag, "
                     "capacity, height, x, y, z, mine_trapped from room where vnum=%i", vnum)
        && db_fetch_row(db)) {
        r = room_create(vnum, db_get(db, "name"), db_get(db, "description"), atoi(db_get(db, "sector")));
        if (r) {
            r->room_flag = atoi(db_get(db, "room_flag"));
            r->capacity = atoi(db_get(db, "capacity"));
            r->height = atoi(db_get(db, "height"));
            r->x = atoi(db_get(db, "x"));
            r->y = atoi(db_get(db, "y"));
            r->z = atoi(db_get(db, "z"));
            r->mine_trapped = atoi(db_get(db, "mine_trapped")) != 0;
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

/* True if a room row exists for vnum. */
bool room_repo_exists(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool found = db_query(db, "select vnum from room where vnum=%i", vnum) && db_fetch_row(db);
    db_close(db);
    return found;
}

/* Finds the lowest unused room vnum in [bottom, top], for redit's
 * create-new-room flow. Returns -1 if the whole range is already taken. */
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

/* Picks one random vnum >= 100, excluding DEATH/PRIVATE/HAVE-TO-WALK
 * rooms -- see room_repo.h's doc comment. */
int room_repo_random_teleport_vnum(void) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;
    int vnum = -1;
    if (db_query(db,
        "select vnum from room where vnum >= 100 and (room_flag & %i) = 0 "
        "order by rand() limit 1",
        ROOM_FLAG_DEATH | ROOM_FLAG_PRIVATE | ROOM_FLAG_HAVE_TO_WALK)
        && db_fetch_row(db)) {
        vnum = atoi(db_get(db, "vnum"));
    }
    db_close(db);
    return vnum;
}

/* Returns a room's zone number, or -1 if the room is unzoned/not found. */
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

/* Creates or updates a room's own row (name, description, sector, flags,
 * capacity, height) -- how redit persists room edits. Does not touch
 * exits; see room_repo_save_exit() below for those. */
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

/* Creates or updates one exit (direction dir) of room vnum. */
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

/* Removes one direction's exit from a room, e.g. when an editor deletes it. */
bool room_repo_delete_exit(int vnum, int dir) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from roomexit where vnum=%i and direction=%i", vnum, dir);

    db_close(db);
    return ok;
}

/* Sets/clears just the room's `mine_trapped` column -- narrow
 * single-column write, same shape as room_repo_save_exit() above. */
bool room_repo_save_mine_trap(int vnum, bool trapped) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update room set mine_trapped=%i where vnum=%i",
                        trapped ? 1 : 0, vnum);
    db_close(db);
    return ok;
}

/* Case-insensitive per-word prefix match, same convention as
 * obj_name_matches()/thing_name_matches() -- `tok` matches `keywords` if
 * it's a prefix of ANY individual space-separated word in it. Local copy
 * rather than shared, same precedent as every other file in this
 * codebase that keeps its own small copy of this exact helper. */
static bool extra_desc_name_matches(const char *keywords, const char *tok) {
    size_t tok_len = strlen(tok);
    if (tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}

/* Finds a room's extra description whose keyword list matches keyword (via
 * extra_desc_name_matches() above) -- backs "look <keyword>" for a room's
 * examine-able details. */
bool room_repo_extra_desc(int vnum, const char *keyword, char *buf, size_t bufsz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select name, description from roomextra where vnum=%i", vnum)) {
        while (db_fetch_row(db)) {
            if (extra_desc_name_matches(db_get(db, "name"), keyword)) {
                snprintf(buf, bufsz, "%s", db_get(db, "description"));
                found = true;
                break;
            }
        }
    }

    db_close(db);
    return found;
}

/* Lists the keyword names of every extra description on a room, sorted --
 * used by redit to show what extra descs already exist. */
int room_repo_extra_list(int vnum, char out[][ROOM_EXTRA_NAME_LEN], int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select name from roomextra where vnum=%i order by name", vnum)) {
        while (n < max && db_fetch_row(db))
            snprintf(out[n++], ROOM_EXTRA_NAME_LEN, "%s", db_get(db, "name"));
    }

    db_close(db);
    return n;
}

/* Loads a single extra description's body by its exact keyword name (as
 * opposed to room_repo_extra_desc() above, which does per-word prefix
 * matching) -- used by redit when editing a specific already-known entry. */
bool room_repo_extra_get(int vnum, const char *name, char *buf, size_t bufsz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select description from roomextra where vnum=%i and name='%s'", vnum, name)) {
        if (db_fetch_row(db)) {
            snprintf(buf, bufsz, "%s", db_get(db, "description"));
            found = true;
        }
    }

    db_close(db);
    return found;
}

/* Creates or updates a room's extra description under the given keyword. */
bool room_repo_extra_save(int vnum, const char *name, const char *description) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into roomextra (vnum, name, description) values (%i, '%s', '%s') "
        "on duplicate key update description='%s'",
        vnum, name, description, description);

    db_close(db);
    return ok;
}

/* Renames an extra description's keyword without touching its body text. */
bool room_repo_extra_rename(int vnum, const char *old_name, const char *new_name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update roomextra set name='%s' where vnum=%i and name='%s'",
                        new_name, vnum, old_name);

    db_close(db);
    return ok;
}

/* Deletes one named extra description from a room. */
bool room_repo_extra_delete(int vnum, const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from roomextra where vnum=%i and name='%s'", vnum, name);

    db_close(db);
    return ok;
}

/* Deletes every extra description on a room, e.g. before a room's description
 * is entirely rewritten and its old extra descs no longer apply. */
bool room_repo_extra_delete_all(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from roomextra where vnum=%i", vnum);

    db_close(db);
    return ok;
}

int room_repo_delete_range(int low, int high) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    db_query(db, "delete from roomexit where vnum between %i and %i", low, high);
    db_query(db, "delete from roomextra where vnum between %i and %i", low, high);
    db_query(db, "delete from room where vnum between %i and %i", low, high);
    long n = db_row_count(db);

    db_close(db);
    return (int)n;
}
