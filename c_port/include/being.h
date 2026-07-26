/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BEING_H
#define TOBIN_BEING_H

#include <stdbool.h>
#include <time.h>

#include "affect.h"
#include "drug.h"
#include "thing.h"

struct obj; /* forward decl only -- avoids a being.h<->obj.h include cycle,
               same idiom thing.h uses for `struct room` */

/* C replacement for the TBeing/TPerson slice of misc/being.h needed so far.
 * TBeing's ~148 virtuals mostly turned out to diverge between PC/mob on only
 * a handful of methods (hitGain, manaGain, wizFileSave, ...) -- not needed
 * yet since there's no full combat/regen system, so no dispatch table here
 * yet. Add one (see obj_vtables.c pattern used for the future obj/ port)
 * when that behavior actually gets ported.
 *
 * struct descriptor (net/descriptor.h) owns the list of connected
 * descriptors used by cmd_who/broadcast -- being_t doesn't duplicate that.
 *
 * Attributes are a simplified 6-stat set (not the original's 12-stat
 * STR/BRA/CON/DEX/AGI/INT/WIS/FOC/PER/CHA/KAR/SPE system in misc/stats.h),
 * chosen via point-buy at character creation and persisted in the new
 * player_attrs table (db/sneezy/player_attrs.sql -- the original doesn't
 * persist attributes in the DB at all, see STATUS.md).
 *
 * Point-buy is a true trade: every attribute starts at ATTR_BASE, and you
 * can raise or lower any attribute by up to ATTR_DELTA_CAP in either
 * direction. Lowering one attribute frees up room to raise another --
 * see attrs_allocated()/the sum<=ATTR_POOL check in descriptor.c. */

#define ATTR_BASE 120      /* every attribute starts here */
#define ATTR_POOL 30       /* net points spendable across all 6 attributes */
#define ATTR_DELTA_CAP 30  /* max amount any single attribute can be raised or lowered from base */
#define ATTR_MAX  250      /* absolute per-attribute ceiling, defense-in-depth beyond the delta cap */

#define BEING_TITLE_LEN 80 /* matches player.title varchar(80) */
#define GROUP_MAX_FOLLOWERS 6 /* see the `master`/`followers`/`grouped` fields below */
#define BEING_BAMF_LEN 96  /* matches player.bamfin/bamfout varchar(96) */

typedef struct {
    int strength;
    int dexterity;
    int constitution;
    int intelligence;
    int wisdom;
    int charisma;
} attrs_t;

/* Maps an attribute token ("str"/"strength", "dex"/"dexterity", ...,
 * case-insensitive) to its field in *a, or NULL if `tok` isn't one.
 * Shared by the character-creation attr screen, `edplayer`, and `set`
 * (descriptor.c / cmd_set.c) so the name list lives in exactly one place. */
int *attrs_field(attrs_t *a, const char *tok);

/* Levels: 50 mortal + 10 immortal, directly mirroring the original's
 * MAX_MORT=50 / GOD_LEVEL1=51 / MAX_IMMORT=60 (misc/defs.h). Unlike the
 * original (which is per-class, tied to a 9-class multiclass system Tobin
 * doesn't have), this is a single unified level -- no classes yet. Also
 * unlike the original (PLR_IMMORTAL flag + level check), immortal status
 * here is purely level-gated: reaching 51 alone grants it, since there's
 * no staff-promotion workflow to build yet. See STATUS.md for what that
 * means for testing (immortal status is currently unreachable through
 * normal play, since XP gain is capped at level 50). */
#define MORTAL_LEVEL_MIN   1
#define MORTAL_LEVEL_MAX   50
#define IMMORTAL_LEVEL_MIN 51
#define IMMORTAL_LEVEL_MAX 60

