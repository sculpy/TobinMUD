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
 * three tiers (Combat / <Class> Skills / Advanced <Class> Skills) --
 * user 2026-07-11: "assign all warrior skills to warriors in three
 * disciplines...", repeated per class. A skill is "known" once level
 * reaches its threshold AND (for the Class/Advanced tiers -- Combat is
 * innate) the player's discipline percentage is above 0 (user
 * 2026-07-12: "add the practice command so players have to visit a
 * guildmaster to gain skills based upon percentage of discipline
 * learned" -- see cmd_practice.c). `force_known` (immortals) shows
 * everything as known regardless of level/percentage. */
static void print_tier(descriptor_t *d, player_class_t cls, skill_tier_t tier, int level,
                       int basic_pct, int advanced_pct, bool force_known,
                       char *out, size_t outsz, size_t *n) {
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
        } else if (tier == SKILL_TIER_ADVANCED) {
            disc_ok = basic_pct >= 95 && advanced_pct > 0;
            disc_reason = basic_pct < 95 ? "need 95% Basic first" : "practice Advanced discipline";
        }

        if (force_known || (level_ok && disc_ok)) {
            *n += (size_t)snprintf(out + *n, outsz - *n, "  %-26s %s\r\n", sk->name, sk->desc);
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
        /* Unreachable in practice (the roster is never empty), but keeps
         * this loop-free if it ever were. */
        descriptor_send(d, "No skills are defined yet.\r\n");
        return true;
    }

    char out[8192];
    size_t n = 0;

    /* Immortals see and can use every class's full roster (user
     * 2026-07-12: "immortals can use any skill or spell in game, no
     * class restrictions") -- print every class's tiers, all shown as
     * known (level 999 sidesteps the per-skill min_level dimming, same
     * as the level-gate bypass in cmd_cast.c/cmd_pray.c). */
    if (being_is_immortal(d->character)) {
        for (player_class_t cls = 0; cls < CLASS_COUNT; cls++) {
            char cout[4096];
            size_t cn = 0;
            char header[64];
            snprintf(header, sizeof(header), "\r\n<y>=== %s ===<z>\r\n", class_name(cls));
            cn += (size_t)snprintf(cout + cn, sizeof(cout) - cn, "%s", header);
            print_tier(d, cls, SKILL_TIER_COMBAT, 999, 100, 100, true, cout, sizeof(cout), &cn);
            print_tier(d, cls, SKILL_TIER_CLASS, 999, 100, 100, true, cout, sizeof(cout), &cn);
            print_tier(d, cls, SKILL_TIER_ADVANCED, 999, 100, 100, true, cout, sizeof(cout), &cn);
            descriptor_send(d, cout);
        }
        return true;
    }

    player_class_t cls = d->character->char_class;
    int level = d->character->progress.level;
    int basic_pct = d->character->progress.basic_disc_pct;
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
                          "Basic discipline: <y>%d%%<z>   Advanced discipline: <y>%d%%<z>%s\r\n",
                          basic_pct, advanced_pct,
                          basic_pct < 95 ? " <k>(locked until Basic reaches 95%)<z>" : "");
    print_tier(d, cls, SKILL_TIER_COMBAT, level, basic_pct, advanced_pct, false, out, sizeof(out), &n);
    print_tier(d, cls, SKILL_TIER_CLASS, level, basic_pct, advanced_pct, false, out, sizeof(out), &n);
    print_tier(d, cls, SKILL_TIER_ADVANCED, level, basic_pct, advanced_pct, false, out, sizeof(out), &n);
    descriptor_send(d, out);
    return true;
}
