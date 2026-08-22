/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_WHITTLE_H
#define TOBIN_WHITTLE_H

/* Whittle profession (Sneezy -> Tobin feature audit, TODO.md "Deferred
 * decisions" -- the second of the two `task/` professions, alongside
 * [[cook]]). Checked the real upstream first (task_whittle.h/.cc): a
 * multi-tick task (carve/scrape/smooth pulses over many game ticks)
 * that can also produce bows/arrows via a whole separate TArrow/TBow
 * weapon-durability subsystem (sharpness, structure points, damage
 * deviation) Tobin has no equivalent of at all -- Tobin has no ranged-
 * weapon/ammo mechanics beyond a generic OBJ_CAT_AMMO placeholder, and
 * no generic "run a task over N ticks" primitive (same gap already
 * disclosed for yoginsa's own scope-down). Scoped down to match what
 * Cook already established as the profession shape: a single-action
 * command, real ingredient consumption, real result vnums -- every one
 * confirmed live against Tobin's actual seeded `obj` table before
 * porting. Bows/arrows/crossbows (task_whittle.cc's own "Arrows"/"Bows"
 * sections, vnums 166-173) and the one CLASS_SHAMAN-gated totem (Tobin
 * has no Shaman class) are the disclosed cuts; every other real,
 * class-unrestricted recipe from the original's `initWhittle()` table
 * is kept. */

#define WHITTLE_RECIPE_COUNT 25

typedef struct {
    const char *keywords;   /* e.g. "wooden-chest small simple chest" -- `whittle <keywords>` match */
    const char *name;       /* e.g. "small simple wooden chest" -- for messages */
    int result_vnum;
    double min_wood_weight; /* total carried wood-log weight required (real seeded vnums 75-88) */
} whittle_recipe_t;

const whittle_recipe_t *whittle_recipe_at(int i);

#endif
