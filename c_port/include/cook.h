/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_COOK_H
#define TOBIN_COOK_H

/* Cook profession (Sneezy -> Tobin feature audit, user 2026-07-26:
 * "professions" -- task_cook.h/.cc, a real ingredient-matching recipe
 * system, ported verbatim: 13 real recipes, every referenced vnum/liquid
 * type/race confirmed live against Tobin's actual seeded data before
 * porting (all present, zero missing). Two ingredient kinds needed new
 * Tobin plumbing to support for real (not stubbed): COOK_ING_LIQUID reuses
 * liquids.h's existing carried-container val[] fields; COOK_ING_CORPSE
 * needed a new corpse->val[2]=source mob_race field (obj.h's own CORPSE
 * val[] doc comment) since Tobin's corpse objects previously carried no
 * race data at all. */

#define COOK_RECIPE_COUNT 13

typedef enum {
    COOK_ING_VNUM,   /* value = a specific obj vnum */
    COOK_ING_LIQUID, /* value = liquid_info() ordinal (liquids.h); amt = units */
    COOK_ING_CORPSE, /* value = mob_race index (being.h); amt always 1, room floor only */
    COOK_ING_AMMO,   /* any OBJ_CAT_AMMO item; value unused */
} cook_ing_kind_t;

typedef struct {
    int recipe;  /* index into cook_recipe_at() */
    int slot;    /* ingredient slot within the recipe -- multiple rows
                    sharing the same (recipe, slot) are alternatives
                    ("any of these"), not all required together */
    int amt;     /* liquid units, or item/corpse count (always 1 today) */
    cook_ing_kind_t kind;
    int value;
} cook_ing_row_t;

typedef struct {
    const char *keywords; /* e.g. "steak marinated" -- `cook <keywords>` match */
    const char *name;     /* e.g. "marinated steak" -- for messages */
    int result_vnum;
} cook_recipe_t;

const cook_recipe_t *cook_recipe_at(int i);

/* All ingredient rows, count via *out_count. Iterate and group by
 * (recipe, slot) to find each recipe's real requirement set. */
const cook_ing_row_t *cook_ingredient_rows(int *out_count);

#endif
