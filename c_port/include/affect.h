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
    /* `invisibility` (Mage, level 17) and `dispel invisible` (Mage,
     * level 17, its counter). Real upstream (disc_mage_spirit.cc's
     * invisibility()/dispelInvisible()) also grants a -40 armor bonus
     * (APPLY_ARMOR) and doubles duration + armor on a crit success --
     * scoped down to a plain flag/timer affect, same shape as
     * AFFECT_SANCTUARY/AFFECT_BERSERK, no armor bonus and no crit
     * branch (same "no crit-fail branch ported" precedent as fear/
     * slumber above). Checked by combat_find_room_target() (an
     * invisible being can't be targeted by name, mirroring the
     * existing linkdead-untargetable gate) and cmd_look.c's room
     * listing (an invisible being doesn't show in `look`'s person
     * list) -- both skip the check entirely for an immortal viewer,
     * same "immortals see everything" convention as the linkdead tag.
     * No `detect invisibility` counter-check yet (that's its own
     * separate, higher-level roster entry, not built this pass) --
     * every mortal viewer is equally blind to it for now. */
    AFFECT_INVISIBLE,
    /* `blindness` (Cleric, level 21). Real upstream (disc_cleric_
     * afflictions.cc's blindness()) is checked all over the codebase
     * (look, movement, combat crit rolls, ...) -- scoped down to the
     * two cheapest, most visible spots: cmd_look.c refuses the room
     * description entirely (matching real cmd_look.cc's own AFF_BLIND
     * check almost verbatim, "You can't see a damn thing -- you're
     * blinded!"), and combat_strike() adds a flat to-hit penalty when
     * the ATTACKER is blinded (same shape as the existing DESTROYED_
     * LIMB_HIT_PENALTY modifier, not a literal port of any specific
     * real formula -- the real effect on to-hit isn't a single traced
     * number, just "checked in several places"). Plain flag/timer, no
     * stat modifier. Not ported: the TRUE_SIGHT/CLARITY immunity
     * (neither affect exists in Tobin), the already-blind refusal
     * (Tobin doesn't need it -- being_apply_affect() just refreshes
     * the duration), and the isNotPowerful() power-gap gate (no clean
     * Tobin equivalent, same cut summon's own version of this check
     * already made). */
    AFFECT_BLIND,
    /* `haste` (Mage, level 23, level-23 audit batch). Real upstream
     * (disc_mage_spirit.cc's haste()/applyHaste()) is a plain flag/timer
     * affect with no stat modifier of its own -- the actual speed comes
     * from Tobin having nothing like it to hook into structurally, since
     * combat here has no per-being "attacks per round" concept at all
     * (every fighter gets exactly one combat_strike() per
     * COMBAT_ROUND_PULSES, full stop). Ported as a genuine extra strike:
     * combat_process_run() (combat.c) gives a hasted fighter one bonus
     * combat_strike() against their opponent immediately after their
     * normal one each round -- "extra speed and actions", literally.
     * Single-target only (self by default, an ally if named), unlike the
     * real spell's "no target = whole group" case -- same disclosed
     * scope-cut as every other buff spell in this roster (meditate,
     * invisibility, ...) being single-target. No crit-success duration/
     * effect doubling either (same "no crit branch ported" precedent as
     * fear/slumber/invisibility/blindness above). */
    AFFECT_HASTE,
    /* `enhance weapon` (Mage, level 24, level-24 audit batch). Roster text
     * says "Permanently enchants a weapon" -- Tobin's objaffect bonus table
     * is keyed by item VNUM (the shared prototype), not per-instance
     * (obj_repo.c's obj_repo_hitroll_bonus()), so there's no runtime slot to
     * write a one-off permanent bonus onto a single weapon the way the real
     * spell would. Deviated (user approved 2026-07-29) to a temporary
     * to-hit buff instead, same shape/precedent as AFFECT_CURSE's own
     * "no separate hitroll stat -- lands on DEXTERITY" deviation, just the
     * positive-direction twin of it via affect_stat_target(). Self by
     * default or a named ally, same single-target scope-cut as haste. */
    AFFECT_ENHANCE_WEAPON,
    /* `detect invisibility` (Mage, level 25, level-25 audit batch). Real
     * upstream (disc_mage_spirit.cc's detectInvisibility()) sets
     * AFF_DETECT_INVISIBLE, which every invisible-being-visibility check
     * across the codebase already conditions on. Tobin's own
     * AFFECT_INVISIBLE gate (combat_find_room_target(), cmd_look.c) never
     * had a counter-check -- explicitly flagged as future work in
     * AFFECT_INVISIBLE's own doc comment above. This closes that loop: a
     * viewer with this affect active can target/see an invisible being
     * same as an immortal already could. Plain flag/timer, no stat
     * modifier. */
    AFFECT_DETECT_INVISIBLE,
    /* `detect magic` (Mage, level 25). Real upstream (disc_mage_alchemy.cc's
     * detectMagic()) sets AFF_DETECT_MAGIC, which lets a viewer see a
     * magical-aura marker on items/beings elsewhere in the codebase --
     * Tobin has no such per-object "this is magic" marker to reveal
     * (objaffect rows are permanent DB data, not something to newly
     * "detect"), so this lands as a flavor-only flag/timer affect, same
     * "no functional backing yet" precedent as several other buffs in
     * this roster. */
    AFFECT_DETECT_MAGIC,
    /* `bind` (Mage, level 25). Real upstream (disc_mage_sorcery.cc's
     * bind()) is "$N traps $N in a mass of sticky, web-like substance" --
     * ported as a genuine movement-blocking affect (checked by
     * do_move()/cmd_move.c, refuses to leave the room while active), a
     * real mechanic Tobin didn't have a use for yet. Plain flag/timer, no
     * stat modifier -- the real spell's -25 AC penalty on the victim
     * folds into no existing Tobin stat cleanly and is dropped, same "no
     * separate hitroll/AC stat" precedent as curse/blindness. */
    AFFECT_BIND,
    /* `infravision`/`mage sight` (Mage, level 28/1) -- real, working
     * dark-vision: checked by room_is_dark_for() (being.c) alongside the
     * existing immortal/always-lit/daytime/active-light exemptions, so a
     * caster with this affect active sees a normally-dark room fine with
     * no light source needed. Plain flag/timer, no stat modifier -- same
     * shape as AFFECT_BIND. */
    AFFECT_INFRAVISION,
    /* `faerie fire` (Mage, level 6) -- "Marks a target with a pink aura,
     * easier to hit". Real, working to-hit debuff: combat.c's strike
     * roll already has a defender-side "easier to hit" precedent
     * (a destroyed limb widens the attacker's modifier) -- this is the
     * same mechanic, gated on this affect instead. Plain flag/timer, no
     * stat modifier -- same shape as AFFECT_BIND/AFFECT_INFRAVISION. */
    AFFECT_FAERIE_FIRE,
    /* `feathery descent` (Mage, level 7) -- "A group buff that softens
     * falls". Real, working fall-damage mitigation: fall.c's own
     * `catfall` skill check (widens the survivable-depth threshold,
     * halves crush damage) already does exactly this for a skill; this
     * affect is checked alongside it so a cast buff gets the identical
     * treatment without needing the skill. Plain flag/timer, no stat
     * modifier. */
    AFFECT_FEATHERY_DESCENT,
    /* `beast soother` (Druid, level 5) -- "Calms a hostile or hunting
     * animal". Tobin's mob aggression is purely reactive (no lingering
     * hunt-memory to clear, mob_ai.c's mob_try_aggress() re-rolls fresh
     * every tick), so "calming" a mob means ending its CURRENT fight
     * (if any) and gating mob_try_aggress() off for the duration -- a
     * real, working ceasefire, not just flavor text. Plain flag/timer,
     * no stat modifier. */
    AFFECT_CALMED,
    /* `silence` (Mage, level 48) -- "Mutes a target, blocking their
     * spellcasting". Real, working gate: checked at the top of
     * cmd_cast()/cmd_pray() (both dispatchers), refusing to cast/pray
     * at all while active. Plain flag/timer, no stat modifier. */
    AFFECT_SILENCE,
    /* `shield of mists` (Shaman/Druid audit batch C, 2026-08-09) -- real
     * upstream is a big (-60, upstream's "lower is better" convention)
     * APPLY_ARMOR buff -- "a thick green mist" that makes the target
     * much harder to hit. Ported as a real, working defender-side to-
     * hit PENALTY for the attacker (combat.c), same flat-bonus shape as
     * AFFECT_FAERIE_FIRE just uses in the opposite direction. Plain
     * flag/timer, no stat modifier. */
    AFFECT_SHIELD_OF_MISTS,
    /* `living vines` (Shaman/Druid audit batch C, 2026-08-09) -- real
     * upstream applies BOTH an AC penalty (aff1, APPLY_ARMOR) and a
     * hitroll penalty (aff2, APPLY_SPELL_HITROLL) to the victim, plus
     * AFF_WEB (entangled). Tobin has no separate AC-modifier-by-amount
     * stat to hang the first on, so this maps onto its two closest REAL
     * working hooks instead: the exact same "easier to hit" flat bonus
     * AFFECT_FAERIE_FIRE already grants an attacker (combat.c), plus a
     * real movement-blocking effect (cmd_move.c), same shape
     * AFFECT_BIND already uses for its own "stuck in webbing" wording.
     * Plain flag/timer, no stat modifier. */
    AFFECT_LIVING_VINES,
    /* `thornflesh` (Shaman/Druid audit batch C, 2026-08-09) -- real
     * upstream: "thorns emerge from your body", a genuine damage-
     * reflection buff (combat.cc's own defender-side check: any melee
     * hit that lands reflects min(dmg-1, 3) back onto the attacker).
     * Ported verbatim -- same formula, checked in combat.c right where
     * a hit's final damage is known. Plain flag/timer, no stat
     * modifier. */
    AFFECT_THORNFLESH,
    /* `transfix` (Druid, Tier-2 port 2026-08-16) -- real upstream is a
     * Shaman-spider spell (disc_shaman_spider.cc's transfix()) that
     * mesmerizes a DUMB ANIMAL that isn't already fighting: "$N stares
     * transfixed into your eyes", holding it frozen and staring for a
     * level-scaled duration. Ported as a real "can't act" hold: checked
     * by cmd_attack.c (a transfixed being can't initiate an attack, same
     * gate shape as AFFECT_FEAR) and mob_ai.c's mob_try_aggress() (a
     * transfixed mob won't pick fresh fights, same gate shape as
     * AFFECT_CALMED). Distinct from fear (which forces a flee) and calmed
     * (which only ends/blocks a mob's aggression): transfix roots the
     * target in place AND blocks its own attacks. Plain flag/timer, no
     * stat modifier. Tobin has no "dumb animal" flag, so the upstream
     * dumb-animal-only restriction lands as "any non-fighting target"
     * (disclosed deviation; the cast branch still refuses an
     * already-fighting target, matching upstream's !victim->fight()). */
    AFFECT_TRANSFIX,
    /* `transform limb` (Druid, Tier-2 port 2026-08-16) -- real upstream
     * (disc_shaman_frog.cc's transformLimb()) turns one of the caster's
     * OWN limbs into an animal form, each limb granting a different real
     * effect (neck->waterbreath, arms->flying, hands->damroll+climb,
     * legs->swim). Tobin has no per-limb transformation subsystem, so
     * this maps the limb keywords onto Tobin's existing real affects:
     * "gills"/neck -> AFFECT_WATERBREATH, "wings"/arms -> AFFECT_FLYING
     * (both already fully working), and "claws"/hands -> THIS affect, a
     * STRENGTH buff (stat-modifying affect, see affect_stat_target(),
     * standing in for upstream's hands-case damroll bonus since Tobin's
     * barehand damage scales off STRENGTH). The head/legs limb cases are
     * scoped out (no clean Tobin hook). Self-only, same single-target
     * scope as the other buff spells. */
    AFFECT_TRANSFORMED_LIMB,
    /* `encamp` (missing-skill audit, generic/cross-class, Tier-3 port
     * 2026-08-16) -- real upstream (disc_advanced_adventuring.cc's
     * encamp()) sets a persistent camp affect keyed to the current room
     * that speeds HP/move regen for the camper (and, in a group, their
     * groupmates at a fraction). Tobin has no group-fraction hook, so
     * this lands as a camper-only timed regen buff (disclosed
     * divergence): while AFFECT_ENCAMP is up, regen_tick_run() adds an
     * extra HP/vitality increment. A plain flag/timer, no stat modifier.
     * Wears off on its own (a camp doesn't last forever) rather than
     * PERMANENT_DURATION -- Tobin has no "break camp"/move-cancels-camp
     * plumbing, so a timer is the honest, self-cleaning scope-cut. */
    AFFECT_ENCAMP,
    /* `fish` cooldown (Fishing/Fishlore audit, generic/cross-class,
     * Tier-3 port 2026-08-16) -- same per-being "picked this spot over
     * recently" throttle AFFECT_FORAGE_COOLDOWN provides for forage, so
     * `fish` isn't a free-food loop. Upstream tracks a per-ROOM "fished"
     * count that slowly recovers; Tobin has no per-room counter, so this
     * is a per-being cooldown instead (disclosed divergence). */
    AFFECT_FISH_COOLDOWN,
    /* `divine` cooldown (water-dowsing audit, generic/cross-class,
     * Tier-3 port 2026-08-16) -- upstream (disc_advanced_adventuring.cc's
     * divineMe()) puts a SKILL_DIVINATION recast timer on the dowser so
     * they can't refill a waterskin every round. Same shape here. */
    AFFECT_DIVINE_COOLDOWN,
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

/* `fish` cooldown -- same magnitude/spirit as FORAGE_COOLDOWN_ROUNDS
 * (a gathering skill shouldn't be spammable every round), Tier-3 port. */
#define FISH_COOLDOWN_ROUNDS 50

/* `divine` water-dowsing recast timer -- a bit longer than the gathering
 * cooldowns since pulling drinkable water out of thin air is meant to be
 * an occasional survival trick, not an every-minute tap. */
#define DIVINE_COOLDOWN_ROUNDS 100

/* `encamp` regen-buff lifespan -- a few real minutes of camp benefit
 * (same ~5-minute magnitude family as PET_CHARM/TRANSFORM above); the
 * camp then goes cold and must be re-pitched. */
#define ENCAMP_DURATION_ROUNDS 250

/* Extra HP (and vitality) added per regen tick while AFFECT_ENCAMP is up
 * -- a modest flat bonus on top of the normal rest-weighted amount, the
 * camper-only stand-in for upstream's camp regen boost. */
#define ENCAMP_REGEN_BONUS 2

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
