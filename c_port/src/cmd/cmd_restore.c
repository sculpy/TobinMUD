/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "being.h"
#include "player_repo.h"

/* Immortal-only support tool (user, 2026-08-03, right after a real live
 * incident where a debug command accidentally chip-damaged a bystander
 * player's HP down to nearly nothing): "restore should be a immortal
 * command restore target restores all health and removes spell affects
 * from target." Online targets only -- affects are live in-memory state,
 * never persisted (affect.h), so there's nothing to "restore" on an
 * offline player beyond HP, and `set <name> hp <hp> <max>` already
 * covers that case. Vitality is restored too, same "fully patched up"
 * spirit, even though the user's wording only named health/affects. */
bool cmd_restore(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char target_name[64];
    if (sscanf(args, "%63s", target_name) != 1) {
        descriptor_send(d, "Usage: restore <target>\r\n");
        return true;
    }

    being_t *target = NULL;
    if (strcasecmp(target_name, "self") == 0) {
        target = d->character;
    } else {
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (it->character && strcasecmp(it->character->base.name, target_name) == 0) {
                target = it->character;
                break;
            }
        }
    }
    if (!target) {
        descriptor_send(d, "No one online by that name.\r\n");
        return true;
    }

    target->progress.hp = target->progress.max_hp;
    target->progress.vit = target->progress.max_vit;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (target->affects[i].type != AFFECT_NONE)
            being_remove_affect(target, target->affects[i].type);
    }
    if (target->player_id > 0)
        player_progress_save(target->player_id, &target->progress);

    char msg[128];
    snprintf(msg, sizeof(msg), "%s is fully restored -- health renewed, all spell affects cleared.\r\n",
             target->base.name);
    descriptor_send(d, msg);

    if (target != d->character && target->desc)
        descriptor_send(target->desc, "A wave of restorative energy washes over you, healing your wounds "
                                       "and clearing every lingering effect!\r\n");
    return true;
}
