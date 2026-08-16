/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
 * player_attrs table (db/tobin/player_attrs.sql -- the original doesn't
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
    /* Intoxication (`alcoholism` skill, missing-skill audit batch C,
     * 2026-08-09). Real upstream's DRUNK condTypeT (0-24, limits.cc's
     * gainCondition()) rescaled onto the same 0-100 "percent" scale
     * hunger/thirst already use here, same rationale (plainer to reason
     * about than the original's cryptic units). Gained by drinking an
     * alcoholic liquid (liquids.h's liquid_type_t.drunk, cmd_drink.c/
     * cmd_sip.c), dampened by the real SKILL_ALCOHOLISM formula
     * (being_gain_drunk(), vitals.c), and sobers up on its own over
     * time (vitals_tick_run()). No -1 immortal-immune sentinel like
     * hunger/thirst -- immortals are gated out at every call site
     * instead (being_is_immortal()), since an immortal never drinks
     * anything that would set it in the first place. */
    int drunk;
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
    /* Mana (user 2026-08-06: "implement it just like sneezy" -- real
     * SneezyMUD's own TPerson::manaLimit(): a Mage's pool is `100 +
     * getSkillValue(SKILL_MANA) * 3` (their own "mana" skill governs
     * its size, gained the same learn-by-doing way as any other skill
     * -- see skill.c's roster entry and cmd_cast.c), a
     * Ranger/Monk-lineage class gets `100 + level * 3` instead (no
     * comparable skill exists for them); Tobin folds Ranger into Druid,
     * so Druid uses that second formula (being_calc_max_mana(),
     * being.c) and Cleric/Warrior/Thief/Monk get none -- Cleric spends
     * a wholly separate resource (piety) in the original that Tobin
     * hasn't built, so `pray` still costs nothing numeric, unchanged.
     * Spent on `cast` per real spell_info.cc MANA_<n> values where one
     * exists (spell_mana.c), regenerates on the same tick/position
     * weighting as HP/Vitality (regen.c). */
    int mana;
    int max_mana;
    /* Piety: the Cleric's divine casting resource (Tobin's analogue
     * to a Mage's mana), a flat 0..100 pool spent by `pray` and
     * regenerated on the mana/piety tick -- being_calc_max_piety(),
     * being_piety_gain(). 0 for every non-Cleric. */
    int piety;
    int max_piety;
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
/* PROMPT_FLAG_MANA (user 2026-08-06, once the mana pool itself shipped --
 * see progress_t's own mana/max_mana comment): 0 for a non-caster, same
 * "just doesn't show" convention as any other stat a given character
 * doesn't have (e.g. Vitality already does this for nobody currently,
 * but mana is the first stat that's genuinely absent for MOST classes
 * by design, not just untested). */
#define PROMPT_FLAG_MANA 32

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
/* PLR_NOTELL = blocks incoming `tell`s (default off; toggle on with
 * `toggle notell`), ported from the original's AUTO_NOTELL -- EXCEPT from
 * whoever this player's OWN descriptor last told (desc->last_told,
 * descriptor.h), so a conversation you started yourself still gets a
 * reply through even with this on. Checked in cmd_tell.c/cmd_reply.c
 * before delivery, with an explicit failure message to the sender
 * (unlike the ignore list, which fails silently by design). */
#define PLR_NOTELL 64
/* PLR_AFK = opts in to auto-away behavior (default off; toggle on with
 * `toggle afk`), ported from the original's AUTO_AFK ("auto-AFK after
 * idle"). Doesn't mark you AFK by itself -- combined with the existing
 * idle-detection threshold (descriptor.c's `last_active`, same one `who`
 * already uses for its "(idle)" tag) in cmd_tell.c/cmd_reply.c: a tell to
 * an idle player with this set gets an extra "is AFK" notice appended for
 * the sender, tell still delivers normally either way. */
#define PLR_AFK 128
/* PLR_MUTED = an IMMORTAL-imposed ban on tell/shout/emote (ported from
 * the original's PLR_GODNOSHOUT), set/cleared only by `mute`/`unmute`
 * (58+, cmd_mute.c) -- unlike every other flag on this list, a player can
 * never set or clear this on themselves. Checked at the SENDER side of
 * each blocked command (cmd_tell.c, cmd_shout.c, the emote social) rather
 * than the recipient side, since this restricts what a muted player can
 * say, not what reaches them. */
#define PLR_MUTED 256

/* Per-limb hit points. A simplified stand-in for the original's real
 * per-slot damage system (`bodyPartsDamage body_parts[MAX_WEAR]` in
 * misc/being.h, driven by `wearSlotT`'s 22-slot layout plus race-specific
 * `slotChance()` weighting -- see misc/limbs.{h,cc}/body.cc). Reshaped
 * 2026-07-26 (user, "Limbs -> wearSlotT") toward that real slot list,
 * decisions confirmed with the user first (none defaulted silently):
 * BACK/WRIST_L/R/HAND_L/R added (real seeded `obj.wear_flag` bits for all
 * of these already existed, just unmapped -- see wear_slot_for_flag(),
 * obj.c); Tobin's own GENITALIA slot (no upstream equivalent) is KEPT;
 * `HOLD_RIGHT`/`HOLD_LEFT` from the original stay OUT of this enum --
 * Tobin's separate `held[2]` array below is unchanged, lower-risk than
 * folding holding into every equipment[] loop in the codebase; the
 * original's mob-only `WEAR_EX_*` extra-leg/-foot slots (four-legged/
 * multi-limbed bodies) are added at the END of the enum, ALWAYS INACTIVE
 * (max_hp=0, see being_limbs_full_heal()/being_has_limb()) until the
 * separate, not-yet-built Body types system actually assigns them to a
 * specific mob prototype -- LIMB_EX_RIGHT_LEG is also `LIMB_REAL_COUNT`,
 * the boundary between real/always-active slots and these placeholders.
 * combat.c's pick_weighted_limb() mirrors slotChance()'s real humanoid
 * proportions (body.cc's BODY_HUMANOID row) for every slot including the
 * new ones; EX_* slots get weight 0, so they're never hit under today's
 * humanoid-only combat regardless. Not persisted (like `fighting`/`desc`)
 * -- see STATUS.md: this follows the same already-accepted precedent as
 * `progress.hp` (only saved at combat defeat, not after every exchange). */
typedef enum {
    LIMB_HEAD,
    LIMB_NECK,
    LIMB_BACK,
    LIMB_LEFT_ARM,
    LIMB_RIGHT_ARM,
    LIMB_LEFT_WRIST,
    LIMB_RIGHT_WRIST,
    LIMB_LEFT_HAND,
    LIMB_RIGHT_HAND,
    LIMB_LEFT_FINGER,
    LIMB_RIGHT_FINGER,
    LIMB_BODY,
    LIMB_WAIST,
    LIMB_GENITALIA,
    LIMB_RIGHT_LEG,
    LIMB_LEFT_LEG,
    LIMB_LEFT_FOOT,
    LIMB_RIGHT_FOOT,
    LIMB_EX_RIGHT_LEG,  /* mob-only, inactive until Body types -- see above */
    LIMB_EX_LEFT_LEG,
    LIMB_EX_RIGHT_FOOT,
    LIMB_EX_LEFT_FOOT,
    LIMB_COUNT
} limb_t;

/* The first mob-only EX_* slot -- also the count of always-active,
 * humanoid-usable limb slots (everything before this one). */
#define LIMB_REAL_COUNT LIMB_EX_RIGHT_LEG

typedef struct {
    int hp;
    int max_hp;
    /* `bandage` (docs/Spell Assignments.xlsx gap audit, 2026-08-08):
     * set when this limb crosses into limb_status_text()'s bad tier
     * (combat.c, same tier-crossing guard the blood-pool spawn already
     * uses), cleared by a successful `bandage` or a full heal
     * (being_limbs_full_heal()). Deliberately transient -- not saved to
     * the DB, resets to false on login, same scope-down precedent as
     * `fighting`. */
    bool bleeding;
} limb_state_t;

/* True iff `b` actually has this limb slot (max_hp > 0) -- false for any
 * of the mob-only EX_* slots on every being today (nothing assigns them a
 * real max_hp until Body types exists). Guards every per-limb display/
 * status loop (cmd_limbs.c, cmd_score.c, being_render_equipment()) and
 * being_has_destroyed_limb() below so an always-inactive EX_* slot is
 * never mistaken for a permanently-destroyed real limb. */
bool being_has_limb(const struct being *b, limb_t limb);

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
    CLASS_COUNT,
    /* All-classes immortals (user, 2026-08-10): `promote` sets this on
     * crossing into immortal level range instead of leaving the
     * character's pre-promotion class stale. Declared AFTER CLASS_COUNT
     * and equal to it on purpose, so every existing `for (c = 0; c <
     * CLASS_COUNT; c++)` loop (class_balance, `skills`'s immortal
     * branch, etc.) already excludes it with no change needed -- it's
     * a real, storable player_class_t value, just never a real class's
     * own row in any per-class table. being_knows_skill() already
     * grants immortals every skill in every class regardless of this
     * field's value (it bypasses the class check entirely for any
     * immortal); this exists so score/who/practice describe that
     * honestly instead of showing a stale single class. */
    CLASS_ALL = CLASS_COUNT,
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

/* Monster-lore kingdoms -- the eight creature categories behind the Know-X
 * lore skills and the `know` command (mob_lore.c / cmd_know.c). Ported from
 * Sneezy's per-race `lore` keyword (Race::Kingdom). LORE_DEMON is Sneezy's
 * "diabolic". */
typedef enum {
    LORE_ANIMAL,
    LORE_VEGGIE,
    LORE_DEMON,
    LORE_REPTILE,
    LORE_UNDEAD,
    LORE_GIANT,
    LORE_PEOPLE,
    LORE_OTHER,
} mob_lore_t;

/* The lore kingdom of a MOB_RACE_NAMES[] index (exhaustive; out-of-range ->
 * LORE_OTHER). */
mob_lore_t mob_race_lore_category(int idx);

/* The roster skill name ("know animal" ...) and a short field phrase
 * ("animals" ...) for a lore category. */
const char *mob_lore_skill_name(mob_lore_t cat);
const char *mob_lore_field_name(mob_lore_t cat);

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
 * 0-5. Each race's net-zero attribute bonus is DERIVED from SneezyMUD's
 * own per-race stat table (sneezymud-master/lib/races/RACE_*): its 12
 * stats fold into Tobin's 6 (brawn->CON, agility+speed->DEX, focus split
 * INT/WIS, perception+karma->CHA), centered per race to net zero and
 * scaled 1 point per 10% -- see docs/RACE_STATS.md and race_stat_bonus().
 * (Corrects an earlier note here that wrongly said that table carried no
 * attribute data -- it does.) Human is the deliberate baseline: no
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

