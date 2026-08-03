/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "whittle.h"

#include <stddef.h>

/* From task_whittle.cc's initWhittle() table, minus the bows/arrows and
 * the one CLASS_SHAMAN totem (see whittle.h's own doc comment for why).
 * Every result_vnum confirmed live against Tobin's real seeded `obj`
 * table before porting (all present). `min_wood_weight` is each
 * result's own real seeded weight * 1.10 -- the same "target weight *
 * 1.10" formula task_whittleCreateNew() uses to size the wood
 * requirement, just applied as one flat threshold instead of that
 * function's partial-consume-multiple-logs loop. Keywords are Tobin's
 * own natural-language choice, not a literal port of the original's
 * internal hyphenated identifiers (those were never meant to be typed
 * by a player). */
static const whittle_recipe_t RECIPES[WHITTLE_RECIPE_COUNT] = {
    { "chair wooden sturdy",     "sturdy wooden chair",        174,    9.9  },
    { "dart wooden simple",      "simple wooden dart",         175,    2.2  },
    { "club wood light",         "light wood club",            176,    7.7  },
    { "staff wooden training",   "wooden training staff",      177,    4.4  },
    { "shield wooden simple",    "simple wooden shield",       178,   11.11 },
    { "ring wood simple",        "simple wood ring",           179,    0.44 },
    { "pipe wooden simple",      "simple wooden pipe",         180,    2.2  },
    { "pen wooden",              "wooden pen",                 181,    0.11 },
    { "toothpick",                "toothpick",                 182,    0.11 },
    { "stick walking",           "walking stick",              183,    7.7  },
    { "totem wooden",            "wooden totem",               184,    1.76 },
    { "chest wooden simple",     "simple wooden chest",        185,   13.2  },
    { "box wooden small",        "small wooden box",           186,    2.75 },
    { "dagger wooden small",     "small wooden dagger",        187,    3.3  },
    { "boat toy small",          "small toy boat",             188,    2.2  },
    { "length wood sturdy",      "sturdy length of wood",      189,    7.7  },
    { "statuette small",         "small statuette",            190,   11.0  },
    { "figurine miniature",      "miniature figurine",         191,    9.35 },
    { "idol moath",              "wooden idol of Moath",       192,    9.35 },
    { "idol lapsos",             "wooden idol of Lapsos",      193,    9.35 },
    { "idol mithros",            "wooden idol of Mithros",     194,    9.35 },
    { "idol gringar",            "wooden idol of Gringar",     195,    9.35 },
    { "idol peel",               "wooden idol of Peel",        197,    5.5  },
    { "sword training wooden",   "small wooden training sword",329,    6.6  },
    { "fishing pole",            "very nice fishing pole",     13862,  5.5  },
};

/* Looks up recipe `i` in RECIPES[] above by index, or NULL if out of
 * range -- the `whittle` command's own accessor into the ported recipe
 * table, same shape as cook_recipe_at(). */
const whittle_recipe_t *whittle_recipe_at(int i) {
    if (i < 0 || i >= WHITTLE_RECIPE_COUNT)
        return NULL;
    return &RECIPES[i];
}
