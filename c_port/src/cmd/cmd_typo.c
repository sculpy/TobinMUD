/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "log.h"
#include "typo_repo.h"

/* `typo <text>`: anyone reports a typo or other text problem -- stored with
 * their name, the date, and the room they were standing in, and echoed to
 * online immortals as a typed [TYPO] log. Bare `typo` lists the outstanding
 * reports for immortals (mortals get the usage hint). `deltypo <id>` (59+)
 * removes one once it's handled. Direct mirror of `bug`/`idea`
 * (cmd_bug.c/cmd_idea.c) -- same shape, different table (user: "add a typo
 * command in the same way"). */
bool cmd_typo(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args == ' ')
        args++;

    if (!*args) {
        /* No text: immortals see the report list; mortals get the usage. */
        if (being_is_immortal(ch)) {
            char out[8192];
            if (typo_repo_list(out, sizeof(out), 40)) {
                char full[8300];
                snprintf(full, sizeof(full), "\r\n<c>-- Typo reports --<z>\r\n%s", out);
                descriptor_page_start(d, full, 0);
            } else {
                descriptor_send(d, "No typo reports are on file.\r\n");
            }
        } else {
            descriptor_send(d, "Usage: typo <what's misspelled/wrong, and where>\r\n");
        }
        return true;
    }

    int room_vnum = ch->base.roomp ? ch->base.roomp->vnum : 0;
    if (typo_repo_add(ch->base.name, args, room_vnum)) {
        descriptor_send(d, "<g>Thank you -- your typo report has been filed.<z>\r\n");
        game_log(LOG_TYPO, "%s filed a typo report: %s", ch->base.name, args);
    } else {
        descriptor_send(d, "Sorry, the typo report could not be filed right now.\r\n");
    }
    return true;
}

/* `deltypo <id>`: remove a handled typo report (59+). */
bool cmd_deltypo(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    int id = 0;
    if (sscanf(args, "%d", &id) != 1 || id <= 0) {
        descriptor_send(d, "Usage: deltypo <id>   (see the numbers in `typo`)\r\n");
        return true;
    }

    if (typo_repo_delete(id)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Typo report #%d deleted.\r\n", id);
        descriptor_send(d, msg);
        game_log(LOG_TYPO, "%s deleted typo report #%d", ch->base.name, id);
    } else {
        descriptor_send(d, "No typo report has that number.\r\n");
    }
    return true;
}
