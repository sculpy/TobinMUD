/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "rent.h"

#include <stdlib.h>

#include "db.h"

/* Cached gamewide settings (see rent.h). Loaded once at boot by
 * rent_config_load(); the rent hot path never hits the DB. */
static int g_rent_tax_at_max = RENT_TAX_AT_MAX_DEFAULT;
static int g_rent_free_level = RENT_FREE_LEVEL_DEFAULT;

int rent_tax_at_max(void) { return g_rent_tax_at_max; }
int rent_free_level(void) { return g_rent_free_level; }

/* Upsert one integer game_config row. */
static void rent_config_persist(const char *name, int value) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    db_query(db, "insert into game_config (name, value) values ('%s', '%d') "
                 "on duplicate key update value='%d'",
             name, value, value);
    db_close(db);
}

/* Read one integer game_config row, `fallback` if missing/unreachable. */
static int rent_config_read(const char *name, int fallback) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return fallback;
    int out = fallback;
    if (db_query(db, "select value from game_config where name='%s'", name)
        && db_fetch_row(db)) {
        const char *v = db_get(db, "value");
        if (v && *v)
            out = atoi(v);
    }
    db_close(db);
    return out;
}

void rent_tax_at_max_set(int value) {
    if (value < 0)
        value = 0;
    g_rent_tax_at_max = value;
    rent_config_persist("rent_tax_at_max", value);
}

void rent_free_level_set(int value) {
    if (value < 0)
        value = 0;
    g_rent_free_level = value;
    rent_config_persist("rent_free_level", value);
}

void rent_config_load(void) {
    g_rent_tax_at_max = rent_config_read("rent_tax_at_max", RENT_TAX_AT_MAX_DEFAULT);
    g_rent_free_level = rent_config_read("rent_free_level", RENT_FREE_LEVEL_DEFAULT);
}

int rent_cost_for(const being_t *ch) {
    if (!ch || being_is_immortal((being_t *)ch))
        return 0;
    int level = ch->progress.level;
    if (level <= g_rent_free_level)
        return 0;
    /* level^3 scaled so a max-level mortal pays tax_at_max. long math: the
     * numerator peaks around 2000*50^3 = 250M, well inside a long. */
    long denom = (long)MORTAL_LEVEL_MAX * MORTAL_LEVEL_MAX * MORTAL_LEVEL_MAX;
    long cost = (long)g_rent_tax_at_max * level * level * level / denom;
    return (int)cost;
}

int rent_apply_charge(being_t *ch, int cost, int *bank_used) {
    if (bank_used)
        *bank_used = 0;
    if (!ch || cost <= 0)
        return 0;
    int owed = cost;
    int from_wallet = owed < ch->progress.gold ? owed : ch->progress.gold;
    ch->progress.gold -= from_wallet;
    owed -= from_wallet;
    int from_bank = 0;
    if (owed > 0) {
        from_bank = owed < ch->progress.bank_gold ? owed : ch->progress.bank_gold;
        ch->progress.bank_gold -= from_bank;
        owed -= from_bank;
    }
    if (bank_used)
        *bank_used = from_bank;
    return cost - owed;   /* actually taken (wallet + bank) */
}
