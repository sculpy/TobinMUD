/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_NEWS_REPO_H
#define TOBIN_NEWS_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* Renders the most recent `limit` items (newest first) from the news channel
 * into `out` as ready-to-send text (title / body / author per item). `wiz`
 * selects the immortal `wiznews` table instead of the public `news` table.
 * Returns false if there are no items. */
bool news_repo_recent(bool wiz, char *out, size_t size, int limit);

/* Inserts an item into the news (`wiz` false) or wiznews (`wiz` true) channel.
 * `title` is UNIQUE per channel -- returns false on a duplicate headline (or
 * DB error). Backs the in-game `ednews` / `edwiznews` commands. */
bool news_repo_add(bool wiz, const char *author, const char *title, const char *body);

#endif
