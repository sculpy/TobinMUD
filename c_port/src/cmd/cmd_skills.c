/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "skill.h"

/* `skills`: lists a player's class's skill/spell roster, grouped into the
 * three tiers (Combat / <Class> Skills / Advanced <Class> Skills) --
 * user 2026-07-11: "assign all warrior skills to warriors in three
 * disciplines...", repeated per class. A skill is "known" purely by
 * character level meeting its threshold (no practice-point economy exists
 * yet); known skills print plain, not-yet-known ones print dimmed with
 * the level still needed. */
static void print_tier(descriptor_t *d, player_class_t cls, skill_tier_t tier, int level,
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
        if (level >= sk->min_level) {
            *n += (size_t)snprintf(out + *n, outsz - *n, "  %-26s %s\r\n", sk->name, sk->desc);
        } else {
            *n += (size_t)snprintf(out + *n, outsz - *n,
                                   "  <k>%-26s %s (level %d)<z>\r\n",
                                   sk->name, sk->desc, sk->min_level);
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

    player_class_t cls = d->character->char_class;
    int level = d->character->progress.level;

    char out[8192];
    size_t n = 0;
    if (skill_at(0) == NULL) {
        /* Unreachable in practice (the roster is never empty), but keeps
         * this loop-free if it ever were. */
        descriptor_send(d, "No skills are defined yet.\r\n");
        return true;
    }

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

    print_tier(d, cls, SKILL_TIER_COMBAT, level, out, sizeof(out), &n);
    print_tier(d, cls, SKILL_TIER_CLASS, level, out, sizeof(out), &n);
    print_tier(d, cls, SKILL_TIER_ADVANCED, level, out, sizeof(out), &n);
    descriptor_send(d, out);
    return true;
}
