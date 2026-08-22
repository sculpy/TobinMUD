/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "news_repo.h"

/* `edit news <headline>`: level 56+ -- post a news item, or re-edit an
 * existing one in place if the headline exactly matches one already posted
 * (existing body preloaded, same "existing text below" convention as
 * `edit help`/cmd_hedit.c). The body is typed into the shared line editor
 * ('.' saves, '~' aborts, '/clear' wipes), and on save is written to the
 * `news` table (author = the poster's name) via EDIT_NEWS in descriptor.c's
 * editor -- news_repo_upsert() overwrites in place rather than failing on
 * the duplicate title, unlike a fresh headline. Read back with `news`.
 * `edit news delete <headline>`: removes an item outright ("News
 * follow-ups", user 2026-07-17 batch: "edit/delete existing news in-game
 * (addnews only creates)"). */
bool cmd_addnews(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    while (*args == ' ')
        args++;

    if (!args[0]) {
        descriptor_send(d,
            "Usage: edit news <headline>\r\n"
            "       edit news delete <headline>\r\n"
            "Then type the story; /s saves, /a aborts, /b blanks, "
            "/f reflows to width.\r\n");
        return true;
    }

    if (strncasecmp(args, "delete ", 7) == 0) {
        const char *headline = args + 7;
        while (*headline == ' ')
            headline++;
        if (!*headline) {
            descriptor_send(d, "Usage: edit news delete <headline>\r\n");
            return true;
        }
        if (news_repo_delete(false, headline))
            descriptor_send(d, "News item deleted.\r\n");
        else
            descriptor_send(d, "No news item has that exact headline.\r\n");
        return true;
    }

    snprintf(d->news_title, sizeof(d->news_title), "%s", args);
    d->edit_buf[0] = '\0';
    d->edit_len = 0;

    char existing[HELP_BODY_MAX];
    bool exists = news_repo_load(false, d->news_title, existing, sizeof(existing));
    if (exists) {
        snprintf(d->edit_buf, sizeof(d->edit_buf), "%s", existing);
        d->edit_len = (int)strlen(d->edit_buf);
    }

    char head[320];
    snprintf(head, sizeof(head),
        "\r\n-- Writing news: \"%s\" (%s) --\r\n"
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
    d->edit_kind = EDIT_NEWS;
    return true;
}
