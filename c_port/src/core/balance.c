/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "balance.h"

static balance_mod_t g_class_balance[CLASS_COUNT];
static balance_mod_t g_race_balance[RACE_COUNT];

/* The "nothing has been changed" values -- every class/race behaves
 * completely normally until an immortal actually balances it. */
static const balance_mod_t NEUTRAL = {
    .hp_mult = 1.0f, .dmg_mult = 1.0f, .tohit_mod = 0, .ac_mod = 0,
    .mana_mult = 1.0f, .move_mult = 1.0f, .food_mult = 1.0f, .drink_mult = 1.0f,
    .resist_poison = 0, .resist_charm = 0, .resist_sleep = 0, .resist_paralysis = 0,
    .resist_energy = 0, .resist_heat = 0, .resist_cold = 0,
    .infravision = 0, .talent = 0,
};

/* Loads every class's and race's balance settings from the database
 * into memory, once, when the server starts up. Combat looks at this
 * in-memory copy on every single swing, so it never has to wait on a
 * database query mid-fight. */
void balance_cache_load(void) {
    for (player_class_t c = 0; c < CLASS_COUNT; c++) {
        if (!class_balance_load(c, &g_class_balance[c]))
            g_class_balance[c] = NEUTRAL;
    }
    for (player_race_t r = 0; r < RACE_COUNT; r++) {
        if (!race_balance_load(r, &g_race_balance[r]))
            g_race_balance[r] = NEUTRAL;
    }
}

/* Looks up the current balance settings for one class, straight from
 * memory (no database hit). Falls back to the neutral defaults if
 * given an out-of-range class, so it's always safe to call. */
const balance_mod_t *class_balance_get(player_class_t cls) {
    if (cls < 0 || cls >= CLASS_COUNT)
        return &NEUTRAL;
    return &g_class_balance[cls];
}

/* Same as class_balance_get(), for a race instead of a class. */
const balance_mod_t *race_balance_get(player_race_t race) {
    if (race < 0 || race >= RACE_COUNT)
        return &NEUTRAL;
    return &g_race_balance[race];
}

/* Saves a class's new balance settings to the database AND updates
 * the in-memory copy at the same time, so the change takes effect for
 * everyone immediately -- this is the only function that should ever
 * change a class's balance settings while the server is running.
 * Returns false (and changes nothing) if the database write fails. */
bool class_balance_set(player_class_t cls, const balance_mod_t *mods) {
    if (cls < 0 || cls >= CLASS_COUNT || !class_balance_save(cls, mods))
        return false;
    g_class_balance[cls] = *mods;
    return true;
}

/* Same as class_balance_set(), for a race instead of a class. */
bool race_balance_set(player_race_t race, const balance_mod_t *mods) {
    if (race < 0 || race >= RACE_COUNT || !race_balance_save(race, mods))
        return false;
    g_race_balance[race] = *mods;
    return true;
}
