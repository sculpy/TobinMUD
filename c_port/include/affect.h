/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
    /* Pet/charm (Sneezy → Tobin feature audit). Carried by a SUMMONED PET
     * MOB, never a PC -- marks it as charmed and times its lifespan.
     * Reuses this same generic buff/debuff array rather than a new field
     * on being_t (same "storage/display/expiry plumbing already exists"
     * reasoning as the disease block above), but its expiry is NOT the
     * generic "wears off" message -- affect_tick_run() special-cases it
     * in tick_being_affects() to actually dissolve and being_destroy()
     * the pet, since letting a charm run out just means the mob keeps
     * existing uncontrolled, matching Sneezy's own real behavior of the
     * charmed creature's affect simply ending its obedience. See
     * being_summon_charmed_pet() (being.c) for how one gets created. */
    AFFECT_CHARMED,
    /* Transformation (Sneezy → Tobin feature audit, Mage "polymorph").
     * Carried by the TEMPORARY mob body a polymorphed player's descriptor
     * is currently attached to (d->character points at it, same swap
     * shape `possess`/`return` already use -- see descriptor.h's
     * `possess_original` field comment). Times how long the
     * transformation lasts; expiry is special-cased in
     * tick_being_affects() to swap the player's descriptor back to their
     * own body (not the generic "wears off" message) and destroy the
     * temporary mob body, mirroring `return`'s own logic. See
     * being_start_polymorph() (being.c). */
    AFFECT_POLYMORPH,
    /* Crafting & extraction (Sneezy → Tobin feature audit, "Planting"'s
     * neighboring audit item): `forage`'s anti-spam cooldown -- plain
     * generic expiry (no special-casing in tick_being_affects(), just the
     * ordinary "wears off" message), gates cmd_forage.c's next attempt via
     * being_has_affect(). Simplification of the original's separate
     * success/failure cooldown durations (4 mud hours / 2 mud hours) down
     * to one flat duration regardless of outcome. */
    AFFECT_FORAGE_COOLDOWN,
    /* Full spell/skill/prayer roster import, Druid's 6 named Shaman
     * spells (user 2026-07-26): Stupidity (disc_shaman.cc) -- a
     * level-scaled INTELLIGENCE penalty, the first stat-modifying affect
     * in Tobin (see being_apply_stat_affect(), affect.c's own
     * affect_stat_target()). */
    AFFECT_STUPIDITY,
    /* Spell/skill functional-completeness audit (2026-07-27): `berserk`
     * (Warrior, level 1) -- a plain flag/timer affect, same shape as
     * AFFECT_SANCTUARY. Checked by combat.c's parry roll (a berserking
     * attacker's hits can't be parried) and cmd_rescue.c (a berserking
     * ally can't be rescued), matching the roster's own one-line
     * description ("much harder to rescue or parry while raging"). */
    AFFECT_BERSERK,
    /* `rally` (Warrior, level 1) -- a stat-modifying affect (see
     * affect_stat_target(), same shape as AFFECT_STUPIDITY but a
     * positive STRENGTH bonus instead of a penalty), applied to every
     * ally in the room by cmd_rally.c. */
    AFFECT_RALLY,
    /* `curse` (Cleric, level 13, level-5+ audit list). Real upstream
     * (misc/magicutils.cc's genericCurse()) is a hitroll penalty plus a
     * worsened paralysis-immunity penalty -- Tobin has no separate
     * hitroll stat or paralysis affect yet, so this lands as a
     * level-scaled DEXTERITY penalty (affect_stat_target() below),
     * standing in for the hitroll debuff since combat_strike()'s own
     * to-hit roll is driven directly off DEXTERITY. See cmd_pray.c. */
    AFFECT_CURSE,
    /* `slumber` (Mage, level 13, level-5+ audit list). Real upstream
     * (disc_mage_spirit.cc's slumber()/rawSleep()) puts the victim into
     * POSITION_SLEEPING for a timed duration with a luck-save resist
     * roll. Special-cased in affect.c's tick_being_affects(): applying
     * it sets position to POSITION_SLEEPING immediately (cmd_cast.c);
     * expiring it wakes the being back up (POSITION_STANDING) with its
     * own message instead of the generic "wears off" line, same
     * dissolve/revert shape as AFFECT_CHARMED/AFFECT_POLYMORPH. */
    AFFECT_SLEEP,
    /* `fear` (Mage, level 14, level-5+ audit list). Real upstream
     * (disc_mage_spirit.cc's fear()) forces an immediate flee, then
     * keeps compelling the victim to keep running while it lingers. A
     * plain flag/timer affect, same shape as AFFECT_SANCTUARY/
     * AFFECT_BERSERK -- no stat modifier. Checked by cmd_attack.c (a
     * feared being can't initiate an attack) and applied/triggered by
     * cmd_cast.c, which also reuses cmd_flee.c's own flee logic for the
     * immediate "run for your life" moment on a PC victim. */
    AFFECT_FEAR,
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

