/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "quest_repo.h"

/* `quest [<name>]` (Sneezy → Tobin feature audit, "Quest system").
 * Checked Sneezy's own doMortalQuest() first: bare `quest` counts/lists
 * active quests (only bits with a help file are shown), `quest N` pages
 * that bit's help file. Same shape here, quest_def_get() standing in for
 * the help-file check. No actual quest content ships with this (user,
 * AskUserQuestion -- see quest_repo.h) -- an empty list is the expected,
 * honest state until an immortal authors something with `questdef`. */
bool cmd_quest(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (*args) {
        char name[QUEST_NAME_LEN];
        snprintf(name, sizeof(name), "%s", args);
        int stage = quest_repo_get_stage(ch->player_id, name);
        if (stage <= 0) {
            descriptor_send(d, "You aren't currently on that quest.\r\n");
            return true;
        }
        char desc[1024];
        if (!quest_repo_def_get(name, stage, desc, sizeof(desc))) {
            descriptor_send(d, "You aren't currently on that quest.\r\n");
            return true;
        }
        char out[1152];
        snprintf(out, sizeof(out), "%s\r\n", desc);
        descriptor_page_start(d, out, 0);
        return true;
    }

    quest_entry_t entries[64];
    int total = quest_repo_list_player(ch->player_id, entries, 64);

    char out[2048];
    int n = snprintf(out, sizeof(out), "Your current quests:\r\n");
    bool any = false;
    for (int i = 0; i < total && (size_t)n < sizeof(out); i++) {
        char desc[256];
        if (!quest_repo_def_get(entries[i].name, entries[i].stage, desc, sizeof(desc)))
            continue; /* no description written yet -- invisible, same as the original */
        any = true;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s\r\n", entries[i].name);
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  None.\r\n");

    descriptor_page_start(d, out, 0);
    return true;
}
