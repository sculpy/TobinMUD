/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* `level` (user 2026-07-19: "a level command that will display when
 * your due for a gain in level, You have X experience and need X
 * experience to level") -- a focused one-line answer to "how close am
 * I", pulled out of `score`'s already-crowded stat dump rather than
 * added as another line there. Reuses progress_xp_for_level()
 * (being.c), the same total-XP-to-reach-a-level curve progress_add_xp()
 * levels a player up against, so this can never drift out of sync with
 * when a level-up actually fires. Immortals (no XP-driven leveling,
 * `promote` instead) and a mortal already at MORTAL_LEVEL_MAX get a
 * distinct message instead of a nonsensical/negative "need" number. */
bool cmd_level(descriptor_t *d, const char *args) {
    (void)args;
    if (!d->character)
        return true;

    const progress_t *p = &d->character->progress;

    if (being_is_immortal(d->character)) {
        descriptor_send(d, "Immortals don't gain levels through experience.\r\n");
        return true;
    }

    if (p->level >= MORTAL_LEVEL_MAX) {
        char msg[96];
        snprintf(msg, sizeof(msg),
                 "You have %ld experience. You are already at the maximum mortal level.\r\n",
                 p->experience);
        descriptor_send(d, msg);
        return true;
    }

    long needed = progress_xp_for_level(p->level + 1) - p->experience;
    if (needed < 0)
        needed = 0;

    char msg[128];
    snprintf(msg, sizeof(msg),
             "You have %ld experience and need %ld more experience to level.\r\n",
             p->experience, needed);
    descriptor_send(d, msg);
    return true;
}
