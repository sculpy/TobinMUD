/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* `addnews <headline>`: level 56+ -- post a news item. The headline is the
 * argument; the body is then typed into the shared line editor ('.' saves,
 * '~' aborts, '/clear' wipes), and on save is written to the `news` table
 * (author = the poster's name) via EDIT_NEWS in descriptor.c's editor.
 * Read back with the `news` command. */
bool cmd_addnews(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    if (!args || !args[0]) {
        descriptor_send(d,
            "Usage: ednews <headline>\r\n"
            "Then type the story; '.' saves, '~' aborts, '/clear' wipes.\r\n");
        return true;
    }

    snprintf(d->news_title, sizeof(d->news_title), "%s", args);
    d->edit_buf[0] = '\0';
    d->edit_len = 0;

    char head[320];
    snprintf(head, sizeof(head),
        "\r\n-- Writing news: \"%s\" --\r\n"
        "Type the story. '.' saves, '~' aborts, '/clear' wipes the buffer.\r\n] ",
        d->news_title);
    descriptor_send(d, head);
    d->edit_kind = EDIT_NEWS;
    return true;
}
