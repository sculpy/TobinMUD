/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"
#include "room.h"
#include "thing.h"

/* `egotrip <subcommand>` (Sneezy port, user 2026-07-12). The original
 * (cmd_egotrip.cc) is a 13-subcommand immortal toy-box (deity/bless/
 * blast/damn/hate/cleanse/wander/stupidity/crit/portal/teleport/
 * disease/garble) built almost entirely on systems Tobin hasn't built
 * yet: a disease system, a garble/speech-distortion system, mob AI
 * hate/aggro tracking (task 27), stat-modifying affects beyond
 * Sanctuary's flat damage halving (task 13), and free-standing portal
 * objects. Rather than stub out twelve dead branches, this ports the
 * one subcommand that maps cleanly onto what already exists: `blast
 * <target>`, a non-lethal bolt of lightning that halves the target's
 * current HP (floored at 1 -- per the original's own comment, "this
 * just nails um, but shouldn't actually kill them"). Implementor-only
 * (60+), matching `balance`'s tier -- same "should be used seldomly"
 * spirit as the original's own help text. */
bool cmd_egotrip(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char sub[16] = "";
    int consumed = 0;
    sscanf(args, "%15s %n", sub, &consumed);
    if (!sub[0] || strcasecmp(sub, "blast") != 0) {
        descriptor_send(d,
            "Syntax: egotrip blast <target>\r\n"
            "(Only 'blast' is implemented -- the original's deity/bless/damn/hate/\r\n"
            "cleanse/wander/stupidity/crit/portal/teleport/disease/garble subcommands\r\n"
            "each depend on systems Tobin hasn't built yet.)\r\n");
        return true;
    }

    const char *rest = args + consumed;
    while (*rest == ' ')
        rest++;
    char tok[64];
    if (sscanf(rest, "%63s", tok) != 1) {
        descriptor_send(d, "Syntax: egotrip blast <target>\r\n");
        return true;
    }

    size_t len = strlen(tok);
    being_t *target = NULL;
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->character && strncasecmp(it->character->base.name, tok, len) == 0) {
            target = it->character;
            break;
        }
    }
    if (!target) {
        char out[128];
        snprintf(out, sizeof(out), "No one named '%s' is in the game.\r\n", tok);
        descriptor_send(d, out);
        return true;
    }

    target->progress.hp /= 2;
    if (target->progress.hp < 1)
        target->progress.hp = 1;

    char out[300];
    snprintf(out, sizeof(out), "You blast %s with a bolt of lightning.\r\n", target->base.name);
    descriptor_send(d, out);

    if (target->desc) {
        descriptor_notify(target->desc,
            "A bolt of lightning streaks down from the heavens right at your feet!\r\n"
            "BZZZZZaaaaaappppp!!!!!\r\n");
    }
    if (target->base.roomp) {
        snprintf(out, sizeof(out),
                 "A bolt of lightning streaks down from the heavens right at %s's feet!\r\n",
                 target->base.name);
        descriptor_room_echo(target->base.roomp, target, out);
    }

    return true;
}