typedef struct {
    int level;
    long experience;
    int hp;
    int max_hp;
    /* Mortal/immortal toggle (Session 21): while an immortal plays as a
     * mortal, their real rank is parked here (0 = not suspended). Every
     * level check in the game reads `level`, so mortality is total; only
     * cmd_immort consults true_level -- the one deliberate exception to
     * "commands above your level are invisible". */
    int true_level;
    /* Good/evil axis only (Session 43 continued, user: "class
     * Mobile_Attitude in sneezy should be implemented into tobin. mobs
     * should react to good vs evil and react accordingly") -- the
     * original's Mobile_Attitude/opinionData is a much larger system
     * (suspicion/greed/malice/anger, hate/fear lists, faction/hunting --
     * see sneezymud-master/docs/systems/critical/
     * 14-monster-ai-behavior.md) that needs per-mob emotional state Tobin
     * has no infrastructure for yet. Scoped down to just the identified
     * prerequisite (no alignment stat existed at all) plus the single
     * reaction the user actually described: -1000 (evil) .. +1000 (good),
     * 0 (neutral, the default). See mob_ai.c for how ACT_AGGRESSIVE mobs
     * read this. Not carried by mobs themselves (no mob.alignment column
     * exists upstream) -- only a PC's own alignment is modeled. */
    int alignment;
    /* Discipline percentages (user 2026-07-12: "add the practice command
     * so players have to visit a guildmaster to gain skills based upon
     * percentage of discipline learned. cant get to advanced disc until
     * basic disc is at least 95% complete") -- a single 0-100 aggregate
     * per tier, NOT per-skill, raised via `practice` (cmd_practice.c) at
     * a guildmaster mob of the player's own class. 0 = none of that
     * tier's skills/spells are usable yet no matter what level says;
     * SKILL_TIER_ADVANCED additionally requires basic_disc_pct >= 95.
     * See skill.h's skill_tier_t. */
    int basic_disc_pct;
    int advanced_disc_pct;
    /* Combat discipline (SKILL_TIER_COMBAT) + the spendable practice-point
     * pool -- the 2026-07-13 practice redesign. Practice points are earned
     * on level-up (combat.c's combat_defeat()) and spent at a guildmaster
     * via `practice <discipline> [<#>]` to raise Basic/Combat/Advanced. A
     * point buys a random 1-2%. Advanced is gated on Basic AND Combat both
     * at 100 (see cmd_practice.c / skill.c). */
    int combat_disc_pct;
    int practice_points;
    /* Unix timestamp of the last `rent` (cmd_rent.c, user 2026-07-12:
     * "make rent work from sneezy"), or 0 if not currently rented out.
     * player_load() (player_repo.c) heals a flat rate for the real time
     * elapsed since this was set, then clears it -- "regenerating HP
     * while rented out" per Sneezy's own `rent` help text, without a
     * standing background job for offline characters. */
    long rented_at;
    /* Wallet gold (Money system, user 2026-07-17: "implement money and
     * shops"). GOLD-COIN-ONLY -- a plain int stat, not a pickupable
     * object, same shape as practice_points. Mobs hand it to their killer
     * directly on defeat (combat.c's combat_defeat()); shops (cmd_shop.c,
     * shop_repo.h) are the spending sink. */
    int gold;
    /* Bank balance (Money system v2, Sneezy → Tobin feature audit,
     * "Money system v2 (banking/taxes)"). Separate from `gold` (the
     * carried wallet, which mobs hand over and shops spend) -- a single
     * global bank rather than the original's per-shop accounts +
     * fractional-reserve central bank (Tobin's population doesn't
     * justify that scaffolding). Deposit/withdraw at the real seeded
     * "Grimhaven First Kingdom Bank" (shop_nr 4, room 31750) via
     * cmd_bank.c. Earns interest once per in-game day
     * (bank_interest_tick(), bank.c) -- a single SQL UPDATE across every
     * player row, not a per-online-character loop, so offline balances
     * grow too, same as the original's daily interest job. */
    int bank_gold;
    /* Vital statistics (Sneezy → Tobin feature audit, "Vital statistics
     * (hunger/thirst/age)"). Rescaled from the original's 0-24 condTypeT
     * range (sneezymud-master/docs/systems/informational/
     * vital-statistics.md) to a plainer 0-100 "percent full/hydrated" --
     * easier to reason about for both players (`score`) and content
     * (FOOD/DRINK val[0], obj.h) than the original's cryptic units.
     * -1 = immune, same meaning and same reason as the original's own
     * setCond() auto-immunity above MAX_MORT: an immortal's hunger/thirst
     * is never drained and never displayed as a real number
     * (vitals_tick_run(), vitals.c, skips them outright; being_is_immortal()
     * gate). Starvation/dehydration (hunger or thirst at 0) costs 1 HP per
     * drain tick, same "never below 1 HP outside real combat" floor
     * cmd_sip.c's poison already established -- lethal starvation is
     * explicitly deferred to whenever the still-open "Death processing"
     * audit item builds a real non-combat death path, not invented here. */
    int hunger;
    int thirst;
    /* Age (same audit item). User 2026-07-19 (AskUserQuestion): track +
     * display only, NOT the original's full graf()-interpolated age-based
     * stat-curve system (6 stats, human-equivalent age conversion, opt-in
     * quest bit, vampire exemption) -- real machinery for a mostly-
     * cosmetic payoff on a small MUD. Unix timestamp of character
     * creation, set once in being_create_pc() and never touched again;
     * `score` (cmd_score.c) shows age as real elapsed time since this
     * moment. Deliberately real-time, not a fictional MUD calendar --
     * Tobin has no calendar system to hang a "MUD year" conversion off of
     * (see gametime.c), and real elapsed play time is honest and needs no
     * new unit. */
    long birth_time;
    /* Vitality (Sneezy → Tobin feature audit, "Vitality stat + Terrain
     * movement cost"). The original has no such stat either -- its
     * "moves" resource (misc/limits.cc's getMaxMove()/moveGain()) is
     * CON-derived but otherwise unnamed; Vitality is a Tobin-original
     * name for the same role, closing the "Depends on Vitality" TODO.md
     * fragment this item's own audit note pointed at. An HP-parallel
     * resource -- max computed by being_calc_max_vit(), regenerates on
     * the same tick and position-weighting as HP (regen.c) -- spent by
     * `north`/`east`/etc (cmd_move.c) at a cost set by the sector being
     * entered (see room.h's sector_move_cost()). Movement is refused,
     * not merely slowed, once vit can't cover the cost -- same "hard
     * gate, not a soft penalty" shape hunger/thirst-at-0 uses for HP
     * loss. Immortals are exempt (being_is_immortal()), same reasoning
     * as hunger/thirst immunity above. */
    int vit;
    int max_vit;
} progress_t;

/* Prompt customization bits (player.prompt_flags, cmd_prompt.c; rendered
 * by the game loop's prompter). Mana/piety (user's original "expand
 * prompt toggles" ask) stay blocked on those stats not existing at all
 * yet -- prayer/casting is component-consumption-based, see
 * cmd_cast.c/cmd_pray.c. Gold unblocked 2026-07-18 once the Money system
 * shipped (progress_t.gold, above); vitality unblocked once the
 * Vitality stat itself shipped (progress_t.vit, above). */
#define PROMPT_FLAG_HP 1
#define PROMPT_FLAG_GOLD 2
#define PROMPT_FLAG_VIT 4
/* PROMPT_FLAG_EXP/EXPNEED (user 2026-07-19: expand prompt toggles to
 * cover experience/experience-needed-to-level) -- mana/piety stay
 * blocked, those resources still don't exist at all. ExpNeed reuses
 * progress_xp_for_level() (being.c), the same curve `level` (cmd_level.c)
 * shows -- clamped at 0 rather than negative once a mortal is already at
 * MORTAL_LEVEL_MAX or is an immortal, same "nothing more to grind
 * toward" convention. */
#define PROMPT_FLAG_EXP 8
#define PROMPT_FLAG_EXPNEED 16

/* Player flag bits (player.pflags). PLR_NEWBIE = on the newbie help channel
 * (default on; toggle off with `toggle newbie`). PLR_NOSHOUT = opted out of
 * hearing `shout`s (default off; toggle on with `toggle noshout`) -- an
 * immortal's shout still gets through regardless, matching the original's
 * sendShout() rule (misc/talk.cc). PLR_NOSPAM = suppresses "you/they miss"
 * combat messages on this player's own screen (default off; toggle on with
 * `toggle nospam`) -- ported from Sneezy's AUTO_NOSPAM (toggle.h/combat.cc),
 * where it's checked independently per viewer (attacker/defender/bystander)
 * rather than as a single global switch; see combat.c's combat_strike().
 * More flags join here. */
#define PLR_NEWBIE 1
#define PLR_NOSHOUT 2
#define PLR_NOSPAM 4
/* PLR_AUTOLOOT = automatically loots everything from an opponent's corpse
 * on defeat (default off; toggle on with `toggle autoloot`), user
 * 2026-07-12: "an autoloot toggle where a player upon opponent death
 * automatically loots all from the corpse" -- checked in combat.c's
 * combat_defeat() right after the corpse is populated. */
