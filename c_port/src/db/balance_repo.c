/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "balance_repo.h"

#include <stdlib.h>

#include "db.h"

/* Reads one class's balance row out of the database into `out`.
 * Returns false if the database couldn't be reached or the row isn't
 * there. */
bool class_balance_load(player_class_t cls, balance_mod_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select hp_mult, dmg_mult, tohit_mod, ac_mod from class_balance where class=%i",
                 (int)cls)
        && db_fetch_row(db)) {
        out->hp_mult = (float)atof(db_get(db, "hp_mult"));
        out->dmg_mult = (float)atof(db_get(db, "dmg_mult"));
        out->tohit_mod = atoi(db_get(db, "tohit_mod"));
        out->ac_mod = atoi(db_get(db, "ac_mod"));
        found = true;
    }
    db_close(db);
    return found;
}

/* Writes one class's balance settings into the database, creating the
 * row if it doesn't exist yet or overwriting it if it does. */
bool class_balance_save(player_class_t cls, const balance_mod_t *in) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into class_balance (class, hp_mult, dmg_mult, tohit_mod, ac_mod) "
        "values (%i, %f, %f, %i, %i) "
        "on duplicate key update hp_mult=%f, dmg_mult=%f, tohit_mod=%i, ac_mod=%i",
        (int)cls, (double)in->hp_mult, (double)in->dmg_mult, in->tohit_mod, in->ac_mod,
        (double)in->hp_mult, (double)in->dmg_mult, in->tohit_mod, in->ac_mod);

    db_close(db);
    return ok;
}

/* Same as class_balance_load(), but for a race's balance row instead
 * of a class's. */
bool race_balance_load(player_race_t race, balance_mod_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select hp_mult, dmg_mult, tohit_mod, ac_mod from race_balance where race=%i",
                 (int)race)
        && db_fetch_row(db)) {
        out->hp_mult = (float)atof(db_get(db, "hp_mult"));
        out->dmg_mult = (float)atof(db_get(db, "dmg_mult"));
        out->tohit_mod = atoi(db_get(db, "tohit_mod"));
        out->ac_mod = atoi(db_get(db, "ac_mod"));
        found = true;
    }
    db_close(db);
    return found;
}

/* Same as class_balance_save(), but for a race's balance row instead
 * of a class's. */
bool race_balance_save(player_race_t race, const balance_mod_t *in) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into race_balance (race, hp_mult, dmg_mult, tohit_mod, ac_mod) "
        "values (%i, %f, %f, %i, %i) "
        "on duplicate key update hp_mult=%f, dmg_mult=%f, tohit_mod=%i, ac_mod=%i",
        (int)race, (double)in->hp_mult, (double)in->dmg_mult, in->tohit_mod, in->ac_mod,
        (double)in->hp_mult, (double)in->dmg_mult, in->tohit_mod, in->ac_mod);

    db_close(db);
    return ok;
}