/* Territory/Homeland (Sneezy -> Tobin feature audit, `docs/systems/important/
 * territory-system.md`): a permanent "upbringing" chosen at creation, right
 * after race, on top of race's own bonus -- the real upstream's sub-race
 * layer, distinct from and additional to plain race. Real upstream gives
 * each of its 6 base races a DIFFERENT list of 4-8 homeland options (Urban,
 * Villager, Mountain, Forest, Recluse, ...) on its full 12-stat system, with
 * modifiers up to +/-30. Deliberately scoped down for Tobin's simpler
 * 6-attribute set (a Tobin-scale slice, same precedent as banking/crafting/
 * materials): a SINGLE shared 3-option set (Urban/Rural/Wilds) usable by
 * every race alike rather than 6 separate race-specific tables, at a
 * modifier scale (+/-3) proportioned to sit alongside race_stat_bonus()/
 * class_stat_bonus()'s own +/-2..4 range rather than dwarfing it. Purely
 * flavor + stats, same as race/class -- grants no skills. */
typedef enum {
    TERRITORY_URBAN = 0,  /* city-raised: sharp and sociable, softer and slower */
    TERRITORY_RURAL,       /* farm/village-raised: practical and hardy, less refined */
    TERRITORY_WILDS,       /* frontier-raised: tough and strong, blunt and unworldly */
    TERRITORY_COUNT
} player_territory_t;