#define PLR_AUTOLOOT 8
/* PLR_PK_OPTIN = willing to fight other players (default off; toggle on
 * with `toggle pk`) -- BOTH the attacker AND the defender need this set
 * for `attack`/`kill`/`hit` to reach a PC target at all; enforced in
 * combat.c's combat_find_room_target() rather than at the command layer,
 * so every attack-style command gets the gate for free. Mob targeting is
 * completely unaffected -- this only gates PC-vs-PC. */
#define PLR_PK_OPTIN 16
/* PLR_NOTIPS = opted out of the periodic pulse-driven tip echo
 * (tips_repo.c's tips_pulse_tick(), default off; toggle on with `toggle
 * tips`) -- deliberately its OWN bit rather than reusing PLR_NEWBIE, user
 * 2026-07-19: "tips channel should be a toggle to shut it off or turn it
 * on again". Before this, the only way to silence tips was `toggle
 * newbie`, which also drops you off the newbie help channel entirely
 * (cmd_newbie.c) -- a much bigger side effect than "stop showing me
 * tips". tips_pulse_tick() now requires PLR_NEWBIE set AND PLR_NOTIPS
 * clear, so tips still only ever reach newbie-flagged connections. */
#define PLR_NOTIPS 32

/* Per-limb hit points. A simplified stand-in for the original's real
 * per-slot damage system (`bodyPartsDamage body_parts[MAX_WEAR]` in
 * misc/being.h, driven by `wearSlotT`'s 13 equipment-aligned slots plus
 * race-specific `slotChance()` weighting -- see misc/limbs.{h,cc}). As of
 * the 13-limb set below it's a near 1:1 match of the original's actual
 * slot list (head/neck/two arms/two fingers/body/waist/genitalia/two
 * legs/two feet -- "finger" here in place of the original's "hand", no
 * separate "back" slot), and combat.c's pick_weighted_limb() (user
 * 2026-07-12) now mirrors slotChance()'s own humanoid proportions too --
 * no per-race variation yet (Tobin has no race-specific body types), but
 * the per-limb weighting itself is a real port, not a Tobin invention.
 * Not persisted (like `fighting`/`desc`) -- see STATUS.md: this follows
 * the same already-accepted precedent as `progress.hp` (only saved at
 * combat defeat, not after every exchange). */
typedef enum {
    LIMB_HEAD,
    LIMB_NECK,
    LIMB_LEFT_ARM,
    LIMB_RIGHT_ARM,
    LIMB_LEFT_FINGER,
    LIMB_RIGHT_FINGER,
    LIMB_BODY,
    LIMB_WAIST,
    LIMB_GENITALIA,
    LIMB_RIGHT_LEG,
    LIMB_LEFT_LEG,
    LIMB_LEFT_FOOT,
    LIMB_RIGHT_FOOT,
    LIMB_COUNT
} limb_t;

typedef struct {
    int hp;
    int max_hp;
} limb_state_t;

/* Display name for a limb (e.g. "left arm"), used in combat messages and
 * score's limb breakdown. */
const char *limb_name(limb_t limb);

/* Descriptive sentence fragment completing "Your <limb> ___" / "<Name>'s
 * <limb> ___" for a limb at `pct` percent health -- "is hurt rather badly"
 * (< 20%), "needs medical attention" (< 10%), "is destroyed and needs
 * medical attention" (0%). Returns NULL for a healthy limb (>= 20%), so
 * callers only print a line/message when this is non-NULL. Used by both
 * `score` (cmd_score.c) and combat's tier-crossing announcements
 * (combat.c), so the wording is identical wherever it shows up. */
const char *limb_status_text(int pct);

/* Body position (original positionTypeT, misc/being.h). Players control the
 * standing/sitting/resting/sleeping rungs via sit/stand/rest/sleep/wake; the
 * lower rungs (dead/incap/...) and mounted/flying are reserved for future
 * use. "Fighting" is not stored -- it is derived from the `fighting` pointer
 * so combat never has to touch this field. */
typedef enum {
    POSITION_DEAD,
    POSITION_MORTALLYW,
    POSITION_INCAP,
    POSITION_STUNNED,
    POSITION_SLEEPING,
    POSITION_RESTING,
    POSITION_SITTING,
    POSITION_ENGAGED,
    POSITION_FIGHTING,
    POSITION_CRAWLING,
    POSITION_STANDING,
    POSITION_MOUNTED,
    POSITION_FLYING
} position_t;

/* Display name ("Standing", "Sleeping", ...) for a position. */
const char *position_name(position_t p);

/* Reverse of position_name() -- case-insensitive prefix match (e.g. "stand"
 * -> POSITION_STANDING). Used by `edsocial`'s min-position field, where a
 * builder types a name rather than memorizing the raw enum ordinal. False
 * (leaves *out untouched) if no position name starts with `name`, or if
 * the prefix is ambiguous between two different positions. */
bool position_from_name(const char *name, position_t *out);

/* Gender (original sexTypeT, misc/being.h -- SEX_NEUTER/SEX_MALE/SEX_FEMALE).
 * Chosen at character creation; drives pronoun selection. Persisted in
 * player.gender as 0/1/2 so the enum values must stay stable. */
typedef enum {
    GENDER_NEUTER = 0,
    GENDER_MALE   = 1,
    GENDER_FEMALE = 2
} gender_t;

/* Shared by two different DB columns: player.appearance (varchar(255)) and
 * mob.description (mediumtext, real seeded content runs up to ~1200 chars
 * -- e.g. vnum 33271's ogre bio). Used to be sized to the PC column alone
 * (256), which silently truncated every mob description mid-sentence on
 * load (mob_repo.c's snprintf into this buffer) -- bug found Session 43
 * continued (user: "increase the buffer size so i can read the entire
 * string"). Bumped with headroom above the real mob max; PC-authored
 * appearance text is unaffected (still saved as-is, and MariaDB itself
 * truncates on the rare INSERT/UPDATE that exceeds the real 255-char
 * column, same as any other varchar overflow). */
#define BEING_APPEARANCE_LEN 2048

/* Display name ("male"/"female"/"neuter") for a gender. */
const char *gender_name(gender_t g);

/* Pronouns for a gender, mirroring the original's HSHR/HMHR/HESH helpers:
 *   gender_subject   -> he / she / it       (subject:    "he smiles")
 *   gender_object    -> him / her / it      (object:     "you hit him")
 *   gender_possess   -> his / her / its     (possessive: "his sword")
 *   gender_reflexive -> himself/herself/itself (reflexive: "pokes himself") */
const char *gender_subject(gender_t g);
const char *gender_object(gender_t g);
const char *gender_possess(gender_t g);
const char *gender_reflexive(gender_t g);

