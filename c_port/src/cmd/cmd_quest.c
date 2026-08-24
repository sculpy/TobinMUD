/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj.h"
#include "obj_repo.h"
#include "quest_repo.h"
#include "thing.h"

/* `quest [<name>]` (Sneezy → Tobin feature audit, "Quest system").
 * Checked Sneezy's own doMortalQuest() first: bare `quest` counts/lists
 * active quests (only bits with a help file are shown), `quest N` pages
 * that bit's help file. Same shape here, quest_def_get() standing in for
 * the help-file check. No actual quest content ships with this (user,
 * AskUserQuestion -- see quest_repo.h) -- an empty list is the expected,
 * honest state until an immortal authors something with `questdef`. */
/* `quest claim <name>` (per-race quest-item tables, Sneezy -> Tobin
 * feature audit -- see quest_repo.h's doc comment for the disclosed
 * "not a port" scope). Grants the caller's race's reward item for their
 * CURRENT stage of `name`, once -- quest_repo_reward_claimed() blocks a
 * repeat claim, and nothing is marked claimed unless the object was
 * actually created and moved into inventory. Silent, specific failure
 * messages rather than folding this into the generic "aren't on that
 * quest" path, since there are three different reasons this can fail
 * (not on the quest, no reward defined for this race/stage, already
 * claimed) and a player should know which. */
static bool cmd_quest_claim(descriptor_t *d, being_t *ch, const char *name) {
    int stage = quest_repo_get_stage(ch->player_id, name);
    if (stage <= 0) {
        descriptor_send(d, "You aren't currently on that quest.\r\n");
        return true;
    }
    if (quest_repo_reward_claimed(ch->player_id, name, stage)) {
        descriptor_send(d, "You've already claimed your reward for that quest stage.\r\n");
        return true;
    }
    int vnum = quest_repo_reward_item(name, stage, ch->race);
    if (vnum < 0) {
        descriptor_send(d, "There's no reward item for your race at this stage.\r\n");
        return true;
    }
    obj_t *o = obj_create_from_proto(vnum);
    if (!o) {
        descriptor_send(d, "Something went wrong creating your reward -- report this.\r\n");
        return true;
    }
    thing_move_to(&o->base, &ch->base);
    player_inventory_save(ch->player_id, ch);
    quest_repo_reward_mark_claimed(ch->player_id, name, stage);
    char msg[192];
    snprintf(msg, sizeof(msg), "You claim your reward: %s.\r\n",
             o->base.short_descr[0] ? o->base.short_descr : o->base.name);
    descriptor_send(d, msg);
    return true;
}
bool cmd_quest(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (strncasecmp(args, "claim", 5) == 0 && (args[5] == '\0' || args[5] == ' ')) {
        const char *rest = args + 5;
        while (*rest == ' ')
            rest++;
        if (!*rest) {
            descriptor_send(d, "Usage: quest claim <name>\r\n");
            return true;
        }
        char name[QUEST_NAME_LEN];
        snprintf(name, sizeof(name), "%s", rest);
        return cmd_quest_claim(d, ch, name);
    }

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
