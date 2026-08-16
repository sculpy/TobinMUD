/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "news_repo.h"
#include "player_repo.h"

/* `wiznews [lines-per-page]`: level 51+ -- the immortals' news channel, read
 * exactly like `news` (whole feed, newest first, paged), but from the
 * `wiznews` table. Items that concern immortals go here (posted with
 * `edwiznews`) rather than into the public news. `wiznews archived`
 * (alias `wiznews old`) shows items older than three weeks. */
bool cmd_wiznews(descriptor_t *d, const char *args) {
    /* Optional leading `archived` (alias `old`) selects the older-than-
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

    /* Reading the LIVE feed catches this immortal up; the archive view is
     * old items only, so it must NOT move the bookmark. Same bookmark-only
     * convention as cmd_news.c's player_set_news_last_seen() call. */
    if (!archived && d->character)
        player_set_wiznews_last_seen(d->character->player_id, news_repo_max_id(true));

    char body[100000];
    if (!news_repo_recent(true, archived, body, sizeof(body), 40)) {
        descriptor_send(d, archived
            ? "There is no archived immortal news yet -- all of it is still current.\r\n"
            : "There is no immortal news yet.\r\n");
        return true;
    }

    char full[101000];
    snprintf(full, sizeof(full), "\r\n<c>=== TobinMUD Immortal News%s ===<z>\r\n%s",
             archived ? " Archive" : "", body);
    descriptor_page_start(d, full, page_size);
    return true;
}