/* Player classes (user 2026-07-11: "implement classes, 6 player classes:
 * mage, cleric, warrior, thief, druid, monk. The rest of the sneezy
 * classes are for mobs only"). Chosen at character creation, persisted in
 * player.class as 0-5 so the enum values must stay stable. SneezyMUD's
 * class system is a full multiclass bitmask (misc/defs.h's CLASS_*) plus
 * per-class discipline/skill trees Tobin has none of yet (TODO.md: "Ignore
 * DISC_* for now") -- this is single-class-only and affects just stat
 * bonuses/HP scaling, not a skill system. Druid has no SneezyMUD
 * equivalent (upstream's nature-caster analog is Shaman, not ported) --
 * new design, not a port. */
typedef enum {
    CLASS_MAGE = 0,
    CLASS_CLERIC,
    CLASS_WARRIOR,
    CLASS_THIEF,
    CLASS_DRUID,
    CLASS_MONK,
    CLASS_COUNT
} player_class_t;

/* Display name ("Mage", "Cleric", ...), capitalized -- score/who. */
const char *class_name(player_class_t c);

/* Readable label for a MOB's raw `mob.class` column (user 2026-07-12's
 * `stat` command: "class should report class text, not class number") --
 * NOT the same encoding as a PC's `player.class` column above (this one
 * is a bitmask: 1=mage, 2=cleric, 4=warrior, 8=thief, 64=monk, 128=druid,
 * see mob_class_mask_to_tobin(), being.c), so it needs its own decoder
 * rather than reusing class_name(). Returns "none" for mask 0, or
 * "unmapped (mask N)" for any other value Tobin doesn't track a class
 * for. */
const char *mob_class_label(int mask, char *buf, size_t bufsz);

/* Readable name for a MOB's raw `mob.race` column (user 2026-07-12's
 * `stat` command: "same for race") -- the FULL original monster-race
 * table (misc/race.cc's RaceNames[], 127 entries, "RACE_" prefix
 * stripped), completely separate from player_race_t/race_name() below
 * (Tobin's own 6-entry PLAYER race set) -- "no race applies to mobs"
 * mechanically (balance.c), but the raw seeded value is still real,
 * genuinely informative monster-race data worth decoding for `stat`. */
const char *mob_race_name(int idx);

/* True iff `idx` (a raw mob.race value, MOB_RACE_NAMES[] index) is a
 * mundane real-world creature race (RODENT, FELINE, CANINE, BEAR, DEER,
 * BIRD, FISH, SNAKE, INSECT, ...) rather than a fantastical/sapient one
 * (DRAGON, ORC, GOBLIN, UNDEAD, DEMON, ...). User 2026-07-19: "animal
 * races should not have wealth, that doesnt make sense" -- gates the
 * mob gold-drop-on-kill in combat.c's combat_defeat(). An ordinary
 * animal plausibly carries no coin purse; a dragon or goblin plausibly
 * does. */
bool mob_race_is_animal(int idx);

/* True iff `idx` (a raw mob.race value, MOB_RACE_NAMES[] index) is a
 * rideable-mount race for the Mount/riding system (cmd_ride.c). HORSE
 * only for v1 -- Sneezy's own real riding-skill categories (domestic/
 * nondomestic/winged/exotic, misc/riding.cc) cover far more ground
 * (camels, griffons, dragons, ...), but Tobin's seeded mob data only
 * has real HORSE-race mobs to draw on right now (verified against the
 * live `mob` table: several "horse"/"warhorse"/"plow-horse" entries,
 * all race=47/HORSE); other races can join this list later without
 * touching any caller. */
bool mob_race_is_rideable(int idx);

/* Applies `c`'s fixed stat bonus/penalty to `*a` IN PLACE (added on top of
 * whatever the player already point-bought) -- called once, at character
 * creation. Every class's bonuses and penalties net to zero. Loosely
 * mirrors the RELATIVE shape of the user's pre-approved stat-affinity spec
 * (TODO.md): mage high INT/low STR, warrior high CON+STR/dump CHA+WIS,
 * thief high DEX/low STR, cleric high WIS/low STR+DEX, monk STR+CON/low
 * CHA; druid (new) high WIS+CON/low INT. */
void class_stat_bonus(player_class_t c, attrs_t *a);

/* Maps the upstream `mob.class` bitmask to a Tobin player_class_t -- only
 * the single-class bits that have a real Tobin equivalent (user
 * 2026-07-12's practice/guildmaster request). Shaman(16)/deikhan(32)/
 * other(256) have no Tobin class and are left unmapped (returns false);
 * ranger(128) maps to Druid, matching the Druid roster's own Ranger-skill
 * lineage. Exposed (not static to being.c) so medit's characteristics
 * auto-calculation (descriptor.c, 2026-07-25) can reuse the exact same
 * mapping being_create_mob() itself uses, rather than a second guess at
 * it. */
bool mob_class_mask_to_tobin(int mask, player_class_t *out);

/* Player races (user 2026-07-11: "implement races, 6 player races: human,
 * elf, ogre, dwarf, hobbit, gnome. The rest of the sneezy races are for
 * mobs only"). Chosen at character creation, persisted in player.race as
 * 0-5. New design (Tobin has no race stat-modifier system to port --
 * SneezyMUD's race table wasn't found carrying attribute bonuses either,
 * see TODO.md), following the same "one dominant trait, net-zero bonus/
 * penalty" shape as classes above. Human is the deliberate baseline: no
 * modifier at all, matching the classic MUD "versatile, unremarkable"
 * convention. */
typedef enum {
    RACE_HUMAN = 0,
    RACE_ELF,
    RACE_OGRE,
    RACE_DWARF,
    RACE_HOBBIT,
    RACE_GNOME,
    RACE_COUNT
} player_race_t;

/* Display name ("Human", "Elf", ...), capitalized -- score/who. */
const char *race_name(player_race_t r);

/* Applies `r`'s fixed stat bonus/penalty to `*a` IN PLACE, same convention
 * as class_stat_bonus() -- called once at creation, alongside (not instead
 * of) the class bonus. */
void race_stat_bonus(player_race_t r, attrs_t *a);

