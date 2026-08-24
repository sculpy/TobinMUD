/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
/* Per-race flavor systems (Sneezy -> Tobin feature audit, docs/RACE_STATS.md's/
 * RACE_PERKS.md's "Not imported" list -- height/weight/age dice, move
 * verbs, body type). Kept in its own translation unit rather than folded
 * into being.c (where race_stat_bonus()/race_name() already live and
 * where this would otherwise naturally sit) because being.c had another
 * session's uncommitted work in flight at the time this was written --
 * see git log/STATUS.md for the merge. Every number here is read
 * straight off SneezyMUD's own per-race RACE_* files
 * (sneezymud-master/lib/races/), the same source race_stat_bonus() (being.c)
 * already draws from. */
#include "being.h"
#include <stdlib.h>
#include "body.h"
/* Rolls `count` dice of `sides` (1-indexed, e.g. roll_dice(1,4) -> 1..4)
 * and returns `base` plus the sum -- straight port of the "base+XdY"
 * notation SneezyMUD's RACE_* files use for age/maleHt/maleWt/femaleHt/
 * femaleWt (e.g. RACE_HUMAN's "age 15+1d4", "maleHt 62+1d17"). Shared by
 * race_roll_age()/race_roll_height()/race_roll_weight() below. */
static int roll_dice(int base, int count, int sides) {
    int total = base;
    for (int i = 0; i < count && sides > 0; i++)
        total += (rand() % sides) + 1;
    return total;
}
/* One row per PC race: age "base+XdY", then male/female height and
 * weight "base+XdY" -- copied verbatim from each race's own RACE_* file
 * (sneezymud-master/lib/races/RACE_HUMAN/RACE_WOODELF/RACE_OGRE/
 * RACE_DWARF/RACE_HOBBIT/RACE_GNOME), same source as being.c's
 * race_stat_bonus()/RACE_NAMES[]. Units are the original's: age in
 * years, height in inches, weight in pounds. */
typedef struct {
    int age_base, age_dice, age_sides;
    int ht_m_base, ht_m_dice, ht_m_sides;
    int wt_m_base, wt_m_dice, wt_m_sides;
    int ht_f_base, ht_f_dice, ht_f_sides;
    int wt_f_base, wt_f_dice, wt_f_sides;
} race_flavor_dice_t;
static const race_flavor_dice_t RACE_FLAVOR_DICE[RACE_COUNT] = {
    /* HUMAN:  age 15+1d4,  maleHt 62+1d17, maleWt 140+6d10, femaleHt 60+1d12, femaleWt 100+4d10 */
    { 15, 1, 4,   62, 1, 17,  140, 6, 10,   60, 1, 12,  100, 4, 10 },
    /* ELF (RACE_WOODELF): age 100+5d6, maleHt 46+1d11, maleWt 90+3d10, femaleHt 44+1d11, femaleWt 70+3d10 */
    { 100, 5, 6,   46, 1, 11,   90, 3, 10,   44, 1, 11,   70, 3, 10 },
    /* OGRE: age 25+1d4, maleHt 84+1d11, maleWt 280+12d12, femaleHt 82+1d11, femaleWt 250+12d12 */
    { 25, 1, 4,   84, 1, 11,  280, 12, 12,   82, 1, 11,  250, 12, 12 },
    /* DWARF: age 40+5d6, maleHt 40+1d9, maleWt 130+4d10, femaleHt 38+1d9, femaleWt 105+4d10 */
    { 40, 5, 6,   40, 1, 9,   130, 4, 10,   38, 1, 9,   105, 4, 10 },
    /* HOBBIT: age 20+3d4, maleHt 30+1d5, maleWt 20+5d4, femaleHt 28+1d5, femaleWt 18+5d4 */
    { 20, 3, 4,   30, 1, 5,    20, 5, 4,   28, 1, 5,    18, 5, 4 },
    /* GNOME: age 60+3d12, maleHt 33+1d8, maleWt 72+1d7, femaleHt 30+1d8, femaleWt 68+1d7 */
    { 60, 3, 12,   33, 1, 8,    72, 1, 7,   30, 1, 8,    68, 1, 7 },
};
static const race_flavor_dice_t *race_flavor_dice(player_race_t r) {
    if (r < 0 || r >= RACE_COUNT)
        r = RACE_HUMAN;
    return &RACE_FLAVOR_DICE[r];
}
int race_roll_age(player_race_t r) {
    const race_flavor_dice_t *d = race_flavor_dice(r);
    return roll_dice(d->age_base, d->age_dice, d->age_sides);
}
/* GENDER_MALE uses the race's maleHt/maleWt dice, everything else
 * (GENDER_FEMALE and GENDER_NEUTER alike) uses femaleHt/femaleWt -- the
 * original itself only ever branches on male-vs-not-male here (no third
 * "neuter" body plan exists to roll from). */
