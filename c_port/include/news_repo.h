/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_NEWS_REPO_H
#define TOBIN_NEWS_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* Renders up to `limit` items (newest first) from the news channel into
 * `out` as ready-to-send text (title / body / author per item). `wiz`
 * selects the immortal `wiznews` table instead of the public `news` table.
 * `archived` splits the feed by age at a three-week cutoff: false yields
 * the current items (posted within the last three weeks), true yields the
 * older, archived ones. Returns false if there are no matching items. */
bool news_repo_recent(bool wiz, bool archived, char *out, size_t size, int limit);

/* Inserts an item into the news (`wiz` false) or wiznews (`wiz` true) channel,
 * or -- since `title` is UNIQUE per channel -- overwrites the body/author of
 * an existing item with that exact title in place (an in-game re-edit, NOT
 * the same no-op upsert news.sql/wiznews.sql use for idempotent reseeding).
 * Backs the in-game `edit news` / `edit wiznews` commands. Returns false only
 * on a DB error. */
bool news_repo_upsert(bool wiz, const char *author, const char *title, const char *body);

/* Loads the body of an existing item by exact title match, for preloading
 * into the editor when re-editing (mirrors help_topic_load_exact). Returns
 * false if no such title exists in that channel. */
bool news_repo_load(bool wiz, const char *title, char *out_body, size_t size);

/* Deletes an item by exact title match. Returns false if no such title
 * exists (or on DB error). */
bool news_repo_delete(bool wiz, const char *title);

/* Highest `id` currently in the channel (0 if it's empty). Used to mark a
 * player as caught-up (player_repo.h's news_last_seen_id) without ever
 * showing them a raw id or count (house rule: no numbers in news text). */
long news_repo_max_id(bool wiz);

#endif
