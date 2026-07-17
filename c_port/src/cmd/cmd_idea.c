/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "idea_repo.h"
#include "log.h"

/* `idea <text>`: anyone files a feature request -- stored with their name
 * and the date, and echoed to online immortals as a typed [IDEA] log. Bare
 * `idea` lists the outstanding requests for immortals (mortals get the
 * usage hint). `delidea <id>` (59+) removes one once it's handled. Direct
 * mirror of `bug`/`delbug` (cmd_bug.c) -- same shape, different table
 * (user: "add an idea command so a player can request new features,
 * should work the same as reporting a bug"). */
bool cmd_idea(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args == ' ')
        args++;

    if (!*args) {
        /* No text: immortals see the request list; mortals get the usage. */
        if (being_is_immortal(ch)) {
            char out[8192];
            if (idea_repo_list(out, sizeof(out), 40)) {
                char full[8300];
                snprintf(full, sizeof(full), "\r\n<c>-- Ideas --<z>\r\n%s", out);
                descriptor_page_start(d, full, 0);
            } else {
                descriptor_send(d, "No ideas are on file.\r\n");
            }
        } else {
            descriptor_send(d, "Usage: idea <describe the feature you'd like to see>\r\n");
        }
        return true;
    }

    if (idea_repo_add(ch->base.name, args)) {
        descriptor_send(d, "<g>Thank you -- your idea has been filed.<z>\r\n");
        game_log(LOG_IDEA, "%s filed an idea: %s", ch->base.name, args);
    } else {
        descriptor_send(d, "Sorry, the idea could not be filed right now.\r\n");
    }
    return true;
}

/* `delidea <id>`: remove a handled idea (59+). */
bool cmd_delidea(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    int id = 0;
    if (sscanf(args, "%d", &id) != 1 || id <= 0) {
        descriptor_send(d, "Usage: delidea <id>   (see the numbers in `idea`)\r\n");
        return true;
    }

    if (idea_repo_delete(id)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Idea #%d deleted.\r\n", id);
        descriptor_send(d, msg);
        game_log(LOG_IDEA, "%s deleted idea #%d", ch->base.name, id);
    } else {
        descriptor_send(d, "No idea has that number.\r\n");
    }
    return true;
}
