/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

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
                descriptor_send(d, "\r\n<c>-- Bug reports --<z>\r\n");
                descriptor_send(d, out);
            } else {
                descriptor_send(d, "No bug reports are on file.\r\n");
            }
        } else {
            descriptor_send(d, "Usage: bug <description of the problem>\r\n");
        }
        return true;
    }

    if (bug_repo_add(ch->base.name, args)) {
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