typedef struct being {
    thing_t base;        /* first member -- see thing.h */
    long account_id;
    long player_id;
    int mob_actions;     /* THING_MOB only, 0 for a PC: mob.actions bitmask
                          * (ACT_* in mob_ai.c), copied verbatim from the
                          * prototype at spawn time (mob_repo.h). */
    /* THING_MOB only, 0 for a PC: mob.align (new column, mob_repo.h),
     * copied at spawn time same as mob_actions. -1 evil, 0 unaligned
     * (existing behavior, untouched), 1 good -- see mob_ai.c's
     * mob_try_aggress() for how an ACT_AGGRESSIVE mob's alignment changes
     * who it attacks/taunts/supports (user 2026-07-11: "good will attack
     * evil and evil will attack good randomly ... people who are neutral
     * should be taunted by evil and supported by good"). */
    int mob_align;
    /* THING_MOB only, 0 for a PC: mob.spec_proc, copied at spawn time
     * same as mob_actions/mob_align (mob_repo.h) -- a pure DATA marker
     * (Tobin has no spec-proc EXECUTION engine; triggers.c replaced that
     * concept), read by mob_ai.c's lamplighter behavior to recognize a
     * real seeded lamp-lighting mob (SPEC_PROC_LAMPLIGHTER) without a
     * per-tick DB round trip, same precedent as shop_repo_is_hospital()'s
     * SPEC_PROC_DOCTOR check (just cached instead of looked up live,
     * since this one runs every mob every AI tick rather than only when
     * a player interacts with a shop). */
    int mob_spec_proc;
    /* THING_MOB only, 0 (NORACE) for a PC: mob.race, copied at spawn
     * time same as mob_actions/mob_align/mob_spec_proc (mob_repo.h) --
     * the raw index into MOB_RACE_NAMES[]/mob_race_name() (being.c),
     * previously only read directly from the DB for `stat`. Now also
     * mechanically relevant: mob_race_is_animal() (being.c) gates the
     * gold-drop-on-kill in combat.c's combat_defeat() (user 2026-07-19:
     * "animal races should not have wealth, that doesnt make sense"). */
    int mob_race;
    attrs_t attrs;
    progress_t progress;
    limb_state_t limbs[LIMB_COUNT];

    /* Handedness (Session 21): 1 = right (default), 0 = left. The primary
     * hand hits harder, the off-hand weaker; strikes alternate hands via
     * the transient off_hand_next (not persisted, like fighting). */
    int handed_right;
    bool off_hand_next;

    /* Prompt customization bitmask (PROMPT_FLAG_*), player.prompt_flags. */
    int prompt_flags;

    /* Player flags bitmask (PLR_*), player.pflags. */
    int pflags;

    /* Player-settable title shown after the name in who (player.title). Empty
     * = no title. Set via the `title` command (cmd_title.c). The original's
     * title is a free-text descriptor ("the Brave", "floats here bleeding");
     * Tobin keeps the same free-form model, length-capped to the DB column. */
    char title[BEING_TITLE_LEN];

    /* Gender (player.gender) -- chosen at creation, drives pronouns. */
    gender_t gender;

    /* Class/race (player.class/player.race) -- chosen at creation, stat
     * bonuses already folded into `attrs` by then (class_stat_bonus()/
     * race_stat_bonus()). Meaningless for most mobs (char_class defaults
     * to CLASS_MAGE/0 since mobs have no race at all) -- EXCEPT a
     * guildmaster mob (user 2026-07-12, cmd_practice.c), whose char_class
     * is loaded from the upstream mob.class bitmask (mob_repo.c) to say
     * which class it trains; mob_class_known distinguishes "really a
     * Mage guildmaster" from "just an ordinary mob defaulting to 0". */
    player_class_t char_class;
    player_race_t race;
    bool mob_class_known;

    /* Free-text self-description (player.appearance), set at creation and
     * shown by `look <player>`/`score`. Empty = none set. */
    char appearance[BEING_APPEARANCE_LEN];

    /* Custom WALKING move messages, immortal-only (player.poofin/poofout;
     * named "bamfin"/"bamfout" until user 2026-07-11: "bamfin|out should
     * modify goto messaging and the current bamfin|out should be called
     * something else following the in|out syntax" freed that name up for
     * `goto`, below). Empty = use the default "exits to the <dir>"/"has
     * arrived" wording (cmd_move.c). May contain the tokens `$d` (direction
     * word) and `$p` (gender_possess() pronoun) -- e.g. "drags $p cross in
     * from the $d". Set via the `poofin`/`poofout` commands (cmd_poof.c). */
    char poofin[BEING_BAMF_LEN];
    char poofout[BEING_BAMF_LEN];

    /* Custom TELEPORT (`goto`) messages, immortal-only (player.bamfin/
     * bamfout -- see the poofin/poofout comment above for why this name
     * moved here). Empty = the default "$name disappears/appears in a puff
     * of smoke." wording (cmd_goto.c). May contain `$p` (gender_possess()
     * pronoun) -- there's no `$d` equivalent, `goto` has no direction. Set
     * via the `bamfin`/`bamfout` commands (cmd_bamf.c). */
    char bamfin[BEING_BAMF_LEN];
    char bamfout[BEING_BAMF_LEN];

    /* Body position (sit/stand/rest/sleep/wake). Default STANDING; not
     * persisted -- you wake up standing on login. "Fighting" is derived from
     * `fighting`, never stored here. */
    position_t position;

    /* Combat (PvP only for now, see STATUS.md -- no NPCs/mobs exist yet).
     * `fighting` is a live in-memory pointer, never persisted (meaningless
     * across a reconnect, same as `desc`). `last_combat_pulse` prevents a
     * round from being resolved twice when both fighters are iterated. */
    struct being *fighting;
    long last_combat_pulse;

    /* Group/party (Sneezy → Tobin feature audit, "Group / party system").
     * Live in-memory only, same "meaningless across a reconnect" rule as
     * `fighting` -- EXCEPT a disconnect deliberately does NOT clear these
     * (a linkdead body stays in memory, same as it already does for
     * everything else), so a group survives a member briefly dropping
     * link, same as the original. Scoped down from Sneezy's own two-tier
     * master/followers tree + a per-player configurable 1-10 money-share
     * factor + quest-flag/charm/mount interactions
     * (docs/systems/critical/09-group-party.md): XP shares here are
     * level-weighted (a simplification of the original's mob_exp()) and
     * gold splits EVENLY across present grouped members, not per-player-
     * configurable -- the real cooperative-play value without commands a
     * small-scale MUD doesn't need yet (`group share <player> <1-10>`).
     * Also, deliberately no leader-succession algorithm: if the leader
     * leaves/dies, the group simply dissolves (being_leave_group(),
     * being.c) rather than promoting a new leader -- a real simplification
     * from the original's two-pass reformGroup(), documented as a
     * conscious scope cut, not an oversight. */
    struct being *master;                        /* who I follow; NULL = leader or solo */
    bool grouped;                                 /* AFF_GROUP equivalent -- only a grouped
                                                      follower shares in XP/gold */
    struct being *followers[GROUP_MAX_FOLLOWERS]; /* this being's own followers, if a leader */

    /* Most recent `pray`/`cast` heal-type target + spell name (user
     * 2026-07-12: "add a continue command so clerics that heal <target>
     * can continue automatically until the target is fully healed or
     * their holy symbol breaks") -- live in-memory only, same "meaningless
     * across a reconnect" rule as `fighting`; cleared whenever a
     * non-heal spell is prayed/cast, whenever `continue` finishes (fully
     * healed / out of holy symbols / target left), and by being_destroy()
     * if the target itself goes away. `last_heal_spell` is a name, not a
     * skill_def_t* -- being.h can't depend on skill.h, and a name survives
     * the roster being rebuilt/reordered underneath it. See cmd_pray.c/
     * cmd_continue.c. */
    struct being *last_heal_target;
    char last_heal_spell[64];

    /* Mount/riding (Sneezy → Tobin feature audit, "Mount / riding
     * system"). Single pointer each way, same bidirectional-single-
     * pointer shape as `fighting` (not `master`/`followers[]`'s array
     * shape -- one rider per mount, one mount per rider, no multi-rider
     * chains). Live in-memory only, same reconnect rule as `fighting` --
     * survives a disconnect (a linkdead rider stays mounted, matching
     * `master`/`followers` not being cleared on disconnect either), but
     * is torn down bidirectionally by being_destroy() (cmd_ride.c/
     * being.c) whenever either side is actually destroyed (quit!,
     * death). See cmd_ride.c for mount/dismount, cmd_move.c for the
     * movement-cost discount + auto-dismount-indoors, combat.c for the
     * mounted attack bonus, and being_total_ac() for the mounted AC
     * bonus. */
    struct being *mount;
    struct being *rider;

    /* Active timed buffs/debuffs (user 2026-07-11's "Affects system
     * (buffs/debuffs/status)" backlog item) -- see affect.h. Live
     * in-memory only, same "meaningless across a reconnect" rule as
     * `fighting` (a disconnect ends any active buff, same as a real
     * MUD's session-scoped affects). */
    active_affect_t affects[MAX_ACTIVE_AFFECTS];

    /* Drug tracking (Sneezy -> Tobin feature audit) -- see drug.h.
     * first_use/last_use/total_consumed persist (player_drug_repo.h);
     * effect_ticks_left/applied[]/withdrawal_applied[] are in-memory
     * only, same "meaningless across a reconnect" precedent as
     * `affects[]` above. */
    drug_state_t drugs[DRUG_COUNT];

    /* Seed-farming `plant <seeds>` task-in-progress (Planting, Sneezy ->
     * Tobin feature audit) -- the dig-hole/sow-seeds/cover-hole 3-step
     * task from the original's task_plant(), scaled down to a simple
     * per-being countdown (no general task engine exists in Tobin yet;
     * see planting.c) rather than a dedicated task struct. `planting_seed`
     * is the SEED OBJECT being consumed (not just its vnum) so a step can
     * verify it's still actually there -- same "obj gone -> abort" safety
     * check task_plant() itself does. `planting_ticks_left` 0 = not
     * planting; 3/2/1 = dig hole / sow seeds / cover hole (planting.c's
     * planting_tick_run() counts it down and prints each step's message).
     * `planting_room` guards against the task surviving a room change
     * (task_plant()'s own `ch->in_room != ch->task->wasInRoom` check).
     * Live in-memory only, same reconnect rule as `fighting` -- a
     * disconnect simply abandons the task. */
    struct obj *planting_seed;
    int planting_ticks_left;
    int planting_type;
    struct room *planting_room;

    /* Pulses remaining before this character can act again (see pulse.h).
     * Mortals accumulate this from `being_set_wait()`; immortals always
     * read/write it as a no-op via being_get_wait()/being_set_wait(). */
    int wait_pulses;

    /* Per-immortal log-type opt-out bitmask (1 << log_type_t), gates
     * game_log()'s [TAG] echoes -- see cmd_setsev.c, a port of Sneezy's
     * `setsev` (misc/immortal.cc doSetsev()). Meaningless for mortals.
     * Deliberately NOT persisted (unlike the original's per-player `wizdata`
     * row) -- session-only, defaults to LOG_SEVERITY_DEFAULT (everything on)
     * at every login via being_create_pc(). Not worth a migration for a
     * niche admin display preference; revisit if that turns out wrong. */
    int severity;

    /* Objects (Phase 2C). Every object attached to this being (carried,
     * worn, or held) lives in the ONE thing_t containment chain
     * (base.stuff_head/stuff_next, parent == &this->base) -- these two
     * arrays are fast-lookup/display pointers INTO that same set, not
     * separate storage; `inventory` (cmd_object.c) walks stuff_head and
     * excludes anything also pointed to here, `equipment` reads these
     * arrays directly. `equipment` is indexed by limb_t -- no second slot
     * enum, reusing the existing 13-limb set (see obj.h's
     * wear_slot_for_flag()). `held` is the primary/off-hand wielded pair
     * (index 0/1), independent of limbs, respecting `handed_right`. NULL =
     * nothing there. Neither array is persisted directly -- see
     * obj_repo.h's player_inventory_save(), which derives the saved `slot`
     * column from these at save time. */
    struct obj *equipment[LIMB_COUNT];
    struct obj *held[2];

    struct descriptor *desc; /* back-pointer to the owning connection, NULL for mobs */
    time_t linkdead_since;   /* wall-clock time `desc` was cleared; 0 if never linkdead (see world.h's linkdead_purge_tick()) */
} being_t;

