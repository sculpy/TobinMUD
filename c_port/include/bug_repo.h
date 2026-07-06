/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BUG_REPO_H
#define TOBIN_BUG_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* Player bug reports, backed by the `bug` table (db/sneezy/bug.sql). */

/* Files a bug report from `submitter`. Returns false on DB error. */
bool bug_repo_add(const char *submitter, const char *body);

/* Renders up to `limit` bug reports (newest first) into `out`, each with its
 * id, date, submitter, and text -- for the immortal `bug` listing. Returns
 * false if there are none (out is left empty) or on DB error. */
bool bug_repo_list(char *out, size_t size, int limit);

/* Deletes the bug with the given id. Returns false if no such bug exists or
 * on DB error. Backs `delbug` (cmd_delbug.c). */
bool bug_repo_delete(int id);

#endif
