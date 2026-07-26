/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "news_repo.h"
#include "player_repo.h"

/* `wiznews [lines-per-page]`: level 51+ -- the immortals' news channel, read
 * exactly like `news` (whole feed, newest first, paged), but from the
 * `wiznews` table. Items that concern immortals go here (posted with
 * `edwiznews`) rather than into the public news. */
bool cmd_wiznews(descriptor_t *d, const char *args) {
    int page_size = 20;
    if (args && args[0]) {
        int n = atoi(args);
        if (n > 0)
            page_size = n;
    }
    if (page_size > 100)
        page_size = 100;
    if (page_size < 5)
        page_size = 5;

    /* Reading the feed catches this immortal up -- clears the login
     * "there's new wiznews" notice (descriptor.c) until something newer
     * is posted. Same bookmark-only convention as cmd_news.c's own
     * player_set_news_last_seen() call. */
    if (d->character)
        player_set_wiznews_last_seen(d->character->player_id, news_repo_max_id(true));

    /* Sized generously (found truncating silently at the old 15000/16000,
     * user 2026-07-11 bug report via smoke_test_wiznews.py: with 40 items
     * of real body text this feed keeps growing every session forever, so
     * a "just big enough for today" buffer was destined to be hit again --
     * the pager already chunks display separately, so there's no reason
     * to keep this tight). */
    char body[100000];
    if (!news_repo_recent(true, body, sizeof(body), 40)) {
        descriptor_send(d, "There is no immortal news yet.\r\n");
        return true;
    }

    char full[101000];
    snprintf(full, sizeof(full), "\r\n<c>=== TobinMUD Immortal News ===<z>\r\n%s", body);
    descriptor_page_start(d, full, page_size);
    return true;
}
