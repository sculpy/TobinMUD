/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "player_repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#include "db.h"
#include "log.h"
#include "obj_repo.h"

/* Flat offline-regen rate for a character returning from `rent`
 * (cmd_rent.c) -- 1 HP per this many real seconds elapsed since renting,
 * capped at max_hp. Deliberately simple (not the same curve as the
 * online regen_tick_run()/regen_amount() in regen.c, which factor in
 * CON and position) since a rented-out character has no position to
 * read and this only needs to feel like real "time passed, you
 * recovered" progress, not a precisely balanced number. */
#define RENT_REGEN_SECONDS_PER_HP 5

/* Default landing room for freshly created characters -- vnum 100 ("Center Square")
 * exists in the seed data (db/sneezy/room.sql) and has a real description,
 * unlike vnum 0 ("The Void"). Revisit once zone-aware character creation
 * (starting city selection etc) is ported. */
#define DEFAULT_LOAD_ROOM DEFAULT_LOAD_ROOM_MORTAL

/* Standing safeguard (user request, 2026-07-07): the real character named
 * Jesus is always level 60, full stop. Added after the player_attrs.sql/
 * player_progress.sql DROP-TABLE bug (see STATUS.md) silently reset nearly
 * every player's level -- rather than rely on a one-off manual SQL fix,
 * self-heals on every load. A no-op once already correct. */
static void ensure_jesus_level(being_t *b) {
    if (b && strcasecmp(b->base.name, "Jesus") == 0 && b->progress.level != 60) {
        b->progress.level = 60;
        player_progress_save(b->player_id, &b->progress);
    }
}

being_t *player_load(const char *name, long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    being_t *b = NULL;
    if (db_query(db, "select id, name, account_id, load_room, handed, prompt_flags, "
                      "title, gender, appearance, pflags, poofin, poofout, bamfin, bamfout, "
                      "class, race from player "
                      "where name='%s' and account_id=%i",
                 name, (int)account_id)
        && db_fetch_row(db)) {
        long player_id = atol(db_get(db, "id"));
        b = being_create_pc(db_get(db, "name"), account_id, player_id);
        if (b) {
            b->handed_right = atoi(db_get(db, "handed"));
            b->prompt_flags = atoi(db_get(db, "prompt_flags"));
            snprintf(b->title, sizeof(b->title), "%s", db_get(db, "title"));
            b->gender = (gender_t)atoi(db_get(db, "gender"));
            snprintf(b->appearance, sizeof(b->appearance), "%s", db_get(db, "appearance"));
            b->pflags = atoi(db_get(db, "pflags"));
            snprintf(b->poofin, sizeof(b->poofin), "%s", db_get(db, "poofin"));
            snprintf(b->poofout, sizeof(b->poofout), "%s", db_get(db, "poofout"));
            snprintf(b->bamfin, sizeof(b->bamfin), "%s", db_get(db, "bamfin"));
            snprintf(b->bamfout, sizeof(b->bamfout), "%s", db_get(db, "bamfout"));
            b->char_class = (player_class_t)atoi(db_get(db, "class"));
            b->race = (player_race_t)atoi(db_get(db, "race"));
        }
    }

    db_close(db);

    if (b) {
        player_attrs_load(b->player_id, &b->attrs); /* falls back to ATTR_BASE defaults if missing */
        player_progress_load(b->player_id, &b->progress); /* falls back to being_create_pc()'s defaults if missing */
        /* Returning from `rent` (cmd_rent.c): heal for the real time spent
         * rented out, then clear the marker so this only fires once. */
        if (b->progress.rented_at > 0) {
            long elapsed = (long)time(NULL) - b->progress.rented_at;
            if (elapsed > 0) {
                int healed = (int)(elapsed / RENT_REGEN_SECONDS_PER_HP);
                b->progress.hp += healed;
                if (b->progress.hp > b->progress.max_hp)
                    b->progress.hp = b->progress.max_hp;
            }
            b->progress.rented_at = 0;
            player_progress_save(b->player_id, &b->progress);
        }
        /* being_create_pc() already sized limbs off level-1 defaults before
         * the real (possibly much higher) level/max_hp landed just above --
         * without this, every reconnect for a leveled character leaves limbs
         * stuck at level-1-sized fractions, making them trivially
         * decapitatable regardless of real max_hp (found 2026-07-12 while
         * testing the affects system). */
        being_limbs_full_heal(b);
        player_inventory_load(b->player_id, b); /* recreates carried/worn/held instances, see obj_repo.h */
        ensure_jesus_level(b);
    }

    return b;
}

