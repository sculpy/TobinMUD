/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "board_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

/* Lists non-removed posts on a board (post_num, date, subject, author) in
 * post_num order, up to max entries -- backs the board's post-listing
 * display. */
int board_repo_list(int board_vnum, board_post_summary_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db,
            "select post_num, date_posted, subject, author from board_message "
            "where board_vnum=%i and date_removed is null order by post_num asc limit %i",
            board_vnum, max)) {
        while (n < max && db_fetch_row(db)) {
            out[n].post_num = atoi(db_get(db, "post_num"));
            snprintf(out[n].date_posted, sizeof(out[n].date_posted), "%s", db_get(db, "date_posted"));
            snprintf(out[n].subject, sizeof(out[n].subject), "%s", db_get(db, "subject"));
            snprintf(out[n].author, sizeof(out[n].author), "%s", db_get(db, "author"));
            n++;
        }
    }

    db_close(db);
    return n;
}

bool board_repo_read(int board_vnum, int post_num, char *subject, size_t subj_sz,
                      char *author, size_t auth_sz, char *body, size_t body_sz,
                      char *date, size_t date_sz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db,
            "select date_posted, subject, author, post from board_message "
            "where board_vnum=%i and post_num=%i and date_removed is null",
            board_vnum, post_num)
        && db_fetch_row(db)) {
        snprintf(subject, subj_sz, "%s", db_get(db, "subject"));
        snprintf(author, auth_sz, "%s", db_get(db, "author"));
        snprintf(body, body_sz, "%s", db_get(db, "post"));
        snprintf(date, date_sz, "%s", db_get(db, "date_posted"));
        found = true;
    }

    db_close(db);
    return found;
}

/* Adds a new post to a board, auto-assigning the next post_num. */
bool board_repo_post(int board_vnum, const char *subject, const char *author, const char *body) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* Same "count(*) + 1" post_num assignment as the original's postMe()
     * insert (obj_board.cc) -- a single-statement select/insert so a
     * concurrent post on the same board can't race two posts onto the
     * same number (the count is read and the row inserted in one trip). */
    bool ok = db_query(db,
        "insert into board_message (board_vnum, subject, author, post, post_num) "
        "select %i, '%s', '%s', '%s', count(*) + 1 from board_message "
        "where board_vnum=%i and date_removed is null",
        board_vnum, subject, author, body, board_vnum);

    db_close(db);
    return ok;
}
