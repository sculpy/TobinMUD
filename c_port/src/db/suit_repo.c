/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "suit_repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

/* Finds the newbie equipment suit defined for a character class, used by
 * player_create() to grant starting gear on first login. Returns -1 if no
 * suit is defined for that class. */
int suit_repo_find_for_class(int player_class) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int found = -1;
    if (db_query(db, "select id from suit where class=%i limit 1", player_class)
        && db_fetch_row(db))
        found = atoi(db_get(db, "id"));

    db_close(db);
    return found;
}

/* Finds the newbie equipment suit defined for a race -- see suit_repo.h. */
int suit_repo_find_for_race(int player_race) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int found = -1;
    if (db_query(db, "select id from suit where race=%i limit 1", player_race)
        && db_fetch_row(db))
        found = atoi(db_get(db, "id"));

    db_close(db);
    return found;
}

/* Loads suit_id's item vnums AND per-item quantities -- see suit_repo.h. */
int suit_repo_load_items_qty(int suit_id, int *vnums, int *qtys, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select obj_vnum, quantity from suit_item where suit_id=%i", suit_id)) {
        while (n < max && db_fetch_row(db)) {
            vnums[n] = atoi(db_get(db, "obj_vnum"));
            qtys[n] = atoi(db_get(db, "quantity"));
            n++;
        }
    }

    db_close(db);
    return n;
}

/* Lists every suit -- see suit_repo.h. */
int suit_repo_list_all(suit_summary_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db, "select id, name, class, description from suit order by id")) {
        while (n < max && db_fetch_row(db)) {
            out[n].id = atoi(db_get(db, "id"));
            snprintf(out[n].name, sizeof(out[n].name), "%s", db_get(db, "name"));
            const char *c = db_get(db, "class");
            out[n].class_restrict = (c && *c) ? atoi(c) : -1;
            snprintf(out[n].description, sizeof(out[n].description), "%s", db_get(db, "description"));
            n++;
        }
    }
    db_close(db);

    for (int i = 0; i < n; i++) {
        db_conn_t *db2 = db_open(DB_TOBIN);
        if (!db2)
            continue;
        if (db_query(db2, "select count(*) as n from suit_item where suit_id=%i", out[i].id)
            && db_fetch_row(db2))
            out[i].item_count = atoi(db_get(db2, "n"));
        db_close(db2);
    }
    return n;
}

/* Loads one suit's scalar fields -- see suit_repo.h. */
bool suit_repo_get(int suit_id, char *name, size_t name_sz,
                    int *out_class, char *description, size_t desc_sz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select name, class, description from suit where id=%i", suit_id)
        && db_fetch_row(db)) {
        if (name)
            snprintf(name, name_sz, "%s", db_get(db, "name"));
        if (out_class) {
            const char *c = db_get(db, "class");
            *out_class = (c && *c) ? atoi(c) : -1;
        }
        if (description)
            snprintf(description, desc_sz, "%s", db_get(db, "description"));
        found = true;
    }

    db_close(db);
    return found;
}

/* Deletes an entire suit (and, via FK cascade, its suit_item rows) --
 * see suit_repo.h. */
bool suit_repo_delete(int suit_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from suit where id=%i", suit_id);

    db_close(db);
    return ok;
}

/* Creates a brand-new, empty suit -- see suit_repo.h. */
int suit_repo_create(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int new_id = -1;
    if (db_query(db, "insert into suit (name, class, description) values "
                      "('%s', NULL, 'New suit')", name))
        new_id = (int)db_last_insert_id(db);

    db_close(db);
    return new_id;
}

/* Updates a suit's class restriction -- see suit_repo.h. */
bool suit_repo_set_class(int suit_id, int class_restrict) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (class_restrict < 0)
        ok = db_query(db, "update suit set class=NULL where id=%i", suit_id);
    else
        ok = db_query(db, "update suit set class=%i where id=%i", class_restrict, suit_id);

    db_close(db);
    return ok;
}

/* Updates a suit's description -- see suit_repo.h. */
bool suit_repo_set_description(int suit_id, const char *description) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update suit set description='%s' where id=%i", description, suit_id);

    db_close(db);
    return ok;
}

/* Adds (or upserts the quantity of) one suit_item row -- see suit_repo.h. */
bool suit_repo_add_item(int suit_id, int obj_vnum, int quantity) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into suit_item (suit_id, obj_vnum, quantity) values (%i, %i, %i) "
        "on duplicate key update quantity=%i",
        suit_id, obj_vnum, quantity, quantity);

    db_close(db);
    return ok;
}

/* Updates an existing suit_item row's quantity -- see suit_repo.h. */
bool suit_repo_set_item_qty(int suit_id, int obj_vnum, int quantity) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update suit_item set quantity=%i where suit_id=%i and obj_vnum=%i",
                        quantity, suit_id, obj_vnum);

    db_close(db);
    return ok;
}

/* Removes one suit_item row -- see suit_repo.h. */
bool suit_repo_delete_item(int suit_id, int obj_vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from suit_item where suit_id=%i and obj_vnum=%i",
                        suit_id, obj_vnum);

    db_close(db);
    return ok;
}
