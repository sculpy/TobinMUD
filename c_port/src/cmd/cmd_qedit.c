/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj_repo.h"
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
/* `questitem <name> <stage> <race> <vnum>` (per-race quest-item tables,
 * Sneezy -> Tobin feature audit -- disclosed NOT a port, see
 * quest_repo.h's doc comment). Builder tier, same as `qedit`. `<race>` is
 * a race name (human/elf/ogre/dwarf/hobbit/gnome), not a raw number --
 * unlike qedit's numeric stage, a builder authoring six rows in a row for
 * the same quest+stage benefits far more from a name than from having to
 * remember player_race_t's ordinal order. `<vnum>` must be a real,
 * loadable object proto (obj_create_from_proto() would otherwise hand a
 * player nothing at claim time) -- checked here up front instead of
 * failing silently later in `quest claim`. */
static bool race_name_to_enum(const char *name, player_race_t *out) {
    for (int i = 0; i < RACE_COUNT; i++) {
        if (strcasecmp(name, race_name((player_race_t)i)) == 0) {
            *out = (player_race_t)i;
            return true;
        }
    }
    return false;
}
bool cmd_questitem(descriptor_t *d, const char *args) {
    char name[QUEST_NAME_LEN];
    int stage = 0;
    char race_tok[32];
    int vnum = 0;
    if (sscanf(args, "%63s %d %31s %d", name, &stage, race_tok, &vnum) != 4) {
        descriptor_send(d, "Usage: questitem <name> <stage> <race> <vnum>\r\n");
        return true;
    }
    if (stage <= 0) {
        descriptor_send(d, "Stage must be a positive number.\r\n");
        return true;
    }
    player_race_t race;
    if (!race_name_to_enum(race_tok, &race)) {
        descriptor_send(d, "Unknown race. Try: human, elf, ogre, dwarf, hobbit, gnome.\r\n");
        return true;
    }
    obj_proto_t proto_check;
    if (!obj_proto_load(vnum, &proto_check)) {
        char msg[64];
        snprintf(msg, sizeof(msg), "No such object vnum %d.\r\n", vnum);
        descriptor_send(d, msg);
        return true;
    }
    if (!quest_repo_reward_set(name, stage, race, vnum)) {
        descriptor_send(d, "Save failed -- the DB rejected it.\r\n");
        return true;
    }
    char msg[160];
    snprintf(msg, sizeof(msg), "Quest \"%s\" stage %d reward for %s set to vnum %d.\r\n",
             name, stage, race_name(race), vnum);
    descriptor_send(d, msg);
    return true;
}
