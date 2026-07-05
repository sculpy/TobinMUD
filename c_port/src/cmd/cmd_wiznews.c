/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "news_repo.h"

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

    char body[15000];
    if (!news_repo_recent(true, body, sizeof(body), 40)) {
        descriptor_send(d, "There is no immortal news yet.\r\n");
        return true;
    }

    char full[16000];
    snprintf(full, sizeof(full), "\r\n<c>=== TobinMUD Immortal News ===<z>\r\n%s", body);
    descriptor_page_start(d, full, page_size);
    return true;
}
