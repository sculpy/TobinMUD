/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "being.h"
#include "quest_repo.h"

/* `qedit <name> <stage> <description...>` (Sneezy → Tobin feature
 * audit, "Quest system", builder tier). Writes/replaces the description a
 * player sees at `quest <name>` once `set <player> quest <name> <stage>`
 * (cmd_set.c) puts them at that stage. No menu editor -- same "no in-game
 * editor for it yet" precedent as several other content types in this
 * codebase (tobin_migrations.sql); re-running `qedit` with corrected
 * text overwrites the old description outright. */
bool cmd_qedit(descriptor_t *d, const char *args) {
    char name[QUEST_NAME_LEN];
    int stage = 0;
    int consumed = 0;

    if (sscanf(args, "%63s %d%n", name, &stage, &consumed) != 2) {
        descriptor_send(d, "Usage: qedit <name> <stage> <description>\r\n");
        return true;
    }
    const char *desc = args + consumed;
    while (*desc == ' ')
        desc++;
    if (!*desc) {
        descriptor_send(d, "Usage: qedit <name> <stage> <description>\r\n");
        return true;
    }
    if (stage <= 0) {
        descriptor_send(d, "Stage must be a positive number (0 means \"not started\").\r\n");
        return true;
    }

    char msg[128];
    if (!quest_repo_def_set(name, stage, desc)) {
        descriptor_send(d, "Save failed -- the DB rejected it.\r\n");
        return true;
    }
    snprintf(msg, sizeof(msg), "Quest \"%s\" stage %d description set.\r\n", name, stage);
    descriptor_send(d, msg);
    return true;
}
