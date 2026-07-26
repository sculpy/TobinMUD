/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_RULES_REPO_H
#define TOBIN_RULES_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* Numbered game rules, backed by the `rules` table (db/tobin/rules.sql). */

/* Renders the rule list ("  1. Title") into `out`, ascending by number.
 * Returns false if there are no rules (out left empty) or on DB error. */
bool rules_repo_list(char *out, size_t size);

/* Renders rule `num` in full ("Rule N: Title" + body) into `out`. Returns
 * false if there is no such rule (out left empty) or on DB error. */
bool rules_repo_get(int num, char *out, size_t size);

/* Inserts or replaces rule `num` with the given title/body (updated_by =
 * editor's name). Backs `edrules`. Returns false on DB error. */
bool rules_repo_upsert(int num, const char *title, const char *body, const char *who);

#endif