being_t *player_create(const char *name, long account_id, const attrs_t *attrs,
                       int handed_right, gender_t gender, const char *appearance,
                       player_class_t char_class, player_race_t race, int alignment) {
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
        "insert into player (name, talens, account_id, load_room, nutrition, handed, gender, appearance, class, race) "
        "values ('%s', 0, %i, %i, 100, %i, %i, '%s', %i, %i)",
        name, (int)account_id, DEFAULT_LOAD_ROOM, handed_right ? 1 : 0,
        (int)gender, appearance ? appearance : "", (int)char_class, (int)race);

    being_t *b = NULL;
    if (ok) {
        long player_id = db_last_insert_id(db);
        b = being_create_pc(name, account_id, player_id);
        if (b) {
            b->handed_right = handed_right ? 1 : 0;
            b->gender = gender;
            b->char_class = char_class;
            b->race = race;
            b->pflags = PLR_NEWBIE; /* new players start on the newbie channel */
            snprintf(b->appearance, sizeof(b->appearance), "%s", appearance ? appearance : "");
            if (attrs)
                b->attrs = *attrs;
            b->progress.alignment = alignment;
            /* being_create_pc() already computed max_hp/limb HP, but from
             * default ATTR_BASE attrs and char_class 0 (CLASS_MAGE) --
             * BEFORE the real race/class-adjusted attrs and chosen class
             * were set just above. Recompute now that both are final, or a
             * Warrior's real HP bonus (etc.) would never actually apply. */
            b->progress.max_hp = being_calc_max_hp(b);
            b->progress.hp = b->progress.max_hp;
            being_limbs_full_heal(b);
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

bool player_name_exists(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool found = db_query(db, "select id from player where name='%s'", name)
                 && db_fetch_row(db);
    db_close(db);
    return found;
}

long player_id_for_name(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;
    long id = -1;
    if (db_query(db, "select id from player where name='%s'", name) && db_fetch_row(db))
        id = atol(db_get(db, "id"));
    db_close(db);
    return id;
}

bool player_set_load_room(const char *name, long account_id, int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update player set load_room=%i where name='%s' and account_id=%i",
                       vnum, name, (int)account_id);

    db_close(db);
    return ok;
}

bool player_set_title(const char *name, long account_id, const char *title) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* An empty title clears the column (SQL NULL); db_query's %s escapes the
     * free-form title text, so no injection through player-supplied strings. */
    bool ok;
    if (title && title[0])
        ok = db_query(db, "update player set title='%s' where name='%s' and account_id=%i",
                      title, name, (int)account_id);
    else
        ok = db_query(db, "update player set title=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

bool player_set_poofin(const char *name, long account_id, const char *msg) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (msg && msg[0])
        ok = db_query(db, "update player set poofin='%s' where name='%s' and account_id=%i",
                      msg, name, (int)account_id);
    else
        ok = db_query(db, "update player set poofin=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

bool player_set_poofout(const char *name, long account_id, const char *msg) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (msg && msg[0])
        ok = db_query(db, "update player set poofout='%s' where name='%s' and account_id=%i",
                      msg, name, (int)account_id);
    else
        ok = db_query(db, "update player set poofout=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

bool player_set_bamfin(const char *name, long account_id, const char *msg) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (msg && msg[0])
        ok = db_query(db, "update player set bamfin='%s' where name='%s' and account_id=%i",
                      msg, name, (int)account_id);
    else
        ok = db_query(db, "update player set bamfin=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

bool player_set_bamfout(const char *name, long account_id, const char *msg) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (msg && msg[0])
        ok = db_query(db, "update player set bamfout='%s' where name='%s' and account_id=%i",
                      msg, name, (int)account_id);
    else
        ok = db_query(db, "update player set bamfout=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
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
    if (db_query(db, "select level, experience, hp, max_hp, true_level, alignment, "
                      "basic_disc_pct, advanced_disc_pct, combat_disc_pct, practice_points, "
                      "rented_at from player_progress where player_id=%i",
                 (int)player_id)
        && db_fetch_row(db)) {
        out->level = atoi(db_get(db, "level"));
        out->experience = atol(db_get(db, "experience"));
        out->hp = atoi(db_get(db, "hp"));
        out->max_hp = atoi(db_get(db, "max_hp"));
        out->true_level = atoi(db_get(db, "true_level"));
        out->alignment = atoi(db_get(db, "alignment"));
        out->basic_disc_pct = atoi(db_get(db, "basic_disc_pct"));
        out->advanced_disc_pct = atoi(db_get(db, "advanced_disc_pct"));
        out->combat_disc_pct = atoi(db_get(db, "combat_disc_pct"));
        out->practice_points = atoi(db_get(db, "practice_points"));
        out->rented_at = atol(db_get(db, "rented_at"));
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
        "insert into player_progress (player_id, level, experience, hp, max_hp, true_level, alignment, "
        "basic_disc_pct, advanced_disc_pct, combat_disc_pct, practice_points, rented_at) "
        "values (%i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i) "
        "on duplicate key update level=%i, experience=%i, hp=%i, max_hp=%i, true_level=%i, alignment=%i, "
        "basic_disc_pct=%i, advanced_disc_pct=%i, combat_disc_pct=%i, practice_points=%i, rented_at=%i",
        (int)player_id, progress->level, (int)progress->experience, progress->hp, progress->max_hp, progress->true_level, progress->alignment,
        progress->basic_disc_pct, progress->advanced_disc_pct, progress->combat_disc_pct, progress->practice_points, (int)progress->rented_at,
        progress->level, (int)progress->experience, progress->hp, progress->max_hp, progress->true_level, progress->alignment,
        progress->basic_disc_pct, progress->advanced_disc_pct, progress->combat_disc_pct, progress->practice_points, (int)progress->rented_at);

    db_close(db);
    return ok;
}

being_t *player_load_admin(const char *name, int *out_load_room) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    being_t *b = NULL;
    int load_room = -1;
    if (db_query(db, "select id, name, account_id, load_room, handed, prompt_flags, "
                      "title, gender, appearance, pflags, poofin, poofout, bamfin, bamfout, "
                      "class, race from player "
                      "where name='%s'",
                 name)
        && db_fetch_row(db)) {
        long player_id = atol(db_get(db, "id"));
        long account_id = atol(db_get(db, "account_id"));
        b = being_create_pc(db_get(db, "name"), account_id, player_id);
        if (b) {
            b->handed_right = atoi(db_get(db, "handed"));
            b->prompt_flags = atoi(db_get(db, "prompt_flags"));
            snprintf(b->title, sizeof(b->title), "%s", db_get(db, "title"));
            b->gender = (gender_t)atoi(db_get(db, "gender"));
            snprintf(b->appearance, sizeof(b->appearance), "%s", db_get(db, "appearance"));
            b->pflags = atoi(db_get(db, "pflags"));
            snprintf(b->poofin, sizeof(b->poofin), "%s", db_get(db, "poofin"));
            snprintf(b->poofout, sizeof(b->poofout), "%s", db_get(db, "poofout"));
            snprintf(b->bamfin, sizeof(b->bamfin), "%s", db_get(db, "bamfin"));
            snprintf(b->bamfout, sizeof(b->bamfout), "%s", db_get(db, "bamfout"));
            b->char_class = (player_class_t)atoi(db_get(db, "class"));
            b->race = (player_race_t)atoi(db_get(db, "race"));
            load_room = atoi(db_get(db, "load_room"));
        }
    }

    db_close(db);

    if (b) {
        player_attrs_load(b->player_id, &b->attrs);
        player_progress_load(b->player_id, &b->progress);
        ensure_jesus_level(b);
        /* Deliberately NOT player_inventory_load() here: edplayer/cmd_set
         * both do `d->edplayer_work = *loaded;` / mutate-then-destroy on
         * this snapshot (a shallow struct copy) -- an object attached via
         * thing_move_to() would leave a dangling pointer the moment
         * being_destroy(loaded) frees it, since being_destroy() now frees
         * everything in the being's containment chain (see being.c). An
         * admin snapshot has no need to inspect inventory in this pass. */
    }
    if (out_load_room)
        *out_load_room = load_room;

    return b;
}

bool player_set_gender_by_name(const char *name, gender_t gender) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set gender=%i where name='%s'",
                       (int)gender, name);
    db_close(db);
    return ok;
}

bool player_set_handed_by_name(const char *name, int handed_right) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set handed=%i where name='%s'",
                       handed_right ? 1 : 0, name);
    db_close(db);
    return ok;
}

