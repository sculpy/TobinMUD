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
#include "cmd.h"
#include "combat.h"
#include "descriptor.h"
#include "log.h"

/* `force <target> <command>` (user, 2026-08-08: "need a force command
 * thats 55+ that force <target> [command] mobs or players can be forced"
 * -- classic Diku/Sneezy immortal command, no dedicated Sneezy source
 * file of its own to port, just the standard shape).
 *
 * A PC target is found world-wide (same g_descriptors scan egotrip's
 * blast/disease/crit already use) and the command runs on their OWN
 * real descriptor -- exactly as if they'd typed it themselves, so their
 * own normal command feedback goes to their own screen, not the
 * immortal's. A mob target is looked up in the CALLER'S OWN room only
 * (combat_find_room_target(), same helper `kill`/`disarm`/etc. already
 * share) -- Tobin has no world-wide mob-by-name index to search instead,
 * a disclosed, narrower scope than Sneezy's own world-wide get_char_vis_
 * world() lookup. A mob has no real descriptor, so a throwaway one is
 * heap-allocated (descriptor_t is large -- a 128KB page buffer alone --
 * so this is NOT stack-allocated), zeroed, and pointed at the mob; fd=-1
 * makes any stray descriptor_send() a safe no-op (descriptor_write()
 * bails out immediately on a negative-fd socket_write() failure) since
 * nothing is listening on the other end anyway.
 *
 * No special per-command filtering is needed: cmd_dispatch()'s own
 * min_level gate (cmd_table.c) already checks the ACTING being's level
 * (the target's, once forced) against each command's own minimum, so a
 * forced mob (ordinary mob level, never immortal) can no more reach an
 * immortal-only command through `force` than it could by any other
 * route -- forcing a lowbie mob to `wipe`/`shutdown`/`egotrip` just
 * gets it the same "Huh?!" a mortal typing it directly would. */
bool cmd_force(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64] = "";
    int consumed = 0;
    sscanf(args, "%63s %n", tok, &consumed);
    const char *command = args + consumed;
    while (*command == ' ')
        command++;

    if (!tok[0] || !*command) {
        descriptor_send(d, "Syntax: force <target> <command>\r\n");
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
    if (!target && ch->base.roomp)
        target = combat_find_room_target(ch, tok);

    if (!target) {
        char out[128];
        snprintf(out, sizeof(out), "No one named '%s' is in the game.\r\n", tok);
        descriptor_send(d, out);
        return true;
    }
    if (target == ch) {
        descriptor_send(d, "Force yourself to do something? Just do it.\r\n");
        return true;
    }
    /* Immortal-vs-immortal guard, same true-rank comparison cmd_kill.c's
     * own instakill guard uses -- can't force an equal-or-higher-ranked
     * immortal peer. Mobs are never immortal, so this only ever gates a
     * PC target. */
    if (target->base.kind == THING_PC) {
        int my_rank = ch->progress.level;
        int their_rank = target->progress.true_level >= IMMORTAL_LEVEL_MIN
                              ? target->progress.true_level
                              : target->progress.level;
        if (their_rank >= my_rank) {
            descriptor_send(d, "Shame shame, you shouldn't do that.\r\n");
            return true;
        }
    }

    char out[256];
    snprintf(out, sizeof(out), "You force %s to '%s'.\r\n", target->base.name, command);
    descriptor_send(d, out);
    game_log(LOG_SILENT, "%s forced %s to '%s'", ch->base.name, target->base.name, command);

    if (target->desc) {
        cmd_dispatch(target->desc, command);
        return true;
    }

    /* Mob target -- see this file's own header comment on why this is
     * heap-, not stack-, allocated. */
    descriptor_t *fake = calloc(1, sizeof(descriptor_t));
    if (!fake) {
        descriptor_send(d, "Out of memory -- could not force a mob.\r\n");
        return true;
    }
    fake->fd = -1;
    fake->character = target;
    cmd_dispatch(fake, command);
    free(fake);
    return true;
}
