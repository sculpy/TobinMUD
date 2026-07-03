#ifndef TOBIN_BEING_H
#define TOBIN_BEING_H

#include <stdbool.h>

#include "thing.h"

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

typedef struct {
    int strength;
    int dexterity;
    int constitution;
    int intelligence;
    int wisdom;
    int charisma;
} attrs_t;

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
} progress_t;

/* Per-limb hit points. A simplified stand-in for the original's real
 * per-slot damage system (`bodyPartsDamage body_parts[MAX_WEAR]` in
 * misc/being.h, driven by `wearSlotT`'s 13 equipment-aligned slots plus
 * race-specific `slotChance()` weighting -- see misc/limbs.{h,cc}). Tobin
 * has no equipment/race system yet, so this doesn't weight by slot, but as
 * of the 13-limb set below it's a near 1:1 match of the original's actual
 * slot list (head/neck/two arms/two fingers/body/waist/genitalia/two
 * legs/two feet -- "finger" here in place of the original's "hand", no
 * separate "back" slot). Not persisted (like `fighting`/`desc`) -- see
 * STATUS.md: this follows the same already-accepted precedent as
 * `progress.hp` (only saved at combat defeat, not after every exchange). */
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

typedef struct being {
    thing_t base;        /* first member -- see thing.h */
    long account_id;
    long player_id;
    attrs_t attrs;
    progress_t progress;
    limb_state_t limbs[LIMB_COUNT];

    /* Combat (PvP only for now, see STATUS.md -- no NPCs/mobs exist yet).
     * `fighting` is a live in-memory pointer, never persisted (meaningless
     * across a reconnect, same as `desc`). `last_combat_pulse` prevents a
     * round from being resolved twice when both fighters are iterated. */
    struct being *fighting;
    long last_combat_pulse;

    /* Pulses remaining before this character can act again (see pulse.h).
     * Mortals accumulate this from `being_set_wait()`; immortals always
     * read/write it as a no-op via being_get_wait()/being_set_wait(). */
    int wait_pulses;

    struct descriptor *desc; /* back-pointer to the owning connection, NULL for mobs */
} being_t;

/* Creates an in-memory being_t for a PC. Does not touch the DB -- pair with
 * player_repo.h's player_create()/player_load() for persistence. */
being_t *being_create_pc(const char *name, long account_id, long player_id);
void being_destroy(being_t *b);

/* True iff b->progress.level >= IMMORTAL_LEVEL_MIN. */
bool being_is_immortal(const being_t *b);

/* b's limb HP as a 0-100 percentage of that limb's max_hp. Returns 0 for
 * an invalid being/limb. */
int being_limb_pct(const being_t *b, limb_t limb);

/* True iff any of b's limbs are at 0 HP -- combat.c uses this to apply a
 * placeholder "penalized in combat" hit-chance penalty. There's no
 * hospital system yet to repair a destroyed limb mid-game; currently the
 * only cure is dying and respawning (being_limbs_full_heal() at combat
 * defeat already fully heals every limb). */
bool being_has_destroyed_limb(const being_t *b);

/* Rank title for an immortal level (51-53 "Immortal", 54-57 "God", 58
 * "Greater God", 59 "Administrator", 60+ "Implementor"), or NULL for a
 * mortal level (< IMMORTAL_LEVEL_MIN) -- callers fall back to showing the
 * raw level number in that case. */
const char *being_level_title(int level);

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
