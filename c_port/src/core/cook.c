/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cook.h"

#include <stddef.h>

/* Verbatim from task_cook.h's recipes[] -- keywords/name/result vnum,
 * every vnum confirmed live against Tobin's real seeded `obj` table
 * before porting (all present). */
static const cook_recipe_t RECIPES[COOK_RECIPE_COUNT] = {
    { "steak marinated",         "marinated steak",           405 },
    { "catfish fried",           "fried catfish",              14358 },
    { "rat stick",               "rat on a stick",             14352 },
    { "potatoes mashed",         "side of mashed potatoes",    31773 },
    { "salad side",              "small side salad",           31774 },
    { "steak dinner",            "steak dinner",                31775 },
    { "chicken fried",           "fried chicken",               3324 },
    { "pita sandwich",           "pita sandwich",               34700 },
    { "offal pot pie",           "offal pot pie",               34701 },
    { "beggar stew",             "beggar stew",                 34704 },
    { "berry friendship bread",  "berry friendship bread",     34705 },
    { "pancake",                 "pancake",                     34708 },
    { "berry pancake",           "berry pancake",               34709 },
};

/* Verbatim from task_cook.h's ingredients[] -- (recipe, slot, amt, type,
 * value). GENERIC_STEAK (31365, low.h) and liquid ordinals (liquids.h,
 * verbatim liqTypeT order: LIQ_WATER=0, LIQ_WHISKY=5) and mob races
 * (being.h's MOB_RACE_NAMES[]: BIRD=16, RODENT=41) resolved to their real
 * Tobin/original values, not re-invented. */
