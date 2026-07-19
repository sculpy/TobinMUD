/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "news_repo.h"
#include "player_repo.h"

/* `news [lines-per-page]`: available to everyone -- shows the whole news feed,
 * newest first, a page at a time (the descriptor's pager). An optional number
 * sets the page size (e.g. news 10 / 20 / 50 / 100); default 20. Player-facing
 * announcements of feature/command/world changes, DB-backed (the `news` table,
 * news.sql). See the house rule in news.sql: an entry is added for every
 * player-affecting change, with no numbers in the text. */
bool cmd_news(descriptor_t *d, const char *args) {
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

    /* Sized generously (same fix as cmd_wiznews.c, found via
     * smoke_test_wiznews.py: with 40 items of real body text this feed
     * keeps growing every session forever, so the old 15000/16000 was
     * silently truncating mid-entry -- the pager already chunks display
     * separately, so there's no reason to keep this tight). */
    /* Reading the feed catches this player up -- clears the login "there's
     * new news" notice (descriptor.c) until something newer is posted. The
     * id itself is never shown to the player (house rule: no numbers in
     * news text), only used internally as a bookmark. */
    if (d->character)
        player_set_news_last_seen(d->character->player_id, news_repo_max_id(false));

    char body[100000];
    if (!news_repo_recent(false, body, sizeof(body), 40)) {
        descriptor_send(d, "There is no news yet.\r\n");
        return true;
    }

    /* No numbers rendered anywhere (user rule) -- ordering conveys recency. */
    char full[101000];
    snprintf(full, sizeof(full), "\r\n<c>=== TobinMUD News ===<z>\r\n%s", body);
    descriptor_page_start(d, full, page_size);
    return true;
}
