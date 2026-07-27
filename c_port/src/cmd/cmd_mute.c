/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"
#include "player_repo.h"

/* `mute <player>` / `unmute <player>` (2026-07-27 docs/systems review --
 * port of the original's PLR_GODNOSHOUT): an immortal-imposed ban on a
 * misbehaving player's tell/shout, checked in cmd_tell.c/cmd_reply.c/
 * cmd_shout.c. The original also blocks "emote" -- skipped here, since
 * Tobin has no freeform emote command at all (only predefined, DB-backed
 * `smile`/`wave`/... social actions via cmd_socials.c, which carry none
 * of the "type any abusive text" risk a real emote does).
 *
 * Online-only, unlike `promote`'s offline-DB-write path: player_repo.h
 * has no by-name pflags setter (only level/gender/handed/appearance/
 * class/race), and muting is inherently a "stop them right now" tool
 * anyway -- a target worth muting is, by definition, actively causing a
 * problem while connected. */

static being_t *find_online(const char *name) {
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->character && strcasecmp(it->character->base.name, name) == 0)
            return it->character;
    }
    return NULL;
}

bool cmd_mute(descriptor_t *d, const char *args) {
    being_t *self = d->character;
    if (!self)
        return true;

    char name[PLAYER_NAME_LEN];
    if (sscanf(args, "%63s", name) < 1) {
        descriptor_send(d, "Usage: mute <player>\r\n");
        return true;
    }
    if (strcasecmp(name, self->base.name) == 0) {
        descriptor_send(d, "You can't mute yourself.\r\n");
        return true;
    }

    being_t *target = find_online(name);
    if (!target) {
        char msg[96];
        snprintf(msg, sizeof(msg), "%s isn't connected right now.\r\n", name);
        descriptor_send(d, msg);
        return true;
    }
    if (target->progress.level >= self->progress.level) {
        descriptor_send(d, "You can't mute someone your equal or better.\r\n");
        return true;
    }

    target->pflags |= PLR_MUTED;
    player_set_pflags(target->player_id, target->pflags);
    char msg[128];
    snprintf(msg, sizeof(msg), "%s is now muted (tell/shout blocked).\r\n", target->base.name);
    descriptor_send(d, msg);
    if (target->desc)
        descriptor_notify(target->desc, "An immortal has muted you -- tell and shout are blocked.\r\n");
    return true;
}

bool cmd_unmute(descriptor_t *d, const char *args) {
    being_t *self = d->character;
    if (!self)
        return true;

    char name[PLAYER_NAME_LEN];
    if (sscanf(args, "%63s", name) < 1) {
        descriptor_send(d, "Usage: unmute <player>\r\n");
        return true;
    }

    being_t *target = find_online(name);
    if (!target) {
        char msg[96];
        snprintf(msg, sizeof(msg), "%s isn't connected right now.\r\n", name);
        descriptor_send(d, msg);
        return true;
    }
    if (!(target->pflags & PLR_MUTED)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "%s isn't muted.\r\n", target->base.name);
        descriptor_send(d, msg);
        return true;
    }

    target->pflags &= ~PLR_MUTED;
    player_set_pflags(target->player_id, target->pflags);
    char msg[96];
    snprintf(msg, sizeof(msg), "%s is no longer muted.\r\n", target->base.name);
    descriptor_send(d, msg);
    if (target->desc)
        descriptor_notify(target->desc, "An immortal has lifted your mute.\r\n");
    return true;
}
