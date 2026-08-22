/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "zone_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

/* Loads every zone's summary row (name, enabled, vnum range, lifespan),
 * zone_nr-sorted, up to max entries -- used at startup and by zone-listing
 * commands. */
int zone_repo_load_all(zone_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select zone_nr, zone_name, zone_enabled, bottom, top, "
                      "lifespan from zone order by zone_nr")) {
        while (n < max && db_fetch_row(db)) {
            out[n].zone_nr = atoi(db_get(db, "zone_nr"));
            snprintf(out[n].name, sizeof(out[n].name), "%s", db_get(db, "zone_name"));
            out[n].enabled = atoi(db_get(db, "zone_enabled")) != 0;
            out[n].bottom = atoi(db_get(db, "bottom"));
            out[n].top = atoi(db_get(db, "top"));
            out[n].lifespan = atoi(db_get(db, "lifespan"));
            n++;
        }
    }
    db_close(db);
    return n;
}

/* Loads a single zone's summary row by zone_nr. */
bool zone_repo_load_one(int zone_nr, zone_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select zone_nr, zone_name, zone_enabled, bottom, top, "
                      "lifespan from zone where zone_nr=%i",
                 zone_nr)
        && db_fetch_row(db)) {
        out->zone_nr = atoi(db_get(db, "zone_nr"));
        snprintf(out->name, sizeof(out->name), "%s", db_get(db, "zone_name"));
        out->enabled = atoi(db_get(db, "zone_enabled")) != 0;
        out->bottom = atoi(db_get(db, "bottom"));
        out->top = atoi(db_get(db, "top"));
        out->lifespan = atoi(db_get(db, "lifespan"));
        found = true;
    }
    db_close(db);
    return found;
}

/* Updates an existing zone's editable fields (name, enabled, vnum range,
 * lifespan) -- zones are created via the seed data/zedit's own creation
 * flow, so this is a plain UPDATE, not an upsert. */
bool zone_repo_save(const zone_t *z) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db,
        "update zone set zone_name='%s', zone_enabled=%i, bottom=%i, top=%i, "
        "lifespan=%i where zone_nr=%i",
        z->name, z->enabled ? 1 : 0, z->bottom, z->top, z->lifespan, z->zone_nr);
    db_close(db);
    return ok;
}

/* Lists the character names of every player assigned as an owner/builder
 * of a zone, name-sorted -- backs zone-ownership listing displays. */
int zone_repo_load_owner_names(int zone_nr, char names[][64], int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select p.name from zone_owner zo join player p on p.id=zo.player_id "
                      "where zo.zone_nr=%i order by p.name",
                 zone_nr)) {
        while (n < max && db_fetch_row(db)) {
            snprintf(names[n], 64, "%s", db_get(db, "name"));
            n++;
        }
    }
    db_close(db);
    return n;
}

/* True if player_id is currently assigned as an owner/builder of zone_nr. */
bool zone_repo_is_assigned(int zone_nr, long player_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool found = db_query(db, "select 1 from zone_owner where zone_nr=%i and player_id=%i",
                           zone_nr, (int)player_id)
                 && db_fetch_row(db);
    db_close(db);
    return found;
}

/* Assigns a player as an owner/builder of a zone. Re-assigning an already-
 * assigned player is a harmless no-op via ON DUPLICATE KEY UPDATE. */
bool zone_repo_assign(int zone_nr, long player_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db,
        "insert into zone_owner (zone_nr, player_id) values (%i, %i) "
        "on duplicate key update zone_nr=zone_nr",
        zone_nr, (int)player_id);
    db_close(db);
    return ok;
}

/* Removes a player's owner/builder assignment from a zone. */
bool zone_repo_unassign(int zone_nr, long player_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "delete from zone_owner where zone_nr=%i and player_id=%i",
                        zone_nr, (int)player_id);
    db_close(db);
    return ok;
}

/* Updates a zone's vnum range (bottom/top), independent of its other
 * fields -- used when resizing a zone rather than editing its full record. */
bool zone_repo_set_range(int zone_nr, int bottom, int top) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update zone set bottom=%i, top=%i where zone_nr=%i",
                        bottom, top, zone_nr);
    db_close(db);
    return ok;
}

bool zone_repo_insert_reset_cmd(int zone_nr, int cmd_no, char command, int if_flag,
                                 int arg1, int arg2, int arg3, int arg4,
                                 const char *comment) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    char cmd_str[2] = { command, '\0' };
    bool ok = db_query(db,
        "insert into zone_reset (zone_nr, cmd_no, command, if_flag, arg1, arg2, arg3, arg4, comment) "
        "values (%i, %i, '%s', %i, %i, %i, %i, %i, '%s')",
        zone_nr, cmd_no, cmd_str, if_flag, arg1, arg2, arg3, arg4, comment ? comment : "");
    db_close(db);
    return ok;
}

/* Loads a zone's reset command list (the scripted mob/obj spawn and reset
 * instructions run on zone reset), cmd_no-ordered, up to max entries. */
int zone_repo_load_resets(int zone_nr, zone_reset_cmd_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select cmd_no, command, if_flag, arg1, arg2, arg3, arg4 "
                      "from zone_reset where zone_nr=%i order by cmd_no",
                 zone_nr)) {
        while (n < max && db_fetch_row(db)) {
            out[n].cmd_no = atoi(db_get(db, "cmd_no"));
            const char *cmd = db_get(db, "command");
            out[n].command = cmd[0];
            out[n].if_flag = atoi(db_get(db, "if_flag"));
            out[n].arg1 = atoi(db_get(db, "arg1"));
            out[n].arg2 = atoi(db_get(db, "arg2"));
            out[n].arg3 = atoi(db_get(db, "arg3"));
            out[n].arg4 = atoi(db_get(db, "arg4"));
            n++;
        }
    }
    db_close(db);
    return n;
}

int zone_repo_delete_resets_referencing_range(int low, int high) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    db_query(db, "delete from zone_reset where (arg1 between %i and %i) or (arg3 between %i and %i)",
             low, high, low, high);
    long n = db_row_count(db);

    db_close(db);
    return (int)n;
}