/* Creates an in-memory being_t for a PC. Does not touch the DB -- pair with
 * player_repo.h's player_create()/player_load() for persistence. */
being_t *being_create_pc(const char *name, long account_id, long player_id);

/* Creates an in-memory being_t for a mob instance, loading the prototype
 * row for `vnum` from the `mob` table (mob_repo.h's mob_proto_load()).
 * Returns NULL if no such vnum exists. Unlike a PC, `account_id`/
 * `player_id` stay 0 (never a real DB row) and `desc` stays NULL forever
 * -- a mob is just a being_t with kind=THING_MOB, see obj.h/STATUS.md's
 * Mobiles decision row for why no separate mob_t struct exists. Not
 * attached to any room/being yet -- caller does that via thing_move_to(). */
being_t *being_create_mob(int vnum);

void being_destroy(being_t *b);

/* True iff `a` and `b` are in the same group -- same identity, one is the
 * other's master, or they share a master (siblings) -- AND both have
 * `grouped` set. Mirrors Sneezy's own inGroup(): deliberately NOT
 * transitive (a follower's own follower is not automatically in the
 * top-level group), see docs/systems/critical/09-group-party.md. */
bool being_in_group(const being_t *a, const being_t *b);

/* Fills `out` (up to `max`) with every grouped member of `self`'s group --
 * the leader (self's master, or self if self has no master) plus every
 * grouped follower of that leader. Returns the count written; 0 if `self`
 * isn't grouped at all. Used by combat.c to split XP/gold on a kill. */
