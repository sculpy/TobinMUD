#ifndef TOBIN_HELP_REPO_H
#define TOBIN_HELP_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* DB-backed help topics (db/sneezy/help_topic.sql) shown by `help <topic>`
 * and edited in-game by `hedit` (cmd_help.c / cmd_hedit.c). New-for-Tobin:
 * the original kept help as flatfiles under lib/help/; Tobin keeps bulk
 * content in MariaDB like everything else. */

#define HELP_TOPIC_NAME_LEN 32
#define HELP_BODY_MAX 4096

/* Exact-name lookup (used by hedit to preload). Returns false if the topic
 * doesn't exist; body gets the stored text ('\n' line endings --
 * descriptor_send() normalizes to CRLF on the way out). */
bool help_topic_load_exact(const char *name, char *body, size_t body_size);

/* Lookup for `help <topic>`: exact match first, then the alphabetically
 * first topic the argument is a prefix of ("help sc" -> "score"). The
 * matched topic's real name is written to `resolved`. */
bool help_topic_find(const char *name, char *resolved, size_t resolved_size,
                     char *body, size_t body_size);

/* Insert-or-replace a topic; `updated_by` records who saved it. */
bool help_topic_save(const char *name, const char *body, const char *updated_by);

#endif
