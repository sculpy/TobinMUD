/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BOARD_REPO_H
#define TOBIN_BOARD_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* DB access for bulletin boards (`read`/`write`, cmd_board.c) -- Sneezy
 * port (user 2026-07-18: "we need to make bulletin boards function, read
 * and write commands, from sneezy"). The `board_message` table is real
 * upstream schema (db/tobin/board_message.sql, already imported, FK'd to
 * `obj.vnum`) -- no migration needed, just this read/write layer on top.
 *
 * Scoped DOWN from the original's TBoard mechanic in one way: posting is
 * direct (`write <subject> <body>` inserts straight to the board), not a
 * two-step "write a note object, then post the note" flow -- Tobin has no
 * separate ITEM_NOTE writable-object system yet, and building one just to
 * immediately consume it on `post` would be pure overhead for the same
 * end result. Reading, numbering, and the per-board `val[0]` level gate
 * (see cmd_board.c) are faithful to the original. Removing/pulling a post
 * back off the board (the original's `get <#>`) is NOT included -- out of
 * scope for a v1 read+write pair; the DB schema's `date_removed` column
 * is ready for it whenever that's wanted. */

#define BOARD_SUBJECT_LEN 80
#define BOARD_AUTHOR_LEN 80
#define BOARD_POST_LEN 1024
#define BOARD_DATE_LEN 32

typedef struct {
    int post_num;
    char date_posted[BOARD_DATE_LEN]; /* raw DB timestamp string */
    char subject[BOARD_SUBJECT_LEN];
    char author[BOARD_AUTHOR_LEN];
} board_post_summary_t;

/* Matches the original's own per-board cap (MAX_MSGS, obj_board.h) --
 * never actually enforced upstream (commented out in postToBoard()), kept
 * here only as the summary-listing array size, not an enforced limit. */
#define BOARD_LIST_MAX 100

/* Fills `out` (capacity `max`) with every live (not date_removed) post on
 * `board_vnum`, oldest first (matches post_num order) -- the `read` (no
 * arg) listing. Returns how many were found (0..max). */
int board_repo_list(int board_vnum, board_post_summary_t *out, int max);

/* Loads one specific post's full body + byline for `read <#>`. False if
 * that post_num doesn't exist (or was removed) on this board. */
bool board_repo_read(int board_vnum, int post_num, char *subject, size_t subj_sz,
                      char *author, size_t auth_sz, char *body, size_t body_sz,
                      char *date, size_t date_sz);

/* Appends a new post to `board_vnum` at the next post_num (count of live
 * posts + 1, same "select count(*)+1" pattern the original's postMe()
 * insert used) -- `write <subject> <body>`. */
bool board_repo_post(int board_vnum, const char *subject, const char *author, const char *body);

#endif