int being_group_members(const being_t *self, being_t **out, int max);

/* Cleanly detaches `b` from any group relationship before it goes away
 * (being_destroy()) or on request (`stop`, cmd_group.c): removes `b` from
 * its master's followers[] and clears `b`'s own master/grouped. If `b` is
 * ITSELF a leader with followers, the group DISSOLVES -- every follower's
 * master/grouped is cleared too (no leader-succession algorithm; see the
 * being_t field comment for why that's a deliberate scope cut). Safe to
 * call on a being with no group relationships at all (no-op). */
void being_leave_group(being_t *b);

/* Pet/charm (Sneezy → Tobin feature audit). The charmed pet mob currently
 * following `master`, or NULL if it has none -- scans master->followers[]
 * for one carrying AFFECT_CHARMED (see affect.h). Used to enforce a
 * one-pet-at-a-time cap (being_summon_charmed_pet() below refuses a
 * second summon while this returns non-NULL) and by mob_ai.c/combat.c/
 * cmd_move.c to find a master's own pet without a second parallel field. */
being_t *being_find_charmed_pet(const being_t *master);

/* Spawns a fresh mob from `vnum` (being_create_mob()), places it in
 * master's room, attaches it as a follower (master->followers[], same
 * slot mechanism `follow` uses -- see cmd_group.c), and marks it charmed
 * for `duration_rounds` (AFFECT_CHARMED, being_apply_affect()) so it
 * dissolves on its own when the affect runs out (affect.c's
 * tick_being_affects()). Returns NULL, doing nothing, if: `vnum` doesn't
 * exist, master's followers[] is full, or master already has a charmed
 * pet (being_find_charmed_pet() -- v1 scope is one pet at a time, not
 * Sneezy's own level-scaled multi-pet cap, tooManyFollowers()). Callers
 * (cmd_cast.c/cmd_pray.c's new "conjure elemental"/"summon swarm"/
 * "animal companion" spells) print their own flavor message; this only
 * does the mechanical summon. */
being_t *being_summon_charmed_pet(being_t *master, int vnum, int duration_rounds);

/* Transformation (Sneezy → Tobin feature audit, Mage "polymorph").
 * Spawns a fresh mob from `vnum` (being_create_mob()), places it in `d`'s
 * current room, and swaps `d`'s descriptor onto it -- the EXACT same
 * raw swap `possess`/`return` already use (cmd_possess.c), reusing
 * `d->possess_original` rather than a second parallel field: `d->
 * character` now points at the temporary form, and the player's real
 * body sits parked with desc==NULL (same shape a plain link-drop already
 * leaves a body in) -- STILL VISIBLE in the room, tagged "(linkdead)"
 * (cmd_look.c's existing convention), not hidden away. Real Sneezy
 * stashes the original body in a dedicated Room::POLY_STORAGE instead;
 * Tobin has no such "nowhere" concept, so this is a disclosed
 * simplification, not an oversight -- someone sharing a room with a
 * freshly-polymorphed player can still see their real name sitting
 * there, linkdead, right next to their new form. Marked AFFECT_POLYMORPH
 * for `duration_rounds` so it
 * reverts on its own (affect.c's tick_being_affects()) -- see also
 * combat.c's combat_defeat() and descriptor.c's descriptor_destroy(),
 * both of which revert IMMEDIATELY (death, disconnect) rather than
 * leaving a dangling swapped descriptor for the affect to eventually
 * clean up. Returns false, doing nothing, if `d` is already possessing/
 * polymorphed into something, has no character, or `vnum` doesn't
 * exist. Caller (cmd_cast.c's "polymorph") prints its own flavor
 * message; this only does the mechanical swap. */
bool being_start_polymorph(struct descriptor *d, int vnum, int duration_rounds);

/* True iff b->progress.level >= IMMORTAL_LEVEL_MIN. */
bool being_is_immortal(const being_t *b);

/* True iff b is carrying/wearing/holding at least one currently-lit
 * OBJ_CAT_LIGHT object (val[3], obj.h) anywhere in its stuff_head chain --
 * matches obj_light_burn_tick()'s own scope (obj.c), not just held[]/
 * equipment[], since a lit lamp works the same whether held or just
 * loose in a pack. Used by the "Weather & light levels" audit item's
 * darkness gate (cmd_look.c/cmd_exits.c) to decide whether a dark,
 * unlit room is still visible to this particular looker. */
bool being_has_active_light(const being_t *b);

/* True iff `r` is currently dark FOR `ch` specifically -- an immortal
 * always sees fine (same "commands above your level are invisible, but
 * darkness isn't a limitation" spirit as their other blanket exemptions);
 * anyone else needs the room to be lit (ROOM_FLAG_ALWAYS_LIT,
 * ROOM_FLAG_INDOORS, or plain daylight, gametime_is_daytime()) OR their
 * own being_has_active_light() to see. Shared by cmd_look.c's bare `look`
 * and cmd_exits.c -- gating only one of the two would let a player just
 * route around the darkness restriction with the other. */
bool room_is_dark_for(const struct room *r, const being_t *ch);

/* A word describing b's health as a fraction of max HP ("near death" ...
 * "perfect"), from the original's prompt_mesg[]. Shown in `score`. */
const char *being_health_word(const being_t *b);

/* A word for a progress_t.alignment value ("demonic".."saintly", "neutral"
 * at 0) -- see progress_t's doc comment (being.h) for the -1000..+1000
 * scale. Shown in `score`; also what mob_ai.c's ACT_AGGRESSIVE reaction
 * checks. */
const char *alignment_word(int alignment);

/* Words for progress_t.hunger/thirst (0-100, -1 = immortal-immune) -- same
 * bucketing style as being_health_word() above. Shown in `score`. */
const char *being_hunger_word(int hunger);
const char *being_thirst_word(int thirst);

/* b's limb HP as a 0-100 percentage of that limb's max_hp. Returns 0 for
 * an invalid being/limb. */
int being_limb_pct(const being_t *b, limb_t limb);

