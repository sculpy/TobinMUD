/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "news_repo.h"

/* `edit wiznews <headline>`: level 56+ -- post an item to the immortal news
 * channel (read with `wiznews`), or re-edit an existing one in place if the
 * headline exactly matches one already posted (existing body preloaded --
 * same treatment as `edit news`/cmd_addnews.c, same underlying gap: "addnews
 * only creates" applied identically to wiznews). Saved to the `wiznews`
 * table via EDIT_WIZNEWS in descriptor.c's editor. `edit wiznews delete
 * <headline>` removes an item outright. For news that concerns immortals
 * rather than the general playerbase. */
bool cmd_edwiznews(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    while (*args == ' ')
        args++;

    if (!args[0]) {
        descriptor_send(d,
            "Usage: edit wiznews <headline>\r\n"
            "       edit wiznews delete <headline>\r\n"
            "Then type the story; /s saves, /a aborts, /b blanks, "
            "/f reflows to width.\r\n");
        return true;
    }

    if (strncasecmp(args, "delete ", 7) == 0) {
        const char *headline = args + 7;
        while (*headline == ' ')
            headline++;
        if (!*headline) {
            descriptor_send(d, "Usage: edit wiznews delete <headline>\r\n");
            return true;
        }
        if (news_repo_delete(true, headline))
            descriptor_send(d, "Immortal news item deleted.\r\n");
        else
            descriptor_send(d, "No immortal news item has that exact headline.\r\n");
        return true;
    }

    snprintf(d->news_title, sizeof(d->news_title), "%s", args);
    d->edit_buf[0] = '\0';
    d->edit_len = 0;

    char existing[HELP_BODY_MAX];
    bool exists = news_repo_load(true, d->news_title, existing, sizeof(existing));
    if (exists) {
        snprintf(d->edit_buf, sizeof(d->edit_buf), "%s", existing);
        d->edit_len = (int)strlen(d->edit_buf);
    }

    char head[320];
    snprintf(head, sizeof(head),
        "\r\n-- Writing immortal news: \"%s\" (%s) --\r\n"
        "Type lines to append. /s saves, /a aborts, /b blanks, "
        "/f reflows to width.\r\n",
        d->news_title, exists ? "existing text below" : "new");
    descriptor_send(d, head);
    if (exists) {
        descriptor_send(d, existing);
        if (existing[0] && existing[strlen(existing) - 1] != '\n')
            descriptor_send(d, "\r\n");
    }
    descriptor_send(d, "] ");
    d->edit_kind = EDIT_WIZNEWS;
    return true;
}
