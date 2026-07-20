/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_AFFECT_H
#define TOBIN_AFFECT_H

#include <stdbool.h>

/* Affects system (buffs/debuffs/status) -- user 2026-07-11's backlog
 * ("Affects system (buffs/debuffs/status)"), self-sequenced after trap
 * mechanics. A small, fixed-size set of TIMED status effects a being
 * can be carrying at once, each just a type + how many combat rounds
 * (COMBAT_ROUND_PULSES, ~1.2s each) it has left. Decremented by
 * affect_tick_run() (a pulse callback, see main.c), which runs for
 * EVERY connected being regardless of whether they're currently
 * fighting -- a buff shouldn't only wear off while you're in combat.
 *
 * v1 scope: one real, working effect (AFFECT_SANCTUARY, wired to the
 * Cleric's "sanctuary" spell -- see cmd_pray.c/combat.c) to prove the
 * system end to end, rather than speculatively building mechanics for
 * every buff-flavored entry in skill.c's roster (same "one concrete
 * example over building everything at once" precedent as weapon depth/
 * trap mechanics). Add more affect_type_t values here as more skills/
 * spells get real mechanics. */

struct being; /* forward declare -- avoids a being.h <-> affect.h cycle,
               * since being.h needs active_affect_t for its own field */

typedef enum {
    AFFECT_NONE = 0,
    /* Halves incoming damage in combat_strike() -- the Cleric spell
     * "sanctuary" ("A strong aura that reduces incoming damage."). */
    AFFECT_SANCTUARY,
    /* Drinking from a puddle (cmd_drink.c) can poison the drinker instead
     * of/alongside a disease roll -- a debuff, not a one-shot hit, so it
     * shows in `affects`, drains HP on its own tick (affect_tick_run(),
     * affect.c), and is curable at a hospital (cmd_shop.c) exactly like a
     * disease, just its own separate affect type rather than folded into
     * affect_is_disease(). Immortals never catch it, same gating as
     * disease. */
    AFFECT_POISON,
    /* Diseases (TODO.md: "modest list affecting players (immortals
     * immune); pulse-driven affect/tick, cure path TBD"; user 2026-07-18:
     * "may as well include all disease now, from sneezy"). Reuse this
     * SAME buff/debuff array rather than a parallel disease.h/.c module
     * -- storage, the `affects` display, and expiry-message plumbing all
     * already exist; only the periodic HP-drain sub-tick
     * (affect_tick_run(), affect.c) is disease-specific (affect_is_
     * disease() + DISEASE_HP_DRAIN[], gated so it only fires every 10th
     * round rather than every affect_tick_run() call, or a "modest"
     * disease would out-damage what its own duration implies). Caught
     * by drinking from a puddle (cmd_drink.c); immortals never catch one
     * (gated at the application site) and stop taking damage from one
     * mid-tick if ever promoted while sick. Curable at any hospital
     * (cmd_shop.c), priced via affect_cure_price() below.
     *
     * This is the FULL upstream `diseaseTypeT` roster (misc/disease.h,
     * 27 entries, DISEASE_NULL..MAX_DISEASE) minus DISEASE_NULL (a
     * placeholder, no Tobin equivalent needed) and DISEASE_POISON
     * (already modeled separately as AFFECT_POISON above, applied by
     * the SAME drink roll rather than duplicated here) -- 26 entries,
     * kept CONTIGUOUS so affect_is_disease() below is a cheap range
     * check instead of a 26-way switch. Deliberately NOT a port of each
     * disease's own upstream spec_proc mechanic (disease.cc is ~2000
     * lines of bespoke per-disease effects -- blindness, muteness,
     * limping, spec-proc-driven progression stages, etc.) -- same v1
     * scope as the original 4: a name, a duration, a periodic HP drain,
     * and a hospital cure. A few upstream diseases (broken bone, numbed
     * limb, voicebox, eyeball) upstream tie into wound-flag/body-part
     * systems Tobin doesn't have; here they're just another flavor of
     * HP-drain debuff, same as a cold. */
    AFFECT_DISEASE_COLD,
    AFFECT_DISEASE_FLU,
    AFFECT_DISEASE_FROSTBITE,
    AFFECT_DISEASE_BLEEDING,
    AFFECT_DISEASE_INFECTION,
    AFFECT_DISEASE_HERPES,
    AFFECT_DISEASE_BROKEN_BONE,
    AFFECT_DISEASE_NUMBED_LIMB,
    AFFECT_DISEASE_VOICEBOX,
    AFFECT_DISEASE_EYEBALL,
    AFFECT_DISEASE_LUNG,
    AFFECT_DISEASE_STOMACH,
    AFFECT_DISEASE_HEMORRHAGE,
    AFFECT_DISEASE_LEPROSY,
    AFFECT_DISEASE_PLAGUE,
    AFFECT_DISEASE_SUFFOCATE,
    AFFECT_DISEASE_FOOD_POISONING,
    AFFECT_DISEASE_DROWNING,
    AFFECT_DISEASE_GARROTTE,
    AFFECT_DISEASE_SYPHILIS,
    AFFECT_DISEASE_BRUISED,
    AFFECT_DISEASE_SCURVY,
    AFFECT_DISEASE_DYSENTERY,
    AFFECT_DISEASE_PNEUMONIA,
    AFFECT_DISEASE_GANGRENE,
    AFFECT_DISEASE_EXTREME_PAIN,
    /* Water, drowning, flight (Sneezy → Tobin feature audit). Plain
     * timed buffs, same shape as AFFECT_SANCTUARY -- not diseases, kept
     * outside the contiguous AFFECT_DISEASE_* range on purpose so
     * affect_is_disease()'s range check stays a cheap bounds test.
     * AFFECT_WATERBREATH ("gills of flesh", Mage spell) lets a PC
     * survive an UNDERWATER sector without drowning (see
     * vitals_tick_run(), vitals.c). AFFECT_FLYING ("levitate", Mage
     * spell) quarters sector_move_cost()'s charge (cmd_move.c, same
     * quartering the original's own flight/levitate movement discount
     * uses) and, like the original's canFly(), bypasses drowning
     * entirely while airborne over water. */
    AFFECT_WATERBREATH,
    AFFECT_FLYING,
    AFFECT_COUNT,
} affect_type_t;

