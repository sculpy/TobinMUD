/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "player_repo.h"
#include "practice.h"
#include "skill.h"
#include "thing.h"

/* `practice [basic|combat|advanced] [<#>]` -- two related but distinct
 * things (user 2026-07-17: "display the skill listing for that
 * discipline along with percentage of proficiency unless in front of a
 * guildmaster then the offer to practice still applies"):
 *   `practice <discipline>`      -- shows that discipline's skill/spell
 *                                    listing with per-skill proficiency,
 *                                    ANYWHERE, no guildmaster required.
 *   `practice <discipline> <#>`  -- actually SPENDS practice points at a
 *                                    guildmaster to raise the discipline
 *                                    percentage (which acts as the
 *                                    ceiling every skill's own individual
 *                                    proficiency climbs toward via use --
 *                                    see skill.c). Random 1-2% per point.
 * Guildmaster tiers are identified by mob.level (51=Basic, 80=Combat,
 * 100=Advanced). Advanced is gated on Basic==100 AND Combat==100. */

/* Finds a guildmaster mob in ch's room that matches ch's class AND the
 * requested tier (mob.level). Returns NULL if none is present. */
static being_t *find_guildmaster(const being_t *ch, int tier_level) {
    if (!ch->base.roomp)
        return NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *mob = (being_t *)t;
        if (!mob->mob_class_known || mob->char_class != ch->char_class)
            continue;
        if (mob->progress.level != tier_level)
            continue;
        if (thing_name_matches(t->name, "guildmaster", strlen("guildmaster")))
            return mob;
    }
    return NULL;
}

/* Returns whichever tier-appropriate guildmaster is in the room, or NULL. */
static being_t *find_any_guildmaster(const being_t *ch, int *out_tier) {
    static const int TIERS[] = { GUILD_LEVEL_BASIC, GUILD_LEVEL_COMBAT, GUILD_LEVEL_ADVANCED };
    for (int i = 0; i < 3; i++) {
        being_t *gm = find_guildmaster(ch, TIERS[i]);
        if (gm) {
            if (out_tier)
                *out_tier = TIERS[i];
            return gm;
        }
    }
    return NULL;
}

/* Returns the name ("Basic", "Combat", "Advanced") for a tier constant. */
static const char *tier_name(int tier) {
    if (tier == GUILD_LEVEL_COMBAT)   return "Combat";
    if (tier == GUILD_LEVEL_ADVANCED) return "Advanced";
    return "Basic";
}

/* Returns the current discipline percentage for a tier. Immortals always
 * read as 100% (user 2026-08-03: "all skills/spells at maxed potential
 * without spending practice points") -- a read-time override, doesn't
 * touch the stored column, so nothing needs undoing if they're ever
 * demoted back to mortal. */
static int disc_pct(const being_t *ch, int tier) {
    if (being_is_immortal(ch))
        return 100;
    if (tier == GUILD_LEVEL_COMBAT)   return ch->progress.combat_disc_pct;
    if (tier == GUILD_LEVEL_ADVANCED) return ch->progress.advanced_disc_pct;
    return ch->progress.basic_disc_pct;
}

/* Sets the discipline percentage for a tier. */
static void disc_pct_set(being_t *ch, int tier, int val) {
    if (tier == GUILD_LEVEL_COMBAT)        ch->progress.combat_disc_pct = val;
    else if (tier == GUILD_LEVEL_ADVANCED) ch->progress.advanced_disc_pct = val;
    else                                   ch->progress.basic_disc_pct = val;
}

/* Maps a GUILD_LEVEL_* tier constant to the matching skill.h tier. */
static skill_tier_t guild_tier_to_skill_tier(int tier) {
    if (tier == GUILD_LEVEL_COMBAT)   return SKILL_TIER_COMBAT;
    if (tier == GUILD_LEVEL_ADVANCED) return SKILL_TIER_ADVANCED;
    return SKILL_TIER_CLASS;
}

/* What `cast`/`pray` (cmd_cast.c/cmd_pray.c) actually consume to invoke a
 * given class's non-combat spells -- shown inline in the listing below
 * (user 2026-07-18: "write help files for each including what symbol/
 * component/commodity is needed to cast/pray"), rather than a help_topic
 * row per spell (~300 of them; this codebase reserves help_topic for
 * commands, not individual spell entries). NULL for classes/tiers that
 * don't cast at all (Warrior/Thief/Monk, and everyone's SKILL_TIER_COMBAT
 * weapon-proficiency rows, which `cast`/`pray` refuse outright). */
static const char *spell_reagent_note(player_class_t cls, skill_tier_t sktier) {
    if (sktier == SKILL_TIER_COMBAT)
        return NULL;
    if (cls == CLASS_MAGE || cls == CLASS_DRUID)
        return "`cast` needs a spell component";
    if (cls == CLASS_CLERIC)
        return "`pray` needs a holy symbol";
    return NULL;
}