/* Default AFFECT_CHARMED lifespan for a summoned pet (Pet/charm, Sneezy →
 * Tobin feature audit) -- roughly 5 real minutes at COMBAT_ROUND_PULSES'
 * ~1.2s/round, one shared duration across all three pet-summon spells
 * (cmd_cast.c's "conjure elemental air/earth/fire/water" and "animal
 * companion", cmd_pray.c's "summon swarm") rather than a per-spell tuned
 * value -- a Tobin-scale simplification of Sneezy's own per-spell
 * duration formulas. */
#define PET_CHARM_DURATION_ROUNDS 250

/* Default AFFECT_POLYMORPH lifespan (Transformation, Sneezy → Tobin
 * feature audit) -- same ~5-minute magnitude as PET_CHARM_DURATION_ROUNDS
 * (both roughly "a while, not the whole session"), kept as its own named
 * constant rather than reused directly since the two features are
 * conceptually unrelated and only coincidentally share a duration. */
#define TRANSFORM_DURATION_ROUNDS 250

/* `forage` cooldown (Crafting & extraction, Sneezy → Tobin feature audit)
 * -- roughly 1 real minute at COMBAT_ROUND_PULSES' ~1.2s/round, short
 * enough to not feel punitive but long enough that spamming `forage`
 * every round isn't a free-food loop. */
#define FORAGE_COOLDOWN_ROUNDS 50

#define MAX_ACTIVE_AFFECTS 4

typedef struct {
    affect_type_t type; /* AFFECT_NONE = this slot is empty */
    int rounds_left;
    int modifier;       /* nonzero only for a stat-modifying affect (e.g.
                            AFFECT_STUPIDITY's INT penalty) -- the exact
                            delta already applied to the target attrs_t
                            field, reversed automatically on expiry/
                            removal. See being_apply_stat_affect(). */
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

/* Same as being_apply_affect(), but for a STAT-modifying affect (e.g.
 * AFFECT_STUPIDITY): `modifier` is added directly to the attribute
 * affect_stat_target() (affect.c) says this type modifies, immediately,
 * and reversed automatically the moment the affect expires (tick_being_
 * affects()) or is explicitly removed (being_remove_affect()) -- same
 * "apply now, reverse later" shape obj.c's own equip/unequip stat bonus
 * already uses, just timer-driven instead of equip-driven. Refreshing an
 * already-active instance of the same type first reverses the OLD
 * modifier before applying the new one, so re-casting a weaker version
 * of the same debuff on top of a stronger one can't leave a stale,
 * doubled-up penalty behind. */
void being_apply_stat_affect(struct being *b, affect_type_t type, int rounds, int modifier);

/* Removes `type` from `b` immediately, if present (no message -- callers
 * that want one print it themselves, e.g. on manual dispel). */
void being_remove_affect(struct being *b, affect_type_t type);

/* Pulse callback (registered in main.c, once every COMBAT_ROUND_PULSES):
 * counts every connected being's active affects down by one round,
 * clearing (and announcing the expiry of) any that hit zero. */
void affect_tick_run(long pulse_num);

#endif
