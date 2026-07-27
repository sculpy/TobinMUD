/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "tips_repo.h"

/* `tips`: shows one random tip on demand (mortal), on top of the
 * periodic pulse-driven echo (tips_repo.c) newbie-flagged connections
 * already get automatically. */
bool cmd_tips(descriptor_t *d, const char *args) {
    (void)args;
    char tip[512];
    if (!tips_repo_random(tip, sizeof(tip))) {
        descriptor_send(d, "No tips on file yet.\r\n");
        return true;
    }
    char msg[560];
    snprintf(msg, sizeof(msg), "<c>Tip:<z> %s\r\n", tip);
    descriptor_send(d, msg);
    return true;
}

/* `tipedit <text>` / `tipedit list` / `tipedit delete <id>` (53+) --
 * same flat add/list/delete dispatch shape as `bug`/`delbug`, not a
 * menu editor (news/help's own editors are for long-form titled
 * content; a tip is one short sentence). */
bool cmd_tipedit(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (!args || !args[0]) {
        descriptor_send(d, "Usage: tipedit <text>   |   tipedit list   |   tipedit delete <id>\r\n");
        return true;
    }

    char first[16] = "";
    int consumed = 0;
    sscanf(args, "%15s %n", first, &consumed);

    if (strcasecmp(first, "list") == 0) {
        char out[4096];
        if (tips_repo_list(out, sizeof(out))) {
            char full[4200];
            snprintf(full, sizeof(full), "\r\n<c>-- Tips --<z>\r\n%s", out);
            descriptor_page_start(d, full, 0);
        } else {
            descriptor_send(d, "No tips on file yet.\r\n");
        }
        return true;
    }

    if (strcasecmp(first, "delete") == 0) {
        int id = atoi(args + consumed);
        if (id <= 0) {
            descriptor_send(d, "Usage: tipedit delete <id>   (see the numbers in 'tipedit list')\r\n");
            return true;
        }
        if (tips_repo_delete(id)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Tip #%d deleted.\r\n", id);
            descriptor_send(d, msg);
        } else {
            descriptor_send(d, "No tip has that number.\r\n");
        }
        return true;
    }

    if (tips_repo_add(ch->base.name, args)) {
        descriptor_send(d, "Tip added.\r\n");
    } else {
        descriptor_send(d, "Something went wrong adding that tip.\r\n");
    }
    return true;
}
