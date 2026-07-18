/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "alias_repo.h"
#include "being.h"

/* `alias` (user 2026-07-17: "players define their own aliases, stored on
 * the account and shared across that account's characters. Scoped by
 * tier: an immortal's aliases apply only to their immortal characters; a
 * mortal's apply to all mortal characters on the account"). Actual
 * expansion happens in cmd_dispatch() (cmd_table.c); this command just
 * manages the account_alias rows (add/list/remove):
 *
 *   alias                    -- lists every alias for your tier
 *   alias <name>              -- shows that one alias's expansion
 *   alias <name> <expansion>  -- creates or overwrites an alias
 *   alias remove <name>       -- deletes an alias
 *
 * Capped at ALIAS_MAX_PER_TIER (alias_repo.h) per account per tier --
 * editing an existing alias never counts against the cap, only adding a
 * genuinely new name does. */
bool cmd_alias(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    const char *tier = being_is_immortal(d->character) ? "immortal" : "mortal";
    long account_id = d->account.account_id;

    while (*args == ' ')
        args++;

    if (!*args) {
        alias_entry_t list[ALIAS_MAX_PER_TIER];
        int count = 0;
        alias_repo_list(account_id, tier, list, ALIAS_MAX_PER_TIER, &count);
        if (count == 0) {
            descriptor_send(d, "You have no aliases yet. Usage: alias <name> <expansion>\r\n");
            return true;
        }
        char out[ALIAS_MAX_PER_TIER * (ALIAS_NAME_LEN + ALIAS_EXPANSION_LEN + 8) + 64];
        int n = snprintf(out, sizeof(out), "\r\n-- Your %s aliases --\r\n", tier);
        for (int i = 0; i < count && (size_t)n < sizeof(out); i++)
            n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s -> %s\r\n",
                          list[i].name, list[i].expansion);
        descriptor_page_start(d, out, 0);
        return true;
    }

    char verb[ALIAS_NAME_LEN];
    const char *rest = args;
    size_t vi = 0;
    while (*rest && *rest != ' ' && vi + 1 < sizeof(verb))
        verb[vi++] = (char)tolower((unsigned char)*rest++);
    verb[vi] = '\0';
    while (*rest == ' ')
        rest++;

    if (strcasecmp(verb, "remove") == 0) {
        if (!*rest) {
            descriptor_send(d, "Remove which alias? Usage: alias remove <name>\r\n");
            return true;
        }
        char target[ALIAS_NAME_LEN];
        snprintf(target, sizeof(target), "%s", rest);
        if (alias_repo_remove(account_id, tier, target)) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Alias '%s' removed.\r\n", target);
            descriptor_send(d, msg);
        } else {
            descriptor_send(d, "No such alias.\r\n");
        }
        return true;
    }

    if (!*rest) {
        /* alias <name> alone (no expansion, not "remove") -- show that
         * one alias's expansion, a small convenience beyond the
         * add/list/remove trio asked for. */
        char expansion[ALIAS_EXPANSION_LEN];
        if (alias_repo_find(account_id, tier, verb, expansion, sizeof(expansion))) {
            char msg[ALIAS_NAME_LEN + ALIAS_EXPANSION_LEN + 16];
            snprintf(msg, sizeof(msg), "%s -> %s\r\n", verb, expansion);
            descriptor_send(d, msg);
        } else {
            descriptor_send(d, "No such alias. Usage: alias <name> <expansion>\r\n");
        }
        return true;
    }

    if (!alias_repo_set(account_id, tier, verb, rest)) {
        char msg[96];
        snprintf(msg, sizeof(msg),
                 "Could not set that alias -- you may already have %d %s aliases (the max).\r\n",
                 ALIAS_MAX_PER_TIER, tier);
        descriptor_send(d, msg);
        return true;
    }

    char msg[ALIAS_NAME_LEN + ALIAS_EXPANSION_LEN + 32];
    snprintf(msg, sizeof(msg), "Alias set: %s -> %s\r\n", verb, rest);
    descriptor_send(d, msg);
    return true;
}
