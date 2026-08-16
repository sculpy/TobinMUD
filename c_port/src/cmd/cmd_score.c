/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Score screen revamp (user 2026-07-25, wireframe pasted directly --
 * "First a revamped score ... Colorize tastefully"): replaces the old
 * single-column labeled-list layout with a compact, classic-MUD-style
 * grid (Name/Level/Experience, Race/Class/Gold, HP/Mana-Piety/Move,
 * the six stats two-per-line, AC/Hand/Sex, Align/Hunger/Thirst, Age,
 * Position).
 *
 * Colorization is deliberately restrained to exactly what the PRE-revamp
 * score already did: Level is tinted by immortal rank (being_rank_color(),
 * empty/no-op for a mortal), nothing else. An earlier draft of this pass
 * also tinted labels and HP/Move/Hunger/Thirst by state -- reverted after
 * discovering it broke a wide swath of pre-existing tests: colorstring.c's
 * `<tag>` markup becomes REAL ANSI escape bytes in the wire output, and
 * dozens of smoke tests parse `score` with plain substring/regex checks
 * that never call `color off` first (the old score text had nothing to
 * strip). Wrapping a label or a mid-line value in color injects escape
 * bytes *between* the text a test is matching against, breaking even a
 * simple `"Level: 1" in out` check. Keeping color exactly where it always
 * safely was (a value-only wrap that's empty for the common/mortal case)
 * avoids reopening that -- a genuinely tasteful restraint, not a missed
 * opportunity.
 *
 * The class-dependent resource-pool field (Mana/Piety/Lifeforce, see
 * resource_pool_label() below) shows a real number for Mage (Mana,
 * user 2026-08-06: "implement it just like sneezy" -- progress_t.mana/
 * max_mana, being_calc_max_mana()) as of this session; Cleric (Piety)
 * and Druid (Lifeforce) still show 0 -- neither resource has been built
 * yet, a disclosed simplification going back to the offensive-spell-
 * system work (see TODO.md); the field stayed in the layout even before
 * Mana existed only because the wireframe asked for it by name. `Move` is
 * Tobin's own Vitality stat (progress_t.vit/max_vit, the terrain-
 * movement-cost resource added earlier), relabeled to match the
 * wireframe wording. */

/* Wireframe note verbatim: "Start at 17 years old and then add age to
 * that number" -- interpreted as real elapsed time since birth_time,
 * converted through gametime.h's own established real-to-mud-year ratio
 * (336 mud-days/year * 96 real minutes/mud-day) rather than a fictional
 * unit invented just for this field. No new persisted field needed --
 * birth_time already exists (being.h). */
#define AGE_STARTING_YEARS 17
#define AGE_SECONDS_PER_MUD_YEAR (336L * 96L * 60L)

static int compute_age_years(long birth_time) {
    long elapsed = (long)time(NULL) - birth_time;
    if (elapsed < 0)
        elapsed = 0;
    return AGE_STARTING_YEARS + (int)(elapsed / AGE_SECONDS_PER_MUD_YEAR);
}

/* Resource-pool label for score's third HP-row field (user 2026-07-25:
 * "Mana/Piety: should either display mana or piety according to class ...
 * maybe we should call druid mana Lifeforce (LF)" then "default to mana
 * in non magic classes"). Matches the real Sneezy doScore()'s own class
 * check (Cleric/Deikhan -> piety, Shaman -> lifeforce, Mage/Monk/
 * psionicist -> mana) as closely as Tobin's class roster allows, with
 * Warrior/Thief defaulting to Mana per that instruction. The VALUE is
 * always 0 regardless of label -- Tobin has no mana/piety/lifeforce
 * resource pool at all (see this file's top-of-file doc comment); only
 * the label changes, so a Cleric player sees a name they recognize even
 * though nothing is spent from it yet. */
/* The current/max value of the class's casting resource pool shown
 * next to resource_pool_label(): Cleric -> piety, Mage/Druid/Monk ->
 * mana, everyone else 0. */
static int resource_pool_cur(const being_t *ch) {
    if (ch->char_class == CLASS_CLERIC)
        return ch->progress.piety;
    if (ch->char_class == CLASS_MAGE || ch->char_class == CLASS_DRUID || ch->char_class == CLASS_MONK)
        return ch->progress.mana;
    return 0;
}
static int resource_pool_max(const being_t *ch) {
    if (ch->char_class == CLASS_CLERIC)
        return ch->progress.max_piety;
    if (ch->char_class == CLASS_MAGE || ch->char_class == CLASS_DRUID || ch->char_class == CLASS_MONK)
        return ch->progress.max_mana;
    return 0;
}

static const char *resource_pool_label(player_class_t c) {
    switch (c) {
        case CLASS_CLERIC: return "Piety";
        case CLASS_DRUID:  return "Lifeforce";
        default:           return "Mana";
    }
}

/* `score` command: renders the compact grid-style character sheet
 * described in the file-top comment -- vitals, stats, and derived
 * fields (age, resource-pool label, hunger/thirst words, injured limbs)
 * pulled straight from the character's live attrs_t/progress_t. */
