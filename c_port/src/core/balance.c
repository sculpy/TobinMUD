/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "balance.h"

static balance_mod_t g_class_balance[CLASS_COUNT];
static balance_mod_t g_race_balance[RACE_COUNT];

static const balance_mod_t NEUTRAL = { 1.0f, 1.0f, 0, 0 };

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

const balance_mod_t *class_balance_get(player_class_t cls) {
    if (cls < 0 || cls >= CLASS_COUNT)
        return &NEUTRAL;
    return &g_class_balance[cls];
}

const balance_mod_t *race_balance_get(player_race_t race) {
    if (race < 0 || race >= RACE_COUNT)
        return &NEUTRAL;
    return &g_race_balance[race];
}

bool class_balance_set(player_class_t cls, const balance_mod_t *mods) {
    if (cls < 0 || cls >= CLASS_COUNT || !class_balance_save(cls, mods))
        return false;
    g_class_balance[cls] = *mods;
    return true;
}

bool race_balance_set(player_race_t race, const balance_mod_t *mods) {
    if (race < 0 || race >= RACE_COUNT || !race_balance_save(race, mods))
        return false;
    g_race_balance[race] = *mods;
    return true;
}