/* `practice <discipline>` (no count): shows the skill/spell listing for
 * that ONE discipline, with each accessible skill's own individual
 * proficiency percentage (Sneezy-style learn-by-doing, see skill.c) --
 * available anywhere, not just at a guildmaster (user 2026-07-17: "this
 * command should display the skill listing for that discipline along
 * with percentage of proficiency unless in front of a guildmaster then
 * the offer to practice still applies"). `has_guildmaster` adds the
 * training reminder at the end when one is actually present. */
static void practice_show_discipline(descriptor_t *d, being_t *ch, int tier, bool has_guildmaster) {
    player_class_t cls = ch->char_class;
    skill_tier_t sktier = guild_tier_to_skill_tier(tier);
    /* Immortals see every skill as available regardless of character
     * level (999 sentinel, same precedent `skills`'s own immortal
     * branch already uses in cmd_skills.c) -- consistent with
     * being_knows_skill()'s "immortals always know everything" rule. */
    int level = being_is_immortal(ch) ? 999 : ch->progress.level;
    int this_pct = disc_pct(ch, tier);

    bool unlocked = this_pct > 0;
    const char *lock_reason = "practice this discipline to unlock";
    if (tier == GUILD_LEVEL_ADVANCED) {
        int basic = disc_pct(ch, GUILD_LEVEL_BASIC);
        int combat = disc_pct(ch, GUILD_LEVEL_COMBAT);
        unlocked = basic >= 100 && combat >= 100 && this_pct > 0;
        if (basic < 100 || combat < 100)
            lock_reason = "master Basic and Combat first";
    }

    const char *reagent = spell_reagent_note(cls, sktier);

    char out[8192];
    size_t n = 0;
    n += (size_t)snprintf(out + n, sizeof(out) - n,
                          "\r\n<c>-- %s discipline: %d%% --<z>\r\n",
                          tier_name(tier), this_pct);
    if (reagent)
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  <y>(%s to invoke any of these)<z>\r\n", reagent);

    int shown = 0;
    int count = skill_count();
    for (int i = 0; i < count && n < sizeof(out) - 128; i++) {
        const skill_def_t *sk = skill_at(i);
        if (sk->cls != cls || sk->tier != sktier)
            continue;
        shown++;

        /* User 2026-08-03: drop the per-skill description (help files
         * already cover that) and report proficiency as "(learned/max
         * potential)" plus a descriptive colorized word instead of a raw
         * percentage -- max potential is the discipline's own ceiling
         * (this_pct), the cap every skill's individual proficiency
         * climbs toward (see this function's top doc comment). */
        bool level_ok = level >= sk->min_level;
        if (!level_ok) {
            n += (size_t)snprintf(out + n, sizeof(out) - n,
                                  "  <k>%-26s (level %d)<z>\r\n",
                                  sk->name, sk->min_level);
        } else if (!unlocked) {
            n += (size_t)snprintf(out + n, sizeof(out) - n,
                                  "  <k>%-26s (%s)<z>\r\n",
                                  sk->name, lock_reason);
        } else {
            int prof = skill_proficiency(ch, sk);
            n += (size_t)snprintf(out + n, sizeof(out) - n,
                                  "  %-26s (%d/%d) %s\r\n",
                                  sk->name, prof, this_pct,
                                  skill_proficiency_word_colored(prof));
        }
    }
    if (shown == 0)
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  (none)\r\n");

    if (has_guildmaster) {
        char tier_lower[16];
        snprintf(tier_lower, sizeof(tier_lower), "%s", tier_name(tier));
        for (char *p = tier_lower; *p; p++)
            *p = (char)tolower((unsigned char)*p);
        n += (size_t)snprintf(out + n, sizeof(out) - n,
                              "\r\nType '<c>practice %s <#><z>' to spend practice points here.\r\n",
                              tier_lower);
    }

    descriptor_page_start(d, out, 0);
}

/* `practice [<discipline> [<#>]]` command -- see file-top comment for the
 * full split between the display and spend behaviors. With no argument,
 * shows the caller's own tier percentages/practice points (plus a
 * guildmaster hint if one is present). With a discipline but no count,
 * delegates to practice_show_discipline() for the per-skill listing. With
 * a count, requires the matching guildmaster in the room and actually
 * spends practice points to raise that tier's percentage. */
