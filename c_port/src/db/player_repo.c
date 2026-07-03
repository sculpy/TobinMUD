#include "player_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"
#include "log.h"

/* Default landing room for freshly created characters -- vnum 1 ("Imperia")
 * exists in the seed data (db/sneezy/room.sql) and has a real description,
 * unlike vnum 0 ("The Void"). Revisit once zone-aware character creation
 * (starting city selection etc) is ported. */
#define DEFAULT_LOAD_ROOM 1

being_t *player_load(const char *name, long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    being_t *b = NULL;
    if (db_query(db, "select id, name, account_id, load_room from player "
                      "where name='%s' and account_id=%i",
                 name, (int)account_id)
        && db_fetch_row(db)) {
        long player_id = atol(db_get(db, "id"));
        b = being_create_pc(db_get(db, "name"), account_id, player_id);
    }

    db_close(db);

    if (b) {
        player_attrs_load(b->player_id, &b->attrs); /* falls back to ATTR_BASE defaults if missing */
        player_progress_load(b->player_id, &b->progress); /* falls back to being_create_pc()'s defaults if missing */
    }

    return b;
}

being_t *player_create(const char *name, long account_id, const attrs_t *attrs) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    db_query(db, "select 1 from player where name='%s'", name);
    if (db_fetch_row(db)) {
        log_error("player_create: name '%s' already taken", name);
        db_close(db);
        return NULL;
    }

    db_query(db, "select count(*) as n from player where account_id=%i", (int)account_id);
    if (db_fetch_row(db) && atoi(db_get(db, "n")) >= MAX_CHARS_PER_ACCOUNT) {
        log_error("player_create: account %ld already has %d characters", account_id, MAX_CHARS_PER_ACCOUNT);
        db_close(db);
        return NULL;
    }

    bool ok = db_query(db,
        "insert into player (name, talens, account_id, load_room, nutrition) "
        "values ('%s', 0, %i, %i, 100)",
        name, (int)account_id, DEFAULT_LOAD_ROOM);

    being_t *b = NULL;
    if (ok) {
        long player_id = db_last_insert_id(db);
        b = being_create_pc(name, account_id, player_id);
        if (b) {
            if (attrs)
                b->attrs = *attrs;
            player_attrs_save(player_id, &b->attrs);
            player_progress_save(player_id, &b->progress);
        }
    }

    db_close(db);
    return b;
}

bool player_delete(const char *name, long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    db_query(db, "select 1 from player where name='%s' and account_id=%i", name, (int)account_id);
    if (!db_fetch_row(db)) {
        db_close(db);
        return false;
    }

    /* player_attrs is removed automatically via ON DELETE CASCADE. */
    bool ok = db_query(db, "delete from player where name='%s' and account_id=%i", name, (int)account_id);

    db_close(db);
    return ok;
}

bool player_list_by_account(long account_id, char names[][PLAYER_NAME_LEN], int levels[], int max, int *count) {
    *count = 0;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
                       "select p.name, coalesce(pp.level, 1) as level"
                       " from player p left join player_progress pp on pp.player_id = p.id"
                       " where p.account_id=%i order by p.name limit %i",
                       (int)account_id, max);
    if (ok) {
        while (*count < max && db_fetch_row(db)) {
            snprintf(names[*count], PLAYER_NAME_LEN, "%s", db_get(db, "name"));
            if (levels)
                levels[*count] = atoi(db_get(db, "level"));
            (*count)++;
        }
    }

    db_close(db);
    return ok;
}

int player_load_room(const char *name, long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int room_vnum = -1;
    if (db_query(db, "select load_room from player where name='%s' and account_id=%i",
                 name, (int)account_id)
        && db_fetch_row(db)) {
        room_vnum = atoi(db_get(db, "load_room"));
    }

    db_close(db);
    return room_vnum;
}

bool player_attrs_load(long player_id, attrs_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select strength, dexterity, constitution, intelligence, wisdom, charisma "
                      "from player_attrs where player_id=%i",
                 (int)player_id)
        && db_fetch_row(db)) {
        out->strength = atoi(db_get(db, "strength"));
        out->dexterity = atoi(db_get(db, "dexterity"));
        out->constitution = atoi(db_get(db, "constitution"));
        out->intelligence = atoi(db_get(db, "intelligence"));
        out->wisdom = atoi(db_get(db, "wisdom"));
        out->charisma = atoi(db_get(db, "charisma"));
        found = true;
    }

    db_close(db);
    return found;
}

bool player_attrs_save(long player_id, const attrs_t *attrs) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into player_attrs (player_id, strength, dexterity, constitution, "
        "intelligence, wisdom, charisma) values (%i, %i, %i, %i, %i, %i, %i) "
        "on duplicate key update strength=%i, dexterity=%i, constitution=%i, "
        "intelligence=%i, wisdom=%i, charisma=%i",
        (int)player_id, attrs->strength, attrs->dexterity, attrs->constitution,
        attrs->intelligence, attrs->wisdom, attrs->charisma,
        attrs->strength, attrs->dexterity, attrs->constitution,
        attrs->intelligence, attrs->wisdom, attrs->charisma);

    db_close(db);
    return ok;
}

bool player_progress_load(long player_id, progress_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select level, experience, hp, max_hp from player_progress where player_id=%i",
                 (int)player_id)
        && db_fetch_row(db)) {
        out->level = atoi(db_get(db, "level"));
        out->experience = atol(db_get(db, "experience"));
        out->hp = atoi(db_get(db, "hp"));
        out->max_hp = atoi(db_get(db, "max_hp"));
        found = true;
    }

    db_close(db);
    return found;
}

bool player_progress_save(long player_id, const progress_t *progress) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into player_progress (player_id, level, experience, hp, max_hp) "
        "values (%i, %i, %i, %i, %i) "
        "on duplicate key update level=%i, experience=%i, hp=%i, max_hp=%i",
        (int)player_id, progress->level, (int)progress->experience, progress->hp, progress->max_hp,
        progress->level, (int)progress->experience, progress->hp, progress->max_hp);

    db_close(db);
    return ok;
}
