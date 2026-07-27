/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TELL_HISTORY_REPO_H
#define TOBIN_TELL_HISTORY_REPO_H

#include <stdbool.h>

/* DB access for the `tell_history` table (db/tobin/tobin_migrations.sql)
 * -- an audit trail of every `tell` sent, so an immortal investigating a
 * harassment/abuse report has something to check. Ported from the
 * original's `tellhistory` table (docs/systems/important/
 * communication-system.md), which the earlier docs/systems review found
 * Tobin had no equivalent for at all. `reply` (tell to your last teller)
 * deliberately does NOT read from this table -- same as the original's
 * `desc->last_teller`, that's live descriptor state (descriptor.h),
 * not persisted. */

#define TELL_HISTORY_CAP_PER_RECIPIENT 25

/* Logs one tell and trims `to_player_id`'s history back down to
 * TELL_HISTORY_CAP_PER_RECIPIENT rows (oldest dropped first) if the insert
 * pushed it over. Returns false on DB error; a failed log never blocks the
 * tell itself from being delivered (caller logs best-effort, after send). */
bool tell_history_add(long from_player_id, long to_player_id, const char *message);

#endif
