/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_IDEA_REPO_H
#define TOBIN_IDEA_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* Player feature requests, backed by the `idea` table (db/sneezy/idea.sql).
 * Same shape as bug_repo.h. */

/* Files an idea from `submitter`. Returns false on DB error. */
bool idea_repo_add(const char *submitter, const char *body);

/* Renders up to `limit` ideas (newest first) into `out`, each with its id,
 * date, submitter, and text -- for the immortal `idea` listing. Returns
 * false if there are none (out is left empty) or on DB error. */
bool idea_repo_list(char *out, size_t size, int limit);

/* Deletes the idea with the given id. Returns false if no such idea exists
 * or on DB error. Backs `delidea` (cmd_delidea.c). */
bool idea_repo_delete(int id);

#endif
