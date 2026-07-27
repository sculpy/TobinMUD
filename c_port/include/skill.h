/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SKILL_H
#define TOBIN_SKILL_H

#include "being.h"

/* Skill/spell roster (user 2026-07-11: "assign all warrior skills to
 * warriors in three disciplines: combat, warrior skills, advanced warrior
 * skills" -- same request repeated per class). A static reference table,
 * same style as cmd_toggle.c's TOGGLES[] -- names/levels/effects ported
 * from SneezyMUD's real discArray[] (misc/spell_info.cc), trimmed to a
 * simplified 3-tier scheme instead of Sneezy's many sub-disciplines:
 *
 *   SKILL_TIER_COMBAT     -- universal fighting basics, available from
 *                            level 1 (Sneezy's DISC_COMBAT weapon-prof
 *                            skills for casters; class-specific physical
 *                            basics for Warrior/Thief/Monk).
 *   SKILL_TIER_CLASS      -- the class's core, always-known kit (Sneezy's
 *                            "isBasic()" base discipline).
 *   SKILL_TIER_ADVANCED   -- higher-level / optional-specialization
 *                            skills (Sneezy's secondary disciplines).
 *
 * v1 scope: a skill is "known" purely by class + character level meeting
 * `min_level` -- there is no separate practice-point economy yet (Tobin
 * has none), so nothing needs to be actively learned. This may change if
 * a practice/train system gets built later. */

typedef enum {
    SKILL_TIER_COMBAT = 0,
    SKILL_TIER_CLASS,
    SKILL_TIER_ADVANCED,
} skill_tier_t;

typedef struct {
    const char *name;
    player_class_t cls;
    skill_tier_t tier;
    int min_level;
    const char *desc;
} skill_def_t;

/* The full roster (skill.c). Iterate directly for a specific class with
 * skill_count()/skill_at(), or use skill_for_class() to walk just one
 * class's entries. */
int skill_count(void);
const skill_def_t *skill_at(int index);

/* Human-readable label for a tier, capitalized for display (e.g.
 * "Advanced Warrior Skills") -- built from the class name + tier, so
 * there's one source of truth for the heading text `skills` prints. */
const char *skill_tier_label(player_class_t cls, skill_tier_t tier, char *buf, size_t bufsz);

/* Whether `b` currently knows the skill/spell named `name` (level +
 * discipline-percentage gates both pass; immortals always know
 * everything). See skill.c for the exact rules. */
bool being_knows_skill(const being_t *b, const char *name);

/* Exact-name lookup (case-insensitive) within `cls`'s roster, or the
 * whole roster if `any_class` (immortals). Returns NULL if no skill by
 * that exact name exists for the search scope. Used by cmd_trap.c and
 * combat.c (dual wield) to find a skill_def_t to hand to the proficiency
 * functions below, when the caller already knows the exact name rather
 * than a player-typed abbreviation. */
const skill_def_t *skill_find(player_class_t cls, const char *name, bool any_class);

/* Per-skill proficiency (Sneezy-style "learn by doing", user 2026-07-17:
 * "the actual gain in proficiency should be gained as in sneezy" --
 * distinct from the coarse *_disc_pct ACCESS gate above, which only
 * decides whether a skill/spell tier is usable at all). A player's
 * individual skill percentage climbs toward a ceiling set by their
 * discipline percentage for that skill's tier every time they attempt
 * it, and gates the actual success/effect of that attempt via a d100
 * roll. See skill.c for the exact formula. */

/* Current proficiency (0-100) for `ch` in `sk`, or 0 if never attempted.
 * Read-only -- does not trigger a gain-check. Used for display (`skills`). */
int skill_proficiency(const being_t *ch, const skill_def_t *sk);

/* Runs one learn-by-doing attempt: may raise `ch`'s stored proficiency
 * in `sk` by 1 (capped at the discipline-percentage ceiling for `sk`'s
 * tier, subject to a Wisdom-scaled diminishing-returns chance and a
 * short anti-grind cooldown), then returns the resulting (possibly
 * unchanged) proficiency. Call this ONCE per attempt, before rolling
 * success with skill_roll_success() -- the returned value already
 * reflects any gain from THIS attempt, matching Sneezy's own ordering. */
int skill_learn_from_doing(being_t *ch, const skill_def_t *sk);

/* d100 roll against `pct`: true with probability pct/100. 0 always
 * fails, 100 always succeeds. */
bool skill_roll_success(int pct);

#endif
