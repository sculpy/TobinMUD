/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "bug_repo.h"
#include "log.h"

/* `bug <text>`: anyone files a bug report -- stored with their name and the
 * date, and echoed to online immortals as a typed [BUG] log. Bare `bug`
 * lists the outstanding reports for immortals (mortals get the usage hint).
 * `delbug <id>` (59+) removes one once it's handled. */
bool cmd_bug(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args == ' ')
        args++;

    if (!*args) {
        /* No text: immortals see the report list; mortals get the usage. */
        if (being_is_immortal(ch)) {
            char out[8192];
            if (bug_repo_list(out, sizeof(out), 40)) {
                char full[8300];
                snprintf(full, sizeof(full), "\r\n<c>-- Bug reports --<z>\r\n%s", out);
                descriptor_page_start(d, full, 0);
            } else {
                descriptor_send(d, "No bug reports are on file.\r\n");
            }
        } else {
            descriptor_send(d, "Usage: bug <description of the problem>\r\n");
        }
        return true;
    }

    int room_vnum = ch->base.roomp ? ch->base.roomp->vnum : 0;
    if (bug_repo_add(ch->base.name, args, room_vnum)) {
        descriptor_send(d, "<g>Thank you -- your bug report has been filed.<z>\r\n");
        game_log(LOG_BUG, "%s filed a bug: %s", ch->base.name, args);
    } else {
        descriptor_send(d, "Sorry, the bug could not be filed right now.\r\n");
    }
    return true;
}

/* `delbug <id>`: remove a handled bug report (59+). */
bool cmd_delbug(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    int id = 0;
    if (sscanf(args, "%d", &id) != 1 || id <= 0) {
        descriptor_send(d, "Usage: delbug <id>   (see the numbers in `bug`)\r\n");
        return true;
    }

    if (bug_repo_delete(id)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Bug #%d deleted.\r\n", id);
        descriptor_send(d, msg);
        game_log(LOG_BUG, "%s deleted bug #%d", ch->base.name, id);
    } else {
        descriptor_send(d, "No bug report has that number.\r\n");
    }
    return true;
}

/* `edbug <id> [note]` (59+, TODO.md-planned): resolve a filed bug IN PLACE
 * instead of only being able to `delbug` (delete) it outright, so the
 * submitter can be told it was actually fixed. If they're online right
 * now, they get a live notice; either way the report itself is kept
 * (marked resolved_at + resolution, bug_repo.c), just no longer shown in
 * the outstanding `bug` list. */
bool cmd_edbug(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    int id = 0;
    int consumed = 0;
    if (sscanf(args, "%d %n", &id, &consumed) < 1 || id <= 0) {
        descriptor_send(d, "Usage: edbug <id> [note for the submitter]   (see the numbers in `bug`)\r\n");
        return true;
    }
    const char *note = args + consumed;

    char submitter[64], body[1024];
    if (!bug_repo_get(id, submitter, sizeof(submitter), body, sizeof(body))) {
        descriptor_send(d, "No bug report has that number.\r\n");
        return true;
    }

    if (!bug_repo_resolve(id, note)) {
        descriptor_send(d, "That bug is already resolved (or couldn't be updated).\r\n");
        return true;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "Bug #%d marked resolved.\r\n", id);
    descriptor_send(d, msg);
    game_log(LOG_BUG, "%s resolved bug #%d (%s): %s", ch->base.name, id, submitter,
             note[0] ? note : "(no note)");

    /* Live notice if the submitter happens to be online right now. */
    size_t slen = strlen(submitter);
    for (descriptor_t *it = g_descriptors; it && slen; it = it->next) {
        if (!it->character || !it->character->base.roomp)
            continue;
        if (strncasecmp(it->character->base.name, submitter, slen) != 0)
            continue;
        char notice[1200];
        if (note[0])
            snprintf(notice, sizeof(notice),
                     "<g>Your bug report (#%d: %s) has been resolved: %s<z>\r\n",
                     id, body, note);
        else
            snprintf(notice, sizeof(notice),
                     "<g>Your bug report (#%d: %s) has been resolved.<z>\r\n", id, body);
        descriptor_send(it, notice);
        break;
    }
    return true;
}