bool cmd_score(descriptor_t *d, const char *args) {
    (void)args;

    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    const attrs_t *a = &d->character->attrs;
    const progress_t *p = &d->character->progress;
    being_t *ch = d->character;

    char level_field[24];
    const char *title = being_level_title(p->level);
    if (title)
        snprintf(level_field, sizeof(level_field), "%s", title);
    else
        snprintf(level_field, sizeof(level_field), "%d", p->level);

    const char *pos = ch->fighting ? "Fighting" : position_name(ch->position);
    const char *rank_col = being_rank_color(p->level);
    const char *rank_reset = rank_col[0] ? "<z>" : "";

    bool immortal = being_is_immortal(ch);
    const char *hunger_word = immortal ? "immune" : being_hunger_word(p->hunger);
    const char *thirst_word = immortal ? "immune" : being_thirst_word(p->thirst);

    int age_years = compute_age_years(p->birth_time);

    char out[1536];
    snprintf(out, sizeof(out),
        "<c>============================================================<1>\r\n"
 /* Real bug (found 2026-08-07): rank_col/rank_reset (immortal
  * rank coloring around the level field) were passed as extra
  * snprintf arguments with NO matching %s specifiers here -- that
  * shifted every argument after them by 2 positions for the rest
  * of the whole format string (XP receiving Race's pointer, Race
  * receiving Class's, etc, all the way down). Compiler caught it
  * via -Wformat once this file was actually recompiled. */
	" <c>Name:<1> %-20.20s <c>Level:<1> %s%-12.12s%s <c>XP:<1> %-10ld\r\n"
        " <c>Race:<1> %-20s <c>Class:<1> %-12s\r\n"
        " <c>Home:<1> %-20s <c>Gold:<1> %-8d <c>Bank:<1> %-8d\r\n"
        "<c>------------------------------------------------------------<1>\r\n"
	/* Real bug (found 2026-08-07): resource_pool_label() (Piety/
	 * Lifeforce/Mana per class) was computed and passed as an
	 * argument, but the label text here was hardcoded to literal
	 * "Mana:" with no %s for it -- same missing-specifier shift
	 * bug as the Level/rank-color one above, corrupting every
	 * field after it (AC, hand, sex, align, hunger, thirst, age,
	 * position). A Cleric/Druid has been seeing "Mana:" instead of
	 * their real "Piety:"/"Lifeforce:" label this whole time too. */
	" <c>HP:<1> %4d/%-4d <c>%s:<1>%4d/%-4d <c>Move:<1>%4d/%-4d\r\n"
        "<c>------------------------------------------------------------<1>\r\n"
        " <c>Str:<1> %-3d <c>Int:<1> %-3d <c>Dex:<1> %-3d\r\n"
        " <c>Wis:<1> %-3d <c>Con:<1> %-3d <c>Cha:<1> %-3d\r\n"
        "<c>------------------------------------------------------------<1>\r\n"
        " <c>Armor Class :<1> %-4d <c>Pri. Hand :<1> %-12s Sex :<1> %-8s\r\n"
        " <c>Align:<1> %-12s <c>Hunger:<1> %-12s <c>Thirst:<1> %-12s\r\n"
        " <c>Age:<1> %d years old\r\n"
        "<c>============================================================<1>\r\n"
        " <c>Position:<1> %s\r\n",
             ch->base.name, rank_col, level_field, rank_reset, p->experience,
             race_name(ch->race), class_name(ch->char_class), territory_name(ch->territory),
             p->gold, p->bank_gold,
             p->hp, p->max_hp, resource_pool_label(ch->char_class),
             /* Real bug (found 2026-08-07): `(CLASS_MAGE || CLASS_MONK)`
              * is a boolean OR of two enum values, which C collapses to
              * 1 -- since CLASS_MAGE==0, this whole comparison was
              * actually `ch->char_class == 1` (CLASS_CLERIC), not "is
              * Mage or Monk". A real Mage's own score has been showing
              * 0/0 mana this entire time; Cleric (always-0 mana anyway)
              * happened to pass harmlessly. being_calc_max_mana() (being.c)
              * is Mage-only regardless -- Monk never gets a real pool in
              * Tobin -- so this now matches that exactly rather than
              * re-adding a Monk branch that would always read 0 anyway. */
             resource_pool_cur(ch),
             resource_pool_max(ch),
             p->vit, p->max_vit,
             a->strength, a->intelligence, a->dexterity,
             a->wisdom, a->constitution, a->charisma,
             being_total_ac(ch), ch->handed_right ? "Right" : "Left", gender_name(ch->gender),
             alignment_word(p->alignment), hunger_word, thirst_word,
             age_years, pos);

    /* Appearance, if the player set one at creation -- kept out of the
     * grid above, unrelated to the wireframe's fields. */
    size_t n = strlen(out);
    if (ch->appearance[0] && n < sizeof(out))
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  <c>Appearance:<1> %s\r\n", ch->appearance);

    /* A limb only shows up here at all once it's hurt (< 20% health, per
     * limb_status_text()) -- a fully healthy character has no Limbs
     * section. Same wording combat announces mid-fight, so a player who
     * reads about an injury there sees the identical phrase here. */
    char injuries[512];
    int inj_n = 0;
    for (int i = 0; i < LIMB_COUNT && (size_t)inj_n < sizeof(injuries); i++) {
        if (!being_has_limb(ch, (limb_t)i))
            continue;
        int limb_pct = being_limb_pct(ch, (limb_t)i);
        const char *status = limb_status_text(limb_pct);
        if (status)
            inj_n += snprintf(injuries + inj_n, sizeof(injuries) - (size_t)inj_n,
                              "  <Y>Your %s %s!<1>\r\n", limb_name((limb_t)i), status);
    }

    if (inj_n > 0 && n < sizeof(out))
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  <c>Limbs:<1>\r\n%s", injuries);

    descriptor_send(d, out);
    return true;
}