/* True for the AFFECT_DISEASE_* range above -- a plain bounds check since
 * they're kept contiguous in the enum on purpose. Used by affect_tick_run()
 * to gate the periodic HP-drain sub-tick, by cmd_drink.c to pick a random
 * disease to apply, and by cmd_shop.c's hospital to know what's curable. */
bool affect_is_disease(affect_type_t type);

/* Gold cost to cure `type` at a hospital (cmd_shop.c) -- covers both
 * AFFECT_POISON and every AFFECT_DISEASE_* value, roughly ranked by the
 * upstream DiseaseInfo[].cure_cost ordering (misc/disease.cc) but
 * rescaled into Tobin's much smaller gold economy (a torch is 3 gold),
 * same "roughly scaled to how nasty each one is" spirit as the original
 * 4-disease pricing. Returns 0 for AFFECT_NONE/AFFECT_SANCTUARY (never
 * offered as a cure at a hospital). */
int affect_cure_price(affect_type_t type);

/* Picks one of the 26 AFFECT_DISEASE_* values uniformly at random --
 * shared by anything that inflicts "a disease" without caring which one
 * (cmd_drink.c's puddle-drinking roll has its own weighted duration
 * table and predates this; this is for newer callers, e.g. cast/pray's
 * disease-inflicting spells, cmd_cast.c/cmd_pray.c). */
affect_type_t affect_random_disease(void);

#define MAX_ACTIVE_AFFECTS 4

typedef struct {
    affect_type_t type; /* AFFECT_NONE = this slot is empty */
    int rounds_left;
} active_affect_t;

/* Display name for an affect type ("Sanctuary", ...) -- used by the
 * `affects` command and affect-expiry messages. */
const char *affect_name(affect_type_t type);

/* Whether `b` currently has `type` active. */
bool being_has_affect(const struct being *b, affect_type_t type);

/* Starts (or refreshes, if already active) `type` on `b` for `rounds`
 * more rounds. Silently does nothing if `b`'s affect slots are all full
 * with OTHER affect types (a deliberately small, fixed cap -- see
 * MAX_ACTIVE_AFFECTS). */
void being_apply_affect(struct being *b, affect_type_t type, int rounds);

/* Removes `type` from `b` immediately, if present (no message -- callers
 * that want one print it themselves, e.g. on manual dispel). */
void being_remove_affect(struct being *b, affect_type_t type);

/* Pulse callback (registered in main.c, once every COMBAT_ROUND_PULSES):
 * counts every connected being's active affects down by one round,
 * clearing (and announcing the expiry of) any that hit zero. */
void affect_tick_run(long pulse_num);

#endif
