/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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

#endif