/* No value in [0, TERRITORY_COUNT) means "no homeland" -- same concept as
 * real upstream's HOME_TER_NONE (0 there; here it's simply anything
 * outside the 3 real choices, -1 by convention). Territory IS a forced
 * creation step now (user, 2026-08-03: "should be a choice after choosing
 * race", same as race/class), so a real player character never actually
 * ends up with this value -- it exists as a safe init placeholder
 * (descriptor.c, before CONN_CHAR_CREATE_TERRITORY runs) and a defensive
 * fallback for any pre-existing DB row from before this column existed.
 * territory_stat_bonus() below already no-ops for any unrecognized value,
 * so TERRITORY_NONE needs no explicit enum member, just this convention. */
#define TERRITORY_NONE ((player_territory_t)(-1))

/* Display name ("Urban Dweller", ...), capitalized -- score/who. Returns
 * "(none)" for TERRITORY_NONE/any other out-of-range value. */
const char *territory_name(player_territory_t t);

/* Applies `t`'s fixed stat bonus/penalty to `*a` IN PLACE, same convention
 * and call site as race_stat_bonus()/class_stat_bonus() -- called once at
 * creation, alongside both. */
void territory_stat_bonus(player_territory_t t, attrs_t *a);

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
    /* body_type_t (body.h), stored as plain int here to avoid a circular
     * include (body.h itself needs limb_t, defined above in this same
     * file). 0 (BODY_NONE) for a PC -- behaves identically to
     * BODY_HUMANOID, since the original's own slot_chance[] row for
     * BODY_NONE is a verbatim duplicate of its BODY_HUMANOID row. For a
     * mob, copied from `mob.body_type` at spawn (mob_repo.h), same
     * precedent as mob_race/mob_align/mob_spec_proc -- see
     * being_limbs_full_heal()/combat.c's pick_weighted_limb() for where
     * this actually changes which limbs a being has and how likely each
     * is to be hit (Limbs -> wearSlotT's EX_* mob-only slots, 2026-07-26,
     * finally becoming reachable here). */
    int body_type;
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
    player_territory_t territory; /* player.territory -- see territory_stat_bonus() above.
                                    * Meaningless for mobs, same as race. */
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

    /* `sneak` (spell/skill functional-completeness audit, 2026-07-27,
     * Thief/Warrior roster entry "Move around without waking sleepers
     * or drawing attention", level 1). A live in-memory toggle, same
     * "meaningless across a reconnect" rule as `fighting` -- you don't
     * stay sneaking across a relog. Suppresses the normal room-wide
     * arrival/departure echo (cmd_move.c) while moving; broken outright
     * the moment you enter combat (cmd_attack.c/cmd_backstab.c clear it
     * when `fighting` gets set), same spirit as disguise refusing to
     * toggle while already fighting. Deliberately does NOT also hide you
     * from a room's person-listing while stationary -- that's `hide`'s
     * job (a separate, higher-level roster entry), not sneak's. */
    bool sneaking;

    /* Riposte (spell/skill functional-completeness audit continued,
     * level 20: skill.c's own Warrior roster "A successful parry gives
     * you a chance to counter-attack immediately."). Real upstream
     * (misc/combat.cc:4348) sets a transient AFF_RIPOSTE flag on a
     * successful parry (50% chance, gated on knowing SKILL_RIPOSTE and
     * its own skill roll), consumed on the SAME being's own next attack
     * this round to grant one bonus hit ("fx++" in hit()) -- ported the
     * same shape here: set on a successful parry in combat_strike()'s
     * parry branch, consumed at the top of the very next combat_strike()
     * call where this being is the ATTACKER (forces that swing to land
     * regardless of the normal to-hit roll, rather than a separate bonus
     * strike or extra damage -- the simplest faithful analog of "one
     * extra swing" inside Tobin's fixed one-strike-per-side-per-round
     * shape). Live in-memory only, meaningless across a reconnect, same
     * rule as sneaking above. */
    bool riposte_ready;

    /* XP gained so far in the CURRENT fight (user, 2026-08-03: "we want xp
     * gain calculated per hit, not at the end of a fight" -- so a mid-fight
     * disconnect/crash doesn't erase XP already earned, and a leveling
     * group member gets their max_hp/practice-point payoff as it happens,
     * not stalled until the kill lands). combat_award_hit_xp() (combat.c)
     * increments this AND progress.experience together on every landed
     * hit, silently (no per-hit message) -- combat_defeat() prints the
     * accumulated total as ONE summary line, then zeroes it back out.
     * Live in-memory only, same "meaningless across a reconnect" rule as
     * `fighting`/`riposte_ready` above -- worst case (a disconnect
     * mid-fight followed by a later, unrelated fight) is a slightly
     * inflated SUMMARY MESSAGE, never a wrong amount of real experience,
     * since progress.experience itself is always incremented correctly
     * regardless of this field. */
    long xp_gained_this_fight;

    /* Combat vitality drain (user 2026-08-03: "vitality should decrease
     * when fighting to about .75 of a point per round", then "drop the
     * vitality drain when fighting 20%" -- 0.6/round net). vit is an int
     * (progress_t.vit) but the requested rate isn't a whole number, so
     * combat_process_run() (combat.c) adds 0.6 here every round either
     * side is still fighting and spends off whole points as they
     * accumulate (being_spend_vit()) -- nets out to exactly 0.6/round on
     * average (3 spends per 5 rounds) without needing vit itself to go
     * fractional. Live in-memory only, same "meaningless across a
     * reconnect" rule as `fighting`/`xp_gained_this_fight` above --
     * worst case is losing a fraction of a point of pending drain. */
    float vit_fatigue_accum;

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
     * (buffs/debuffs/status)" backlog item) -- see affect.h. Persisted
     * for a real PC (affect_repo.h/.c, player_active_affect table) as of
     * 2026-07-26 -- the original SneezyMUD round-trips active affects
     * through every login/logout, so Tobin now matches. A quick linkdead
     * reconnect (within LINKDEAD_PURGE_SECONDS) never actually touches
     * this persistence layer at all -- it reuses the same live being_t
     * directly, so an active buff was already surviving that path before
     * this change too; the DB layer only matters once the being_t is
     * genuinely destroyed and reloaded (a real quit!, the 5-minute
     * linkdead auto-purge, or a plain process restart). A mob's own
     * affects (charmed pets, a polymorph's temporary body) are still
     * never persisted -- see affect_repo.h. */
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

    /* `yoginsa` (Monk) background task (user 2026-07-28: "yoginsa should
     * be automatic, a task" -- reverting this audit item's original
     * single-action scope-down back toward the real upstream's own
     * recurring `task_yoginsa()` shape, disc/disc_monk_meditation.cc,
     * now that the pattern exists via planting.c above). Set true by
     * cmd_yoginsa.c when meditation starts (auto-sitting a standing
     * caster first, same as before); meditate_tick_run() (meditate.c)
     * rolls a fresh heal every REGEN_PULSES tick while this stays true,
     * same formula cmd_yoginsa.c's own single-shot version used.
     * Cleared -- with a message -- the moment the being stops resting/
     * sitting or starts fighting; also toggled off by typing the
     * command again while already meditating. `cast meditate`/`pray
     * penance` (Mage/Druid/Cleric) stay single-action, unaffected --
     * the user's request was scoped to yoginsa specifically. Live
     * in-memory only, same reconnect rule as `fighting`/`sneaking`. */
    bool meditating;

    /* `cast`/`pray` multi-round casting delay (user 2026-08-09: "spell
     * casting should take 2-3 rounds before hitting with purple colored
     * messaging... druids should have modified messages that mages have
     * except those messages should have a forest flavor... druid
     * messaging should be <y>" -- Cleric's `pray`, cmd_pray.c, is
     * explicitly NOT in scope and stays instant). Set by cmd_cast.c's
     * cmd_cast() once every gate (class/level/discipline/mana/component/
     * proficiency roll) has already passed and been paid -- the caster
     * has fully committed to the cast, only the EFFECT is deferred.
     * spellcast_tick_run() (spellcast.c), registered every
     * COMBAT_ROUND_PULSES alongside combat/meditate, counts
     * `cast_rounds_left` down to 0, printing that round's Mage-purple/
     * Druid-yellow flavor lines each tick, then invokes the real spell
     * effect (cmd_cast_resolve_effect(), the same per-spell dispatch
     * chain `cast` always used, just moved out from under the instant
     * path) once the delay completes. `cast_spell_name` is re-looked-up
     * via skill_find() at resolution time rather than caching a
     * skill_def_t* directly (being.h can't include skill.h -- circular).
     * `cast_target` is a raw being_t* (self-casts point at `ch` itself,
     * never NULL) -- being_destroy() clears it the same way it already
     * clears `fighting`/`last_heal_target` if the target dies mid-cast,
     * which spellcast_tick_run() reads as "target vanished, fizzle".
     * Live in-memory only, same "no reconnect persistence" convention as
     * `meditating`/`fighting` above. */
    bool is_casting;
    int cast_rounds_left;
    int cast_rounds_total;
    char cast_spell_name[64];
    struct being *cast_target;
    /* Per-round mana payment (user 2026-08-16: "mana should deplete for
     * each round of casting ... that way if they lose concentration it
     * doesn't cost the same as a full cast"). A delayed Mage/Druid cast
     * no longer pays its whole cost up front -- `cast_mana_cost` is the
     * full spell cost fixed at cast time, and `cast_mana_paid` tracks how
     * much spellcast_tick_run() has drawn so far as it charges a
     * proportional share each round. A cast shattered mid-way leaves the
     * remainder unpaid; a completed cast has paid exactly the full cost.
     * Both 0 for immortals and non-mana classes (nothing to charge). */
    int cast_mana_cost;
    int cast_mana_paid;
    /* Distraction counter (Sneezy spelltask parity, user 2026-08-10):
     * disruptive maneuvers landed on a caster mid-`cast` (bash/kick/trip/
     * grapple) add to this via spellcast_distract(); spellcast_tick_run()
     * rolls it each round -- a high enough distraction shatters the
     * in-progress spell, otherwise the caster shakes it off but the cast
     * takes a round longer. Wisdom-mitigated (Sneezy ties concentration
     * to WIS, not to the wizardry skill). Plain melee does NOT distract,
     * matching upstream. Cleared to 0 each round it's rolled. */
    int cast_distracted;

    /* `feign death` (Monk, level 25, level-25 audit batch: "Play dead to
     * avoid detection or attack."). Set by cmd_feigndeath.c; checked by
     * mob_ai.c's mob_try_aggress() to skip a feigning PC when an
     * aggressive mob picks a new target. Cleared by moving, fighting, or
     * typing the command again -- same "live in-memory only, no
     * reconnect persistence" convention as `meditating`/`fighting`
     * above. */
    bool feigning;

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

/* Same word bucketing as being_health_word() above, but for a raw 0-100
 * percentage directly (being_health_word() itself is just this applied to
 * b->progress.hp/max_hp) -- shared so any OTHER 0-100 health-style value
 * gets the exact same vocabulary instead of a raw number. Used by `limbs`
 * (cmd_limbs.c, user 2026-08-03: "limbs command, list health words not
 * %") via being_limb_pct(). */
const char *health_word_for_pct(int pct);

/* Same word as health_word_for_pct(), pre-wrapped in a Tobin color tag
 * (colorstring.c's <letter>...<1> convention, same "bright edges, dim
 * middle, red danger" gradient shape obj_condition_word() (obj.c) uses
 * for item condition -- user 2026-08-03: "tastefully colored like item
 * condition"). Used by game_loop.c's per-round prompt Tank:/Vict: tags. */
const char *health_word_for_pct_colored(int pct);

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

/* Direct port of real SneezyMUD's TPerson::hitLimit() (misc/limits.cc):
 * (baseHp()=21 + ageHpMod()=16 [both constants, since real aging is
 * disabled upstream too] + classHpPerLevel()*level) * getConHpModifier()
 * (a 0.8..1.25 power curve on CON, see being.c's plot_value()). Tobin's
 * own `balance` command multiplier is folded into the class-level term
 * on top of this real formula. User 2026-08-06: "sneezy had good
 * balance, no sense reinventing the wheel." */
/* PC-race effect resistances (race_balance resist_*, docs/RACE_PERKS.md).
 * being_race_resists() rolls the race's percentage for `type` and returns
 * true if the effect should be shrugged off this time. Mobs and immortals
 * never benefit (always false). Applied at the poison (cmd_drink/cmd_sip)
 * and charm/sleep (cmd_cast) effect sites; the elemental types are stored
 * and tunable now, applied where such damage sources are added. */
typedef enum {
    RESIST_POISON, RESIST_CHARM, RESIST_SLEEP, RESIST_PARALYSIS,
    RESIST_ENERGY, RESIST_HEAT, RESIST_COLD
} resist_type_t;
bool being_race_resists(const being_t *b, resist_type_t type);

/* race_balance.talent enum (docs/RACE_PERKS.md). being_race_talent()
 * returns a PC's race talent (0/none for a mob). Adaptable speeds every
 * skill's learn-by-doing; Brawler/Woodland speed their themed skills
 * (skill.c); Detect Magic is applied as an innate affect at login
 * (enter_world, descriptor.c). */
enum {
    RACE_TALENT_NONE = 0,
    RACE_TALENT_ADAPTABLE,
    RACE_TALENT_BRAWLER,
    RACE_TALENT_WOODLAND,
    RACE_TALENT_DETECT_MAGIC,
};
int being_race_talent(const being_t *b);

int being_calc_max_hp(const being_t *b);

/* Direct port of real SneezyMUD's getMaxMove()/moveLimit()
 * (misc/limits.cc): 100 + 15 + level + plotStat(CON,3,18,13), plus
 * Iron Legs*2 for a Monk who knows it. Real upstream also folds in a
 * per-race move mod, gear/affect move bonuses, and an asthmatic-quest-
 * bit halving -- Tobin has none of those systems wired up yet, so
 * they're simply absent rather than approximated. */
int being_calc_max_vit(const being_t *b);

/* Recomputes `b`'s max mana -- see progress_t's own mana/max_mana doc
 * comment for the real per-class formula this ports from SneezyMUD.
 * Returns 0 for any class/kind with no mana pool at all (most of the
 * roster). */
int being_calc_max_mana(const being_t *b);
int being_calc_max_piety(const being_t *b);
int being_mana_gain(const being_t *b);
int being_piety_gain(const being_t *b);

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

/* Like being_hurt_limb(), but touches ONLY the limb's own HP -- b's overall
 * progress.hp is left untouched (the caller is expected to have already
 * applied that separately). TODO.md priority item, user 2026-07-30:
 * "reduce blood and limb-damage generation rates by 50%" -- combat_strike()
 * (combat.c) uses this to apply a limb its own HALVED share of a hit's
 * damage while overall HP still takes the full amount, so limbs decay
 * slower (and cross into a bloody status tier -- see combat_strike()'s own
 * doc comment on obj_grow_pool() -- less often) without changing overall
 * combat lethality/pacing at all. */
void being_hurt_limb_only(being_t *b, limb_t limb, int dmg);

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

/* Heals b's mana by `amount` (clamped at max_mana). No-op for
 * amount <= 0 or for a being with no mana pool (max_mana == 0). Used
 * by the regen tick and by `cast meditate` (cmd_cast.c). */
void being_heal_mana(being_t *b, int amount);

/* Spends `amount` of b's mana (clamped at 0). No-op for amount <= 0.
 * Used by cmd_cast.c to pay a spell's real mana cost (spell_mana.c).
 * Callers must check `progress.mana >= cost` themselves first and
 * refuse the cast otherwise -- this function does not refuse, same
 * "spend, don't gate" division of responsibility being_spend_vit()
 * already uses (cmd_move.c does its own affordability check). */
void being_spend_mana(being_t *b, int amount);
void being_heal_piety(being_t *b, int amount);
void being_spend_piety(being_t *b, int amount);

/* GMCP/MSDP push on vitals change (TobinMUD Client project, 2026-08-05).
 * No-op unless `b` is a connected PC (`b->desc`) with the matching
 * option flag set (d->opt_gmcp/opt_msdp, descriptor.h) -- safe to call
 * unconditionally from any HP/mana-changing site. Called from combat.c
 * wherever `progress.hp` changes; not yet wired to mana (Tobin's own
 * mana-spend paths weren't part of this pass' scope, see STATUS.md). */
void being_notify_vitals_changed(being_t *b);

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
