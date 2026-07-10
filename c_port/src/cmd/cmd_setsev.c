/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "log.h"

/* `setsev` -- port of Sneezy's setsev (misc/immortal.cc doSetsev()): lets an
 * immortal opt out of specific game_log() [TAG] echoes. Bare `setsev` lists
 * every type with its current on/off state (prefix-matched, first match
 * wins, same convention as `toggle`); `setsev <type>` flips one.
 *
 * LOG_JESUS is personalized upstream too (a name-gated self-toggle) -- only
 * the immortal actually named Jesus can see or flip it here, matching that.
 *
 * Deliberately simplified vs. the original: see being.h's `severity` field
 * comment for why this is session-only, not persisted. */

typedef struct {
    log_type_t type;
    const char *name;
    const char *desc;
} setsev_entry_t;

static const setsev_entry_t SETSEV_TYPES[] = {
    { LOG_GAME,   "game",   "Generic/miscellaneous events" },
    { LOG_PIO,    "pio",    "Player login/logout/link events" },
    { LOG_COMBAT, "combat", "Combat and deaths" },
    { LOG_BUG,    "bug",    "Bug reports" },
    { LOG_IDEA,   "idea",   "Player feature requests" },
    { LOG_DB,     "db",     "Database events" },
    { LOG_EDIT,   "edit",   "In-game building/editing" },
    { LOG_TEST,   "test",   "Smoke-test announcements (@test hook)" },
    { LOG_JESUS,  "jesus",  "Personal messages for Jesus" },
};
#define NUM_SETSEV_TYPES (sizeof(SETSEV_TYPES) / sizeof(SETSEV_TYPES[0]))

static bool is_jesus(const being_t *ch) {
    return strcasecmp(ch->base.name, "Jesus") == 0;
}

bool cmd_setsev(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[32] = "";
    sscanf(args, "%31s", tok);

    if (!tok[0]) {
        char out[768];
        int n = snprintf(out, sizeof(out), "\r\n<c>-- Log Severity --<z>\r\n");
        for (size_t i = 0; i < NUM_SETSEV_TYPES && (size_t)n < sizeof(out); i++) {
            if (SETSEV_TYPES[i].type == LOG_JESUS && !is_jesus(ch))
                continue;
            bool on = (ch->severity & (1 << SETSEV_TYPES[i].type)) != 0;
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                          "  %-8s %-3s  <k>%s<z>\r\n",
                          SETSEV_TYPES[i].name, on ? "<g>on<z>" : "<r>off<z>",
                          SETSEV_TYPES[i].desc);
        }
        descriptor_send(d, out);
        return true;
    }

    const setsev_entry_t *match = NULL;
    size_t tlen = strlen(tok);
    for (size_t i = 0; i < NUM_SETSEV_TYPES; i++) {
        if (SETSEV_TYPES[i].type == LOG_JESUS && !is_jesus(ch))
            continue;
        if (strncasecmp(SETSEV_TYPES[i].name, tok, tlen) == 0) {
            match = &SETSEV_TYPES[i];
            break;
        }
    }
    if (!match) {
        descriptor_send(d, "Incorrect log type. Type 'setsev' to see them all.\r\n");
        return true;
    }

    ch->severity ^= (1 << match->type);
    bool on = (ch->severity & (1 << match->type)) != 0;
    char msg[96];
    snprintf(msg, sizeof(msg), "Log type %s toggled %s.\r\n", match->name, on ? "on" : "off");
    descriptor_send(d, msg);
    return true;
}
