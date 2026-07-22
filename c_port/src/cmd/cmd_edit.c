/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

/* `edit <noun> [args]` -- unifies every standalone `ed*` command into one
 * entry point (user, 2026-07-11: "unify all ed* commands into one edit
 * command that accepts arguments for example edit room <vnum>, edit
 * object <vnum>, edit player <name>, etc. and keep the level assignments
 * for each function valid"). Each noun just forwards to the SAME
 * implementation function the old standalone command called -- no editor
 * behavior changed, only how it's reached. The dispatcher itself is
 * registered at BUILD_MIN_LEVEL (51, the lowest of any sub-editor) so
 * everyone who could reach at least one editor can reach `edit`; a noun
 * needing a HIGHER level than that (player 58+, help/news/wiznews 56+,
 * rules 59+) checks it here and refuses with the same "Huh?!" wording the
 * command table itself would have given, so nothing was quietly
 * loosened. `trigger` (added 2026-07-11, cmd_edtrigger.c), `account`
 * (added 2026-07-18, cmd_edaccount.c), and `object` (added 2026-07-22,
 * cmd_edobject.c -- the object-prototype editor, not the trigger target
 * type of the same name) are nouns that ISN'T a folded-in old command --
 * new editors added straight into this dispatcher rather than getting
 * their own standalone verb first. `mob` is still reserved in the usage
 * line for the day `edmobile` exists (see TODO.md) -- not wired to
 * anything yet. */
bool cmd_edit(descriptor_t *d, const char *args) {
    while (*args == ' ')
        args++;

    char noun[32];
    int n = sscanf(args, "%31s", noun);
    if (n != 1) {
        descriptor_send(d,
            "Usage: edit <room|zone|object|player|account|help|news|wiznews|rules|social|trigger> [args]\r\n");
        return true;
    }

    const char *rest = args + strlen(noun);
    while (*rest == ' ')
        rest++;

    int level = d->character ? d->character->progress.level : 0;

    if (strcasecmp(noun, "room") == 0)
        return cmd_edroom(d, rest);
    if (strcasecmp(noun, "zone") == 0)
        return cmd_edzone(d, rest);
    if (strcasecmp(noun, "object") == 0)
        return cmd_edobject(d, rest);
    if (strcasecmp(noun, "trigger") == 0)
        return cmd_edtrigger(d, rest);

    if (strcasecmp(noun, "player") == 0) {
        if (level < EDPLAYER_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        return cmd_edplayer(d, rest);
    }
    if (strcasecmp(noun, "account") == 0) {
        if (level < EDACCOUNT_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        return cmd_edaccount(d, rest);
    }
    if (strcasecmp(noun, "help") == 0) {
        if (level < HELP_EDIT_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        return cmd_hedit(d, rest);
    }
    if (strcasecmp(noun, "news") == 0) {
        if (level < ADDNEWS_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        return cmd_addnews(d, rest);
    }
    if (strcasecmp(noun, "wiznews") == 0) {
        if (level < ADDNEWS_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        return cmd_edwiznews(d, rest);
    }
    if (strcasecmp(noun, "rules") == 0) {
        if (level < EDRULES_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        return cmd_edrules(d, rest);
    }
    if (strcasecmp(noun, "social") == 0) {
        if (level < EDSOCIAL_MIN_LEVEL) {
            descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
            return true;
        }
        return cmd_edsocial(d, rest);
    }

    descriptor_send(d,
        "Usage: edit <room|zone|object|player|account|help|news|wiznews|rules|social> [args]\r\n");
    return true;
}