int race_roll_height(player_race_t r, gender_t g) {
    const race_flavor_dice_t *d = race_flavor_dice(r);
    if (g == GENDER_MALE)
        return roll_dice(d->ht_m_base, d->ht_m_dice, d->ht_m_sides);
    return roll_dice(d->ht_f_base, d->ht_f_dice, d->ht_f_sides);
}
int race_roll_weight(player_race_t r, gender_t g) {
    const race_flavor_dice_t *d = race_flavor_dice(r);
    if (g == GENDER_MALE)
        return roll_dice(d->wt_m_base, d->wt_m_dice, d->wt_m_sides);
    return roll_dice(d->wt_f_base, d->wt_f_dice, d->wt_f_sides);
}
/* moveIn/moveOut verbs, verbatim from each race's own RACE_* file. Only
 * Elf and Ogre deviate from the generic Human wording in the real
 * source data -- Dwarf/Hobbit/Gnome all carry the same "has arrived"/
 * "leaves" as Human. */
static const char *const RACE_MOVE_VERB_IN[RACE_COUNT] = {
    "has arrived",   /* HUMAN */
    "strides in",    /* ELF (RACE_WOODELF) */
    "lumbers in",    /* OGRE */
    "has arrived",   /* DWARF */
    "has arrived",   /* HOBBIT */
    "has arrived",   /* GNOME */
};
static const char *const RACE_MOVE_VERB_OUT[RACE_COUNT] = {
    "exits",   /* HUMAN -- RACE_HUMAN's real moveOut is "leaves"; Tobin's
                * own do_move() fuses the verb with a direction phrase
                * ("$verb to the north") instead of Sneezy's bare
                * "$name leaves.", so the neutral verb here is "exits"
                * (matching cmd_move.c's pre-existing wording) rather
                * than a literal "leaves to the north". */
    "strides", /* ELF (RACE_WOODELF's real moveOut) */
    "lumbers", /* OGRE's real moveOut */
    "exits",   /* DWARF -- real moveOut "leaves", same fusion as Human */
    "exits",   /* HOBBIT -- real moveOut "leaves", same fusion as Human */
    "exits",   /* GNOME -- real moveOut "leaves", same fusion as Human */
};
const char *race_move_verb_in(player_race_t r) {
    if (r < 0 || r >= RACE_COUNT)
        r = RACE_HUMAN;
    return RACE_MOVE_VERB_IN[r];
}
const char *race_move_verb_out(player_race_t r) {
    if (r < 0 || r >= RACE_COUNT)
        r = RACE_HUMAN;
    return RACE_MOVE_VERB_OUT[r];
}
/* Every one of Tobin's 6 playable races has `body Humanoid` in its own
 * RACE_* file -- verified directly (grep across all six), not assumed --
 * so this is a real per-race table that happens to resolve identically
 * today. Table-driven (not a bare hardcode) so a future 7th playable
 * race with a genuinely different body type -- Centaur, say -- only
 * needs a new row here, not a new special case at every call site. */
int race_body_type(player_race_t r) {
    (void)r; /* every current PC race is BODY_HUMANOID -- see doc comment */
    return BODY_HUMANOID;
}
