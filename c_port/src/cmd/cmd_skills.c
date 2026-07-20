/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "being.h"
#include "skill.h"

/* Appends a formatted chunk to `out`/`*n`, the way every list-builder in
 * this file needs to. NOT the same as the raw `*n += snprintf(out + *n,
 * outsz - *n, ...)` pattern used elsewhere in the codebase: snprintf's
 * return value is how much WOULD have been written if the buffer were
 * big enough, not how much actually was, so once one call truncates, `*n`
 * ends up larger than `outsz` -- and the next call's `outsz - *n` then
 * underflows (both unsigned) into a huge bogus size, handing snprintf
 * free rein to write past the real buffer end. Guarding entry (`*n >=
 * outsz` bails before computing outsz - *n) and clamping `*n` to `outsz`
 * afterward closes both ends of that hole; a full buffer just silently
 * stops accumulating instead of corrupting memory. Found the hard way:
 * an immortal typing `skills` (the "every class, everything unlocked"
 * branch below) reliably segfaulted the live server, because the ~300+
 * skill catalog doesn't fit in one screen's buffer and several call
 * sites here had no guard at all. */
static void append_fmt(char *out, size_t outsz, size_t *n, const char *fmt, ...) {
    if (*n >= outsz)
        return;
    va_list ap;
    va_start(ap, fmt);
    int wrote = vsnprintf(out + *n, outsz - *n, fmt, ap);
    va_end(ap);
    if (wrote > 0)
        *n += (size_t)wrote;
    if (*n > outsz)
        *n = outsz;
}

/* What `cast`/`pray` (cmd_cast.c/cmd_pray.c) consume to invoke a class's
 * non-combat spells -- shown inline here (user 2026-07-18: "write help
 * files for each including what symbol/component/commodity is needed to
 * cast/pray"), same duplicated-static-helper convention as
 * cmd_practice.c's identical spell_reagent_note(). NULL for classes/tiers
 * that don't cast (Warrior/Thief/Monk, and every class's SKILL_TIER_COMBAT
 * weapon rows, which `cast`/`pray` refuse outright). */
static const char *spell_reagent_note(player_class_t cls, skill_tier_t tier) {
    if (tier == SKILL_TIER_COMBAT)
        return NULL;
    if (cls == CLASS_MAGE || cls == CLASS_DRUID)
        return "`cast` needs a spell component";
    if (cls == CLASS_CLERIC)
        return "`pray` needs a holy symbol";
    return NULL;
}

/* `skills`: lists a player's class's skill/spell roster, grouped into the
 * three tiers (Combat / <Class> Skills / Advanced <Class> Skills). A skill
 * is "known" (usable) once level reaches its threshold AND the player's
 * relevant discipline percentage is above 0 -- that ACCESS gate is
 * unchanged. A known skill also shows its own individual proficiency
 * percentage (Sneezy-style learn-by-doing, user 2026-07-17), which is a
 * separate number that climbs with use and gates the actual success of
 * each attempt (see skill_learn_from_doing()/skill_roll_success() in
 * cmd_cast.c/cmd_pray.c/cmd_trap.c/combat.c). `force_known` (immortals)
 * shows everything as accessible regardless of level/percentage, but
 * still shows their own real (usually 0%) proficiency, since immortals
 * bypass the gates rather than having pre-set skill values. */
