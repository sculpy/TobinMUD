/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TYPO_REPO_H
#define TOBIN_TYPO_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* Player typo/text reports, backed by the `typo` table (db/tobin/typo.sql).
 * Same shape as idea_repo.h/bug_repo.h. */

/* Files a typo report from `submitter`, standing in room `room_vnum` (0 or
 * negative if unknown -- stored as NULL). Returns false on DB error. */
bool typo_repo_add(const char *submitter, const char *body, int room_vnum);

/* Renders up to `limit` typo reports (newest first) into `out`, each with
 * its id, date, submitter, room (if known), and text -- for the immortal
 * `typo` listing. Returns false if there are none (out is left empty) or
 * on DB error. */
bool typo_repo_list(char *out, size_t size, int limit);

/* Deletes the typo report with the given id. Returns false if no such
 * report exists or on DB error. Backs `deltypo` (cmd_deltypo.c). */
bool typo_repo_delete(int id);

#endif
