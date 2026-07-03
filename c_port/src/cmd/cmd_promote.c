#include "cmd_internal.h"

#include <stdio.h>
#include <strings.h>

#include "player_repo.h"

/* `promote <name> [level]`: immortal-only, sets another player's level
 * (defaults to IMMORTAL_LEVEL_MIN -- "make them an immortal"). Replaces the
 * manual `UPDATE player_progress ...` SQL that was the only promotion path
 * from Session 10 through Session 20 (Phase 2A). Loosely modeled on the
 * original's doAdvance (cmd/cmd_advance.cc) but simplified: no per-class
 * multiclass leveling, and the gate is level-based (can't set anyone above
 * your own level) rather than wiz-power-based, consistent with how `kill`'s
 * POWER_SLAY gate was simplified in Session 14.
 *
 * Works on offline players too (it's a DB write); if the target is online,
 * their live character is updated immediately and they're told. The name
 * must be exact (no abbreviation) -- too consequential for prefix matching. */
bool cmd_promote(descriptor_t *d, const char *args) {
    being_t *self = d->character;
    if (!self)
        return true;

    char name[PLAYER_NAME_LEN];
    int level = IMMORTAL_LEVEL_MIN;
    int parsed = sscanf(args, "%63s %d", name, &level);
    if (parsed < 1) {
        descriptor_send(d, "Usage: promote <name> [level]\r\n");
        return true;
    }

    if (level < MORTAL_LEVEL_MIN || level > IMMORTAL_LEVEL_MAX) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Level must be between %d and %d.\r\n",
                 MORTAL_LEVEL_MIN, IMMORTAL_LEVEL_MAX);
        descriptor_send(d, msg);
        return true;
    }
    if (level > self->progress.level) {
        descriptor_send(d, "You can't promote anyone above your own level.\r\n");
        return true;
    }
    if (strcasecmp(name, self->base.name) == 0) {
        descriptor_send(d, "You can't promote yourself.\r\n");
        return true;
    }

    /* Online target? Update the live character too, not just the DB, so
     * the change takes effect without a relog. */
    being_t *target = NULL;
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->state == CONN_PLAYING && it->character
            && strcasecmp(it->character->base.name, name) == 0) {
            target = it->character;
            break;
        }
    }

    if (!player_set_level_by_name(target ? target->base.name : name, level)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "No player named '%s' exists.\r\n", name);
        descriptor_send(d, msg);
        return true;
    }

    const char *title = being_level_title(level);
    char msg[160];
    if (target) {
        int old_level = target->progress.level;
        target->progress.level = level;
        if (target->desc) {
            snprintf(msg, sizeof(msg), "%s has %s you to level %d%s%s%s!\r\n",
                     self->base.name, level >= old_level ? "promoted" : "demoted",
                     level, title ? " (" : "", title ? title : "", title ? ")" : "");
            descriptor_send(target->desc, msg);
        }
    }
    snprintf(msg, sizeof(msg), "%s is now level %d%s%s%s.%s\r\n",
             target ? target->base.name : name, level,
             title ? " (" : "", title ? title : "", title ? ")" : "",
             target ? "" : " (offline -- takes effect at their next login)");
    descriptor_send(d, msg);
    return true;
}