static void print_tier(descriptor_t *d, const being_t *ch, player_class_t cls, skill_tier_t tier,
                       int level, int basic_pct, int combat_pct, int advanced_pct,
                       bool force_known, char *out, size_t outsz, size_t *n) {
    char label[48];
    skill_tier_label(cls, tier, label, sizeof(label));

    append_fmt(out, outsz, n, "\r\n<c>-- %s --<z>\r\n", label);
    const char *reagent = spell_reagent_note(cls, tier);
    if (reagent)
        append_fmt(out, outsz, n, "  <y>(%s to invoke any of these)<z>\r\n", reagent);

    int shown = 0;
    int count = skill_count();
    for (int i = 0; i < count && *n < outsz; i++) {
        const skill_def_t *sk = skill_at(i);
        if (sk->cls != cls || sk->tier != tier)
            continue;
        shown++;

        bool level_ok = level >= sk->min_level;
        bool disc_ok = true;
        const char *disc_reason = NULL;
        if (tier == SKILL_TIER_CLASS) {
            disc_ok = basic_pct > 0;
            disc_reason = "practice Basic discipline";
        } else if (tier == SKILL_TIER_COMBAT) {
            disc_ok = combat_pct > 0;
            disc_reason = "practice Combat discipline";
        } else if (tier == SKILL_TIER_ADVANCED) {
            disc_ok = basic_pct >= 100 && combat_pct >= 100 && advanced_pct > 0;
            if (basic_pct < 100 || combat_pct < 100)
                disc_reason = "master Basic and Combat first";
            else
                disc_reason = "practice Advanced discipline";
        }

        if (force_known || (level_ok && disc_ok)) {
            int prof = skill_proficiency(ch, sk);
            append_fmt(out, outsz, n, "  %-26s %s <y>[%d%%]<z>\r\n",
                       sk->name, sk->desc, prof);
        } else if (!level_ok) {
            append_fmt(out, outsz, n,
                       "  <k>%-26s %s (level %d)<z>\r\n",
                       sk->name, sk->desc, sk->min_level);
        } else {
            append_fmt(out, outsz, n,
                       "  <k>%-26s %s (%s)<z>\r\n",
                       sk->name, sk->desc, disc_reason);
        }
    }
    if (shown == 0)
        append_fmt(out, outsz, n, "  (none)\r\n");
    (void)d;
}

bool cmd_skills(descriptor_t *d, const char *args) {
    (void)args;
    if (!d->character)
        return true;

    if (skill_at(0) == NULL) {
        descriptor_send(d, "No skills are defined yet.\r\n");
        return true;
    }

    char out[8192];
    size_t n = 0;

    /* Immortals see every class's full roster, all shown as known --
     * accumulated into one buffer and paged, same as the mortal path
     * below (six classes' worth of skills is well beyond one screen).
     * 65536 bytes -- the full ~300-skill catalog across every class,
     * fully spelled out at 100%, runs well past the previous 16000-byte
     * size (the actual cause of a real crash, see append_fmt's comment
     * above); comfortably under the pager's own 128KB buffer. */
    if (being_is_immortal(d->character)) {
        char out[65536];
        size_t n = 0;
        for (player_class_t cls = 0; cls < CLASS_COUNT; cls++) {
            append_fmt(out, sizeof(out), &n, "\r\n<y>=== %s ===<z>\r\n", class_name(cls));
            print_tier(d, d->character, cls, SKILL_TIER_COMBAT, 999, 100, 100, 100, true, out, sizeof(out), &n);
            print_tier(d, d->character, cls, SKILL_TIER_CLASS, 999, 100, 100, 100, true, out, sizeof(out), &n);
            print_tier(d, d->character, cls, SKILL_TIER_ADVANCED, 999, 100, 100, 100, true, out, sizeof(out), &n);
        }
        descriptor_page_start(d, out, 0);
        return true;
    }

    player_class_t cls = d->character->char_class;
    int level = d->character->progress.level;
    int basic_pct = d->character->progress.basic_disc_pct;
    int combat_pct = d->character->progress.combat_disc_pct;
    int advanced_pct = d->character->progress.advanced_disc_pct;

    bool has_any = false;
    for (int i = 0; i < skill_count(); i++) {
        if (skill_at(i)->cls == cls) {
            has_any = true;
            break;
        }
    }
    if (!has_any) {
        descriptor_send(d, "Your class has no skills defined yet.\r\n");
        return true;
    }

    n += (size_t)snprintf(out + n, sizeof(out) - n,
                          "Basic: <y>%d%%<z>   Combat: <y>%d%%<z>   Advanced: <y>%d%%<z>%s\r\n",
                          basic_pct, combat_pct, advanced_pct,
                          (basic_pct < 100 || combat_pct < 100)
                              ? " <k>(Advanced locked until Basic and Combat reach 100%)<z>" : "");
    print_tier(d, d->character, cls, SKILL_TIER_COMBAT, level, basic_pct, combat_pct, advanced_pct, false, out, sizeof(out), &n);
    print_tier(d, d->character, cls, SKILL_TIER_CLASS, level, basic_pct, combat_pct, advanced_pct, false, out, sizeof(out), &n);
    print_tier(d, d->character, cls, SKILL_TIER_ADVANCED, level, basic_pct, combat_pct, advanced_pct, false, out, sizeof(out), &n);
    descriptor_page_start(d, out, 0);
    return true;
}
