/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TIPS_REPO_H
#define TOBIN_TIPS_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* Tips system (TODO.md: "tips command + periodic tip echoes (pulse-
 * driven), per-player newbie toggle, tipedit (53+). DB-backed like
 * news/help") -- backed by the `tip` table (db/tobin/tip.sql). Kept
 * one-liner-simple like `bug`/`idea` rather than a full menu editor
 * (news/help's own editors are for long-form, titled content; a tip is
 * one short sentence) -- `tipedit` is a single command with add/list/
 * delete sub-forms, same dispatch shape as `bug`/`delbug`. "Per-player
 * newbie toggle" originally reused the existing PLR_NEWBIE flag
 * (being.h) -- the periodic echo only reaches players currently on the
 * newbie channel. A dedicated PLR_NOTIPS bit (user 2026-07-19) now lets
 * a newbie-channel player silence just the tip echoes with `toggle tips`
 * without leaving the newbie channel entirely. */

/* Files a new tip (`added_by` = the immortal's name). Returns false on
 * DB error. Backs `tipedit <text>`. */
bool tips_repo_add(const char *added_by, const char *body);

/* Picks ONE tip at random into `out`. Returns false (out left empty) if
 * there are no tips at all, or on DB error. Backs bare `tips` and the
 * periodic pulse echo. */
bool tips_repo_random(char *out, size_t size);

/* Renders every tip (id + body, newest first) into `out` for `tipedit
 * list`. Returns false (out left empty) if there are none. */
bool tips_repo_list(char *out, size_t size);

/* Deletes the tip with the given id. Returns false if no such tip exists
 * or on DB error. Backs `tipedit delete <id>`. */
bool tips_repo_delete(int id);

/* Pulse callback (registered in main.c): echoes one random tip, prefixed
 * "Tip:", to every connected being currently on the newbie channel
 * (PLR_NEWBIE, being.h) that hasn't opted out with `toggle tips`
 * (PLR_NOTIPS, being.h) -- silently does nothing if there are no tips or
 * no eligible connections right now. */
void tips_pulse_tick(long pulse_num);

#endif
