/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "log.h"

/* `snoop <name>` (59+, user 2026-07-11: "implement a snoop command like
 * sneezy, the command should be 59+ where you cant snoop anyone of same
 * or higher level"). Modeled on TPerson::doSnoop() (bundled reference
 * tree, misc/immortal.cc): mirrors everything a target sees AND everything
 * they type over to the snooper, one outgoing snoop per snooper at a time.
 * The actual mirroring lives in descriptor.c -- descriptor_send() mirrors
 * output, the CONN_PLAYING input handler mirrors typed lines (prefixed
 * "% ") -- this file just manages the snoop_target/snooped_by pointers and
 * the level/availability checks. Bare `snoop` (no argument) or `snoop
 * <yourself>` both stop your own outgoing snoop -- user 2026-07-11: "have
 * it default to self without an arg" (bare `snoop` used to just show
 * usage). Covert: the target is never told, and this is logged LOG_SILENT
 * (file only, matching the get/drop precedent for anything that shouldn't
 * tip anyone off live). */
bool cmd_snoop(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64];
    bool self_target = sscanf(args, "%63s", tok) != 1;
    size_t len = self_target ? 0 : strlen(tok);

    /* Bare `snoop`, or `snoop <yourself>` (any prefix of your own name):
     * stop your own outgoing snoop, if any -- same convention as the
     * original, just also reachable with no argument at all now. */
    if (self_target || strncasecmp(ch->base.name, tok, len) == 0) {
        if (d->snoop_target) {
            descriptor_send(d, "You stop snooping.\r\n");
            d->snoop_target->snooped_by = NULL;
            d->snoop_target = NULL;
        } else {
            descriptor_send(d, "Ok, you just snoop yourself.\r\n");
        }
        return true;
    }

    being_t *target = NULL;
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->character && it->character->base.roomp
            && strncasecmp(it->character->base.name, tok, len) == 0) {
            target = it->character;
            break;
        }
    }
    if (!target) {
        descriptor_send(d, "No such person around.\r\n");
        return true;
    }
    if (!target->desc) {
        descriptor_send(d, "There's no link.. nothing to snoop.\r\n");
        return true;
    }
    if (target->progress.level >= ch->progress.level) {
        descriptor_send(d, "You failed.\r\n");
        return true;
    }
    if (target->desc->snooped_by) {
        descriptor_send(d, "Busy already.\r\n");
        return true;
    }

    /* Only one outgoing snoop at a time -- switching targets drops the old
     * one first (same as the original). */
    if (d->snoop_target)
        d->snoop_target->snooped_by = NULL;

    d->snoop_target = target->desc;
    target->desc->snooped_by = d;

    char msg[128];
    snprintf(msg, sizeof(msg), "Ok. You are now snooping %s.\r\n", target->base.name);
    descriptor_send(d, msg);
    game_log(LOG_SILENT, "%s started snooping %s", ch->base.name, target->base.name);
    return true;
}
