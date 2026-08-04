/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* Shows every limb's current health, unconditionally -- unlike score's
 * Limbs section (cmd_score.c), which only lists a limb once it's actually
 * hurt, `limbs` is a full status readout of all LIMB_COUNT limbs at all
 * times. Health reads as a WORD first (user, 2026-08-03: "limbs command,
 * list health words not %", same health_word_for_pct() vocabulary
 * score's overall HP line already uses), with the raw percentage kept
 * alongside in parentheses rather than dropped outright -- several debug/
 * test flows (smoke_test_limb_damage_rate.py's per-round damage-ratio
 * math in particular) genuinely need the exact number, not just which of
 * 10 coarse tiers a limb falls in. Any limb below full health also gets
 * the same injury phrase score/combat use (limb_status_text(),
 * being.h/being.c), so the wording is identical wherever it shows up. */
bool cmd_limbs(descriptor_t *d, const char *args) {
    (void)args;

    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char out[1280];
    int n = snprintf(out, sizeof(out), "\r\n-- %s's Limbs --\r\n", d->character->base.name);
    if (n < 0)
        n = 0;

    for (int i = 0; i < LIMB_COUNT && (size_t)n < sizeof(out); i++) {
        if (!being_has_limb(d->character, (limb_t)i))
            continue;
        int pct = being_limb_pct(d->character, (limb_t)i);
        const char *word = health_word_for_pct(pct);
        const char *status = limb_status_text(pct);
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %-13s %-11s (%3d%%)%s%s\r\n",
                      limb_name((limb_t)i), word, pct,
                      status ? "  -- " : "", status ? status : "");
    }

    descriptor_send(d, out);
    return true;
}