static const cook_ing_row_t ROWS[] = {
    /* marinated steak (recipe 0) */
    { 0, 1, 1, COOK_ING_VNUM, 31365 },   /* any steak (GENERIC_STEAK) */
    { 0, 2, 3, COOK_ING_LIQUID, 5 },     /* 3 units whiskey */
    { 0, 3, 1, COOK_ING_VNUM, 432 },     /* 1 orange */

    /* fried catfish (1) */
    { 1, 1, 1, COOK_ING_VNUM, 13803 },   /* catfish */
    { 1, 2, 1, COOK_ING_VNUM, 263 },     /* jar of whale grease */

    /* rat on a stick (2) */
    { 2, 1, 1, COOK_ING_CORPSE, 41 },    /* RODENT corpse */
    { 2, 2, 1, COOK_ING_AMMO, 0 },       /* 1 arrow */

    /* mashed potatoes (3) */
    { 3, 1, 1, COOK_ING_VNUM, 31766 },   /* potato */
    { 3, 2, 1, COOK_ING_VNUM, 31767 },   /* butter */

    /* side salad (4) -- slot 1 has 4 lettuce alternatives */
    { 4, 1, 1, COOK_ING_VNUM, 10037 },
    { 4, 1, 1, COOK_ING_VNUM, 14349 },
    { 4, 1, 1, COOK_ING_VNUM, 28947 },
    { 4, 1, 1, COOK_ING_VNUM, 31768 },
    { 4, 2, 1, COOK_ING_VNUM, 14348 },   /* tomato */
    { 4, 3, 1, COOK_ING_VNUM, 31769 },   /* dressing */
    { 4, 4, 1, COOK_ING_VNUM, 31770 },   /* onion */
    { 4, 5, 1, COOK_ING_VNUM, 31771 },   /* carrot */
    { 4, 6, 1, COOK_ING_VNUM, 31772 },   /* croutons */

    /* steak dinner (5) -- built from three other recipes' own results */
    { 5, 1, 1, COOK_ING_VNUM, 405 },     /* marinated steak (recipe 0) */
    { 5, 2, 1, COOK_ING_VNUM, 31773 },   /* mashed potatoes (recipe 3) */
    { 5, 3, 1, COOK_ING_VNUM, 31774 },   /* side salad (recipe 4) */

    /* fried chicken (6) */
    { 6, 1, 1, COOK_ING_CORPSE, 16 },    /* BIRD corpse */
    { 6, 2, 1, COOK_ING_VNUM, 263 },     /* jar of whale grease */

    /* pita sandwich (7) -- slot 2 has the same 4 lettuce alternatives */
    { 7, 1, 1, COOK_ING_VNUM, 25550 },   /* pita */
    { 7, 2, 1, COOK_ING_VNUM, 10037 },
    { 7, 2, 1, COOK_ING_VNUM, 14349 },
    { 7, 2, 1, COOK_ING_VNUM, 28947 },
    { 7, 2, 1, COOK_ING_VNUM, 31768 },
    { 7, 3, 1, COOK_ING_VNUM, 14348 },   /* tomato */

    /* offal pot pie (8) */
    { 8, 1, 1, COOK_ING_VNUM, 10030 },   /* offal */
    { 8, 2, 1, COOK_ING_VNUM, 256 },     /* gnome flour */
    { 8, 3, 3, COOK_ING_LIQUID, 0 },     /* 3 units water */
    { 8, 4, 1, COOK_ING_VNUM, 34703 },   /* parsley */

    /* beggar stew (9) -- vnum 10913 is a real joke ingredient, a plain
     * rock, matching task_cook.h's own BOGUS_PLACEHOLDER naming */
    { 9, 1, 1, COOK_ING_VNUM, 10030 },   /* offal */
    { 9, 2, 1, COOK_ING_VNUM, 10913 },   /* a small rock */
    { 9, 3, 10, COOK_ING_LIQUID, 0 },    /* 10 units water */

    /* berry friendship bread (10) -- slot 2 has 6 berry alternatives */
    { 10, 1, 1, COOK_ING_VNUM, 256 },    /* gnome flour */
    { 10, 2, 1, COOK_ING_VNUM, 276 },
    { 10, 2, 1, COOK_ING_VNUM, 5701 },
    { 10, 2, 1, COOK_ING_VNUM, 10900 },
    { 10, 2, 1, COOK_ING_VNUM, 10907 },
    { 10, 2, 1, COOK_ING_VNUM, 10911 },
    { 10, 2, 1, COOK_ING_VNUM, 24703 },
    { 10, 3, 3, COOK_ING_LIQUID, 0 },    /* 3 units water */
    { 10, 4, 1, COOK_ING_VNUM, 34706 },  /* sugar */
    { 10, 5, 1, COOK_ING_VNUM, 34707 },  /* yeast */

    /* pancake (11) */
    { 11, 1, 1, COOK_ING_VNUM, 34706 },  /* sugar */
    { 11, 2, 1, COOK_ING_VNUM, 256 },    /* gnome flour */
    { 11, 3, 2, COOK_ING_LIQUID, 0 },    /* 2 units water */

    /* berry pancake (12) -- slot 4 has the same 6 berry alternatives */
    { 12, 1, 1, COOK_ING_VNUM, 34706 },  /* sugar */
    { 12, 2, 1, COOK_ING_VNUM, 256 },    /* gnome flour */
    { 12, 3, 2, COOK_ING_LIQUID, 0 },    /* 2 units water */
    { 12, 4, 1, COOK_ING_VNUM, 276 },
    { 12, 4, 1, COOK_ING_VNUM, 5701 },
    { 12, 4, 1, COOK_ING_VNUM, 10900 },
    { 12, 4, 1, COOK_ING_VNUM, 10907 },
    { 12, 4, 1, COOK_ING_VNUM, 10911 },
    { 12, 4, 1, COOK_ING_VNUM, 24703 },
};
#define ROW_COUNT (sizeof(ROWS) / sizeof(ROWS[0]))

/* Looks up recipe `i` in RECIPES[] above by index, or NULL if out of
 * range -- the `cook` command's own accessor into the verbatim-ported
 * recipe table. */
const cook_recipe_t *cook_recipe_at(int i) {
    if (i < 0 || i >= COOK_RECIPE_COUNT)
        return NULL;
    return &RECIPES[i];
}

/* Returns the full ROWS[] ingredient table above and, via `out_count`,
 * how many rows it has -- the `cook` command scans this flat table
 * (filtering by recipe index) to check/consume ingredients, since a
 * recipe can have multiple alternative rows for the same slot (see the
 * berry-alternatives comments above). */
const cook_ing_row_t *cook_ingredient_rows(int *out_count) {
    if (out_count)
        *out_count = (int)ROW_COUNT;
    return ROWS;
}
