/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "balance_repo.h"

#include <stdlib.h>

#include "db.h"

/* The full column list, shared by the class_balance and race_balance
 * tables (identical schema -- see db/tobin/balance.sql). Kept in one
 * place so the four functions below can't drift out of column order. */
#define BALANCE_COLS \
    "hp_mult, dmg_mult, tohit_mod, ac_mod, " \
    "mana_mult, move_mult, food_mult, drink_mult, " \
    "resist_poison, resist_charm, resist_sleep, resist_paralysis, " \
    "resist_energy, resist_heat, resist_cold, infravision, talent"

/* Fills `out` from the current DB row (already fetched). */
static void balance_read_row(db_conn_t *db, balance_mod_t *out) {
    out->hp_mult = (float)atof(db_get(db, "hp_mult"));
    out->dmg_mult = (float)atof(db_get(db, "dmg_mult"));
    out->tohit_mod = atoi(db_get(db, "tohit_mod"));
    out->ac_mod = atoi(db_get(db, "ac_mod"));
    out->mana_mult = (float)atof(db_get(db, "mana_mult"));
    out->move_mult = (float)atof(db_get(db, "move_mult"));
    out->food_mult = (float)atof(db_get(db, "food_mult"));
    out->drink_mult = (float)atof(db_get(db, "drink_mult"));
    out->resist_poison = atoi(db_get(db, "resist_poison"));
    out->resist_charm = atoi(db_get(db, "resist_charm"));
    out->resist_sleep = atoi(db_get(db, "resist_sleep"));
    out->resist_paralysis = atoi(db_get(db, "resist_paralysis"));
    out->resist_energy = atoi(db_get(db, "resist_energy"));
    out->resist_heat = atoi(db_get(db, "resist_heat"));
    out->resist_cold = atoi(db_get(db, "resist_cold"));
    out->infravision = atoi(db_get(db, "infravision"));
    out->talent = atoi(db_get(db, "talent"));
}

/* Writes one balance row (class or race) via INSERT ... ON DUPLICATE KEY
 * UPDATE. `key_col` is "class" or "race", `key` the enum value. */
static bool balance_write_row(const char *table, const char *key_col, int key,
                              const balance_mod_t *in) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db,
        "insert into %s (%s, " BALANCE_COLS ") values "
        "(%i, %f, %f, %i, %i, %f, %f, %f, %f, %i, %i, %i, %i, %i, %i, %i, %i, %i) "
        "on duplicate key update hp_mult=%f, dmg_mult=%f, tohit_mod=%i, ac_mod=%i, "
        "mana_mult=%f, move_mult=%f, food_mult=%f, drink_mult=%f, "
        "resist_poison=%i, resist_charm=%i, resist_sleep=%i, resist_paralysis=%i, "
        "resist_energy=%i, resist_heat=%i, resist_cold=%i, infravision=%i, talent=%i",
        table, key_col, key,
        (double)in->hp_mult, (double)in->dmg_mult, in->tohit_mod, in->ac_mod,
        (double)in->mana_mult, (double)in->move_mult, (double)in->food_mult, (double)in->drink_mult,
        in->resist_poison, in->resist_charm, in->resist_sleep, in->resist_paralysis,
        in->resist_energy, in->resist_heat, in->resist_cold, in->infravision, in->talent,
        (double)in->hp_mult, (double)in->dmg_mult, in->tohit_mod, in->ac_mod,
        (double)in->mana_mult, (double)in->move_mult, (double)in->food_mult, (double)in->drink_mult,
        in->resist_poison, in->resist_charm, in->resist_sleep, in->resist_paralysis,
        in->resist_energy, in->resist_heat, in->resist_cold, in->infravision, in->talent);
    db_close(db);
    return ok;
}

/* Reads one class's balance row out of the database into `out`.
 * Returns false if the database couldn't be reached or the row isn't there. */
bool class_balance_load(player_class_t cls, balance_mod_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool found = false;
    if (db_query(db, "select " BALANCE_COLS " from class_balance where class=%i", (int)cls)
        && db_fetch_row(db)) {
        balance_read_row(db, out);
        found = true;
    }
    db_close(db);
    return found;
}

/* Writes one class's balance settings into the database. */
bool class_balance_save(player_class_t cls, const balance_mod_t *in) {
    return balance_write_row("class_balance", "class", (int)cls, in);
}

/* Same as class_balance_load(), but for a race's balance row. */
bool race_balance_load(player_race_t race, balance_mod_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool found = false;
    if (db_query(db, "select " BALANCE_COLS " from race_balance where race=%i", (int)race)
        && db_fetch_row(db)) {
        balance_read_row(db, out);
        found = true;
    }
    db_close(db);
    return found;
}

/* Same as class_balance_save(), but for a race's balance row. */
bool race_balance_save(player_race_t race, const balance_mod_t *in) {
    return balance_write_row("race_balance", "race", (int)race, in);
}
