/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* `edwiznews <headline>`: level 56+ -- post an item to the immortal news
 * channel (read with `wiznews`). Same flow as `ednews` (headline arg + story
 * in the shared line editor), but saved to the `wiznews` table via
 * EDIT_WIZNEWS in descriptor.c's editor. For news that concerns immortals
 * rather than the general playerbase. */
bool cmd_edwiznews(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    if (!args || !args[0]) {
        descriptor_send(d,
            "Usage: edwiznews <headline>\r\n"
            "Then type the story; '.' saves, '~' aborts, '/clear' wipes, "
            "'/format' reflows to width.\r\n");
        return true;
    }

    snprintf(d->news_title, sizeof(d->news_title), "%s", args);
    d->edit_buf[0] = '\0';
    d->edit_len = 0;

    char head[320];
    snprintf(head, sizeof(head),
        "\r\n-- Writing immortal news: \"%s\" --\r\n"
        "Type the story. '.' saves, '~' aborts, '/clear' wipes, "
        "'/format' reflows to width.\r\n] ",
        d->news_title);
    descriptor_send(d, head);
    d->edit_kind = EDIT_WIZNEWS;
    return true;
}
