/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "news_repo.h"
#include "player_repo.h"

/* `news [lines-per-page]`: available to everyone -- shows the whole news feed,
 * newest first, a page at a time (the descriptor's pager). An optional number
 * sets the page size (e.g. news 10 / 20 / 50 / 100); default 20. Player-facing
 * announcements of feature/command/world changes, DB-backed (the `news` table,
 * news.sql). See the house rule in news.sql: an entry is added for every
 * player-affecting change, with no numbers in the text. `news archived`
 * (alias `news old`) shows items older than three weeks, which drop out of
 * the current feed; a page size may follow either form (e.g. news archived 50). */
bool cmd_news(descriptor_t *d, const char *args) {
    /* An optional leading `archived` (alias `old`) selects the older-than-
     * three-weeks feed; a page-size number may follow either form. */
    bool archived = false;
    int page_size = 20;
    if (args && args[0]) {
        while (*args == ' ')
            args++;
        if (strncasecmp(args, "archiv", 6) == 0 || strncasecmp(args, "old", 3) == 0) {
            archived = true;
            while (*args && *args != ' ')
                args++;
            while (*args == ' ')
                args++;
        }
        int n = atoi(args);
        if (n > 0)
            page_size = n;
    }
    if (page_size > 100)
        page_size = 100;
    if (page_size < 5)
        page_size = 5;

    /* Reading the LIVE feed catches this player up -- clears the login
     * "there's new news" notice (descriptor.c) until something newer is
     * posted. The archive view is old items only, so it must NOT move the
     * bookmark. The id itself is never shown to the player (house rule: no
     * numbers in news text), only used internally as a bookmark. */
    if (!archived && d->character)
        player_set_news_last_seen(d->character->player_id, news_repo_max_id(false));

    /* Sized generously (same fix as cmd_wiznews.c, found via
     * smoke_test_wiznews.py): the pager chunks display separately, so
     * there's no reason to keep this tight. */
    char body[100000];
    if (!news_repo_recent(false, archived, body, sizeof(body), 40)) {
        descriptor_send(d, archived
            ? "There is no archived news yet -- all of it is still current.\r\n"
            : "There is no news yet.\r\n");
        return true;
    }

    /* No numbers rendered anywhere (user rule) -- ordering conveys recency. */
    char full[101000];
    snprintf(full, sizeof(full), "\r\n<c>=== TobinMUD News%s ===<z>\r\n%s",
             archived ? " Archive" : "", body);
    descriptor_page_start(d, full, page_size);
    return true;
}