bool player_set_class_by_name(const char *name, player_class_t cls) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set class=%i where name='%s'",
                       (int)cls, name);
    db_close(db);
    return ok;
}

bool player_set_race_by_name(const char *name, player_race_t race) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set race=%i where name='%s'",
                       (int)race, name);
    db_close(db);
    return ok;
}

bool player_set_appearance_by_name(const char *name, const char *appearance) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set appearance='%s' where name='%s'",
                       appearance ? appearance : "", name);
    db_close(db);
    return ok;
}

bool player_set_level_by_name(const char *name, int level) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    long player_id = -1;
    if (db_query(db, "select id from player where name='%s'", name) && db_fetch_row(db))
        player_id = atol(db_get(db, "id"));

    if (player_id < 0) {
        db_close(db);
        return false;
    }

    /* A Tobin-created player always has a progress row (player_create seeds
     * one), but tolerate a missing row anyway with the same 100/100 HP
     * defaults the manual promotion SQL has used since Session 11. */
    bool ok = db_query(db,
        "insert into player_progress (player_id, level, experience, hp, max_hp) "
        "values (%i, %i, 0, 100, 100) "
        "on duplicate key update level=%i",
        (int)player_id, level, level);

    db_close(db);
    return ok;
}

bool player_set_prompt_flags(long player_id, int flags) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set prompt_flags=%i where id=%i",
                       flags, (int)player_id);
    db_close(db);
    return ok;
}

bool player_set_pflags(long player_id, int flags) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set pflags=%i where id=%i",
                       flags, (int)player_id);
    db_close(db);
    return ok;
}

bool player_save(long player_id, const being_t *b) {
    bool ok = player_attrs_save(player_id, &b->attrs);
    ok = player_progress_save(player_id, &b->progress) && ok;
    ok = player_inventory_save(player_id, b) && ok;
    return ok;
}