/* True iff any of b's limbs are at 0 HP -- combat.c uses this to apply a
 * placeholder hit-chance penalty to BOTH sides of a fight a destroyed limb
 * is involved in (worse at landing your own hits; easier for others to
 * land theirs). Two ways to clear it: get treated at a Hospital
 * (limb repair, see cmd_shop.c's spec_proc==48 doctor shops), or lose a
 * fight -- combat defeat already fully heals every limb as part of the
 * "revived at half HP" recovery (being_limbs_full_heal(), combat.c), same
 * as it always has; that's deliberate soft-respawn behavior, not a bug to
 * fix here. */
bool being_has_destroyed_limb(const being_t *b);

/* Total armor class across every worn slot (sums obj_armor_ac() over
 * equipment[LIMB_COUNT]) -- combat.c's combat_strike() subtracts a scaled
 * fraction of this from the attacker's hit roll. 0 for an unarmored
 * being. */
int being_total_ac(const being_t *b);

/* Renders a being's worn/held equipment as "  <label>: <value>\r\n" lines
 * (one per limb slot plus primary/secondary hold, skipping LIMB_GENITALIA
 * -- nothing is ever worn there) into `out`. Shared by `equipment`
 * (cmd_object.c, your own gear) and `look <person>` (cmd_look.c,
 * user 2026-07-12: "when you look at someone you should also see what
 * equipment thier wearing"). Does NOT write a leading header line --
 * callers add their own ("You are using:" vs "<Name> is wearing:"). */
void being_render_equipment(const being_t *b, char *out, size_t out_sz, size_t *n);

/* Correct MID-SENTENCE display text for a being: a PC's own base.name
 * (already properly cased at creation), or a MOB's short_descr (its own
 * lowercase "a lady"-style article+description) -- NOT the raw base.name
 * a mob actually carries, which is its space-separated KEYWORD list
 * ("lady stroll walk", matched by `look lady`/`look stroll`/`look walk`),
 * never meant for display. Same "raw keyword list used as a display
 * name" bug class fixed in mob_ai.c's mob_try_wander()/mob_try_scavenge()/
 * mob_try_aggress() -- combat.c's per-hit messages had the same bug
 * (found in the 2026-07-11 capitalization audit) and use this instead. */
const char *being_display_name(const being_t *b);

/* SENTENCE-INITIAL capitalized version of being_display_name() -- writes
 * into `buf` (size `bufsz`) and returns it. A PC's name passes through
 * unchanged (already capitalized); a MOB's short_descr is capitalized,
 * skipping a leading color tag first if present (same bug class as
 * cap_first() elsewhere). */
const char *being_display_name_cap(const being_t *b, char *buf, size_t bufsz);

/* Rank title for an immortal level (51-53 "Immortal", 54-57 "God", 58
 * "Greater God", 59 "Administrator", 60+ "Implementor"), or NULL for a
 * mortal level (< IMMORTAL_LEVEL_MIN) -- callers fall back to showing the
 * raw level number in that case. */
const char *being_level_title(int level);

/* Color tag for an immortal's rank tier, used to tint their name in who and
 * score: 51-53 <c>, 54-56 <C>, 57-58 <p>, 59+ <P>; "" for a mortal. Pair
 * with a "<z>" reset after the coloured text. */
const char *being_rank_color(int level);

/* Normalizes a character name to proper case in place (first letter
 * uppercase, rest lowercase) -- e.g. "TESTGUY" or "testguy" both become
 * "Testguy". Call once at character creation, before the name is stored,
 * so every later display (who, look, score, combat messages, the account
 * menu) already shows a consistently-cased name -- mirrors the original's
 * sstring::cap() applied in sys/create_character.cc. */
void being_normalize_name(char *name);

/* Pulses remaining before b can act again. Always 0 for immortals. */
int being_get_wait(const being_t *b);

/* Sets b's wait to `pulses`. No-op for immortals. */
void being_set_wait(being_t *b, int pulses);

/* Placeholder max-HP formula: 20 base + (constitution above ATTR_BASE) +
 * 5 per level. Not the original's class/level-table-driven formula (no
 * classes exist) -- revisit once a real growth curve is designed. */
int being_calc_max_hp(const being_t *b);

/* Placeholder max-Vitality formula: 50 base + (constitution above
 * ATTR_BASE) + 2 per level -- same "CON-and-level-driven resource" shape
 * as the original's getMaxMove() (misc/limits.cc: 100 + 15 + level +
 * plotStat(CON,...)), scaled down since Tobin's vit only pays for
 * per-room movement cost, not a whole combat-fatigue economy. Revisit
 * alongside being_calc_max_hp() if a real growth curve is designed. */
int being_calc_max_vit(const being_t *b);

/* (Re)sets every limb's max_hp from b->progress.max_hp (split evenly across
 * LIMB_COUNT, placeholder -- the original weights per-slot max via
 * hitLimit()/slotChance(), not replicated here) and heals every limb to
 * full. Called at character creation and whenever a defeated character is
 * patched up (combat_defeat()/combat_instakill() in combat.c). */
void being_limbs_full_heal(being_t *b);

/* Applies dmg to both b's overall HP and the given limb's HP (clamped at 0,
 * doesn't go negative -- unlike progress.hp, which combat already lets go
 * negative to detect defeat). */
void being_hurt_limb(being_t *b, limb_t limb, int dmg);

/* Heals b's overall HP and every limb's HP by `amount` (each clamped at its
 * own max) -- used by the regen tick (src/core/regen.c). No-op for
 * amount <= 0. */
void being_heal(being_t *b, int amount);

/* Heals b's Vitality by `amount` (clamped at max_vit). No-op for
 * amount <= 0. Used by the regen tick (src/core/regen.c), same
 * position-weighted cadence as being_heal(). */
void being_heal_vit(being_t *b, int amount);

/* Spends `amount` of b's Vitality (clamped at 0 -- never negative,
 * unlike progress.hp). Used by cmd_move.c to pay a sector's movement
 * cost. No-op for amount <= 0. */
void being_spend_vit(being_t *b, int amount);

/* Placeholder XP curve (level*level*100) -- the original's is a recursive
 * kill-count formula tied to mob levels, which don't exist in Tobin yet.
 * Total XP required to REACH `level` from level 1. */
long progress_xp_for_level(int level);

/* Applies xp_gain to *p, advancing level for every threshold crossed
 * (handles a single large gain crossing more than one level). Hard-capped
 * at MORTAL_LEVEL_MAX -- no accidental immortal promotion via grinding.
 * Returns the number of levels gained (0 if none). Caller is responsible
 * for notifying the player of any level-up(s). */
int progress_add_xp(progress_t *p, long xp_gain);

#endif
