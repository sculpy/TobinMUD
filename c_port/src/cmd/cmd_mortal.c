/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "player_repo.h"

/* `mortal` / `immort` -- the at-will mortality toggle (user spec, Session
 * 21). `mortal` (51+) parks the immortal's real rank in
 * progress.true_level and drops their effective level to MORTAL_LEVEL_MAX:
 * every level check in the game reads the effective level, so mortality is
 * total -- wait-states apply, they're killable, and every immortal command
 * (including this one) vanishes from their reach. `immort` brings it back.
 *
 * `immort` is therefore registered at MORTAL_LEVEL_MIN -- the one
 * deliberate exception to "commands above your level are invisible" -- and
 * gates itself on the STORED true level, answering a plain mortal with the
 * exact "Huh?!" an unknown command gets (nothing is leaked). Its help
 * column is NULL so it never appears in mortals' help lists; the `mortal`
 * topic documents the pair. Both states persist (true_level column), so
 * dying or quitting while mortal never eats the rank. */
bool cmd_mortal(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (ch->progress.true_level >= IMMORTAL_LEVEL_MIN) {
        descriptor_send(d, "You are already mortal. Type 'immort' to reclaim your divinity.\r\n");
        return true;
    }

    ch->progress.true_level = ch->progress.level;
    ch->progress.level = MORTAL_LEVEL_MAX;
    player_progress_save(ch->player_id, &ch->progress);

    char msg[160];
    snprintf(msg, sizeof(msg),
             "You set your divinity aside and walk the world as a mortal (level %d).\r\n"
             "Type 'immort' to reclaim it at any time.\r\n", MORTAL_LEVEL_MAX);
    descriptor_send(d, msg);
    return true;
}

/* `immort` -- reverses cmd_mortal() above: restores the stored true_level
 * as the effective level. Gated on the STORED true level rather than the
 * command table, so a genuine mortal sees the same "unknown command"
 * response as if `immort` didn't exist (see file header). */
bool cmd_immort(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (ch->progress.true_level < IMMORTAL_LEVEL_MIN) {
        /* A real mortal typed it: identical to an unknown command. */
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    ch->progress.level = ch->progress.true_level;
    ch->progress.true_level = 0;
    player_progress_save(ch->player_id, &ch->progress);

    char msg[128];
    snprintf(msg, sizeof(msg),
             "Your divinity floods back into you (level %d).\r\n", ch->progress.level);
    descriptor_send(d, msg);
    return true;
}