bool cmd_practice(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args == ' ')
        args++;

    /* No argument: always shows your own discipline percentages and
     * practice points, regardless of location -- this is your own
     * character sheet, not something a guildmaster needs to tell you.
     * If a guildmaster happens to be here, add their flavor line and a
     * training reminder. */
    if (!*args) {
        char msg[512];
        int n = snprintf(msg, sizeof(msg),
                 "Basic: <c>%d%%<z>  Combat: <c>%d%%<z>  Advanced: <c>%d%%<z>\r\n"
                 "Practice points available: <c>%d<z>\r\n",
                 disc_pct(ch, GUILD_LEVEL_BASIC),
                 disc_pct(ch, GUILD_LEVEL_COMBAT),
                 disc_pct(ch, GUILD_LEVEL_ADVANCED),
                 ch->progress.practice_points);

        int tier = 0;
        being_t *gm = find_any_guildmaster(ch, &tier);
        if (gm) {
            char gmname[128];
            being_display_name_cap(gm, gmname, sizeof(gmname));
            /* Each guildmaster only teaches their own tier -- don't invite
             * a discipline this particular one will just refuse. */
            char tier_lower[16];
            snprintf(tier_lower, sizeof(tier_lower), "%s", tier_name(tier));
            for (char *p = tier_lower; *p; p++)
                *p = (char)tolower((unsigned char)*p);
            snprintf(msg + n, sizeof(msg) - (size_t)n,
                     "%s is here. Type '<c>practice %s<z>' to train.\r\n",
                     gmname, tier_lower);
        }
        descriptor_send(d, msg);
        return true;
    }

    /* Parse: practice <discipline> [<count>] */
    char word[32] = "";
    const char *rest = args;
    int i = 0;
    while (*rest && *rest != ' ' && i < (int)sizeof(word) - 1)
        word[i++] = *rest++;
    word[i] = '\0';
    while (*rest == ' ')
        rest++;

    bool has_count = (*rest != '\0');
    int count = 1;
    if (has_count) {
        count = atoi(rest);
        if (count < 1)
            count = 1;
    }

    size_t wlen = strlen(word);
    int tier;
    if (wlen && strncasecmp(word, "basic", wlen) == 0)
        tier = GUILD_LEVEL_BASIC;
    else if (wlen && strncasecmp(word, "combat", wlen) == 0)
        tier = GUILD_LEVEL_COMBAT;
    else if (wlen && strncasecmp(word, "advanced", wlen) == 0)
        tier = GUILD_LEVEL_ADVANCED;
    else if (wlen && strncasecmp(word, class_name(ch->char_class), wlen) == 0)
        /* Basic's skills tier is labeled by class name elsewhere (`skills`
         * shows "Warrior Skills", not "Basic Skills"), so accept the
         * caller's own class name here too -- "practice warrior" reads
         * more naturally than "practice basic" for most players. */
        tier = GUILD_LEVEL_BASIC;
    else {
        descriptor_send(d, "Practice what? Try '<c>practice basic<z>', '<c>practice combat<z>', or '<c>practice advanced<z>'.\r\n");
        return true;
    }

    being_t *gm = find_guildmaster(ch, tier);

    /* `practice <discipline>` with no count: just show the listing +
     * per-skill proficiency, anywhere -- no guildmaster required. Only
     * an explicit count actually spends points, which still needs one
     * present (checked below). */
    if (!has_count) {
        practice_show_discipline(d, ch, tier, gm != NULL);
        return true;
    }

    if (!gm) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "You don't see a %s guildmaster of your discipline here.\r\n",
                 tier_name(tier));
        descriptor_send(d, msg);
        return true;
    }

    /* Advanced gate: Basic AND Combat must both be 100%. */
    if (tier == GUILD_LEVEL_ADVANCED) {
        if (disc_pct(ch, GUILD_LEVEL_BASIC) < 100 || disc_pct(ch, GUILD_LEVEL_COMBAT) < 100) {
            descriptor_send(d, "The guildmaster shakes their head. "
                "\"Master your Basic and Combat disciplines first.\"\r\n");
            return true;
        }
    }

    int cur = disc_pct(ch, tier);
    if (cur >= 100) {
        char msg[128];
        snprintf(msg, sizeof(msg), "You have already mastered your %s discipline.\r\n",
                 tier_name(tier));
        descriptor_send(d, msg);
        return true;
    }

    if (ch->progress.practice_points <= 0) {
        descriptor_send(d, "You have no practice points. Gain more by leveling up.\r\n");
        return true;
    }

    /* Spend up to `count` points, each awarding random 1-2%. */
    int spent = 0;
    for (int p = 0; p < count; p++) {
        if (ch->progress.practice_points <= 0 || cur >= 100)
            break;
        ch->progress.practice_points--;
        int gain = 1 + (rand() % 2);
        cur += gain;
        if (cur > 100)
            cur = 100;
        spent++;
    }
    disc_pct_set(ch, tier, cur);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "You practice with the guildmaster. <g>%s discipline: %d%%<z> (%d point%s spent, %d remaining).\r\n",
             tier_name(tier), cur, spent, spent == 1 ? "" : "s",
             ch->progress.practice_points);
    descriptor_send(d, msg);

    player_progress_save(ch->player_id, &ch->progress);
    return true;
}
