/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "skill.h"

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

    char header[64];
    snprintf(header, sizeof(header), "\r\n<c>-- %s --<z>\r\n", label);
    *n += (size_t)snprintf(out + *n, outsz - *n, "%s", header);

    int shown = 0;
    int count = skill_count();
    for (int i = 0; i < count && *n < outsz - 128; i++) {
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
            *n += (size_t)snprintf(out + *n, outsz - *n, "  %-26s %s <y>[%d%%]<z>\r\n",
                                   sk->name, sk->desc, prof);
        } else if (!level_ok) {
            *n += (size_t)snprintf(out + *n, outsz - *n,
                                   "  <k>%-26s %s (level %d)<z>\r\n",
                                   sk->name, sk->desc, sk->min_level);
        } else {
            *n += (size_t)snprintf(out + *n, outsz - *n,
                                   "  <k>%-26s %s (%s)<z>\r\n",
                                   sk->name, sk->desc, disc_reason);
        }
    }
    if (shown == 0)
        *n += (size_t)snprintf(out + *n, outsz - *n, "  (none)\r\n");
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
     * below (six classes' worth of skills is well beyond one screen). */
    if (being_is_immortal(d->character)) {
        char out[16000];
        size_t n = 0;
        for (player_class_t cls = 0; cls < CLASS_COUNT; cls++) {
            char header[64];
            snprintf(header, sizeof(header), "\r\n<y>=== %s ===<z>\r\n", class_name(cls));
            n += (size_t)snprintf(out + n, sizeof(out) - n, "%s", header);
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
