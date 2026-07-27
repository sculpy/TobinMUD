/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BUG_REPO_H
#define TOBIN_BUG_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* Player bug reports, backed by the `bug` table (db/tobin/bug.sql). */

/* Files a bug report from `submitter`. Returns false on DB error. */
bool bug_repo_add(const char *submitter, const char *body);

/* Renders up to `limit` OUTSTANDING (unresolved) bug reports (newest first)
 * into `out`, each with its id, date, submitter, and text -- for the
 * immortal `bug` listing. Returns false if there are none (out is left
 * empty) or on DB error. */
bool bug_repo_list(char *out, size_t size, int limit);

/* Deletes the bug with the given id. Returns false if no such bug exists or
 * on DB error. Backs `delbug` (cmd_delbug.c) -- for a bug that doesn't
 * warrant keeping around at all (spam, a duplicate). */
bool bug_repo_delete(int id);

/* Marks the bug with the given id resolved (sets resolved_at, stores
 * `note`), WITHOUT deleting it -- so its submitter can later be told it was
 * fixed. Returns false if no such bug exists, it's already resolved, or on
 * DB error. Backs `edbug <id> [note]` (cmd_edbug.c). */
bool bug_repo_resolve(int id, const char *note);

/* Looks up the submitter and body text of bug `id` (for `edbug`'s live
 * notice to the reporter, if they're online). Returns false if no such bug
 * exists or on DB error; `submitter`/`body` are left empty in that case. */
bool bug_repo_get(int id, char *submitter, size_t submitter_size,
                  char *body, size_t body_size);

#endif
