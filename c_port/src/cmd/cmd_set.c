/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "player_repo.h"

/* `set <name> <field> <value>`: Administrator (58+) only -- a one-shot,
 * scriptable sibling of `edplayer` for quick single-field edits (user
 * spec: build both, not one instead of the other). Same admin-wide reach
 * as `edplayer`/`promote` (any player by exact name, online or offline,
 * via player_load_admin()), same online-target sync courtesy, but no
 * menu -- one line in, one field changed, done. See `edplayer` for the
 * menu-driven equivalent covering every field in one sitting. */

/* Loads the target, applies one field mutation to it, then persists via
 * whichever repo call(s) that field actually belongs to. Returns a
 * confirmation/error string (never NULL) -- caller sends it verbatim. */
static const char *apply_field(being_t *w, int load_room, int *out_load_room,
                                const char *field, const char *rest) {
    static char msg[160];

    if (!rest[0]) {
        snprintf(msg, sizeof(msg), "Usage: set <name> %s <value>\r\n", field);
        return msg;
    }

    if (strcasecmp(field, "level") == 0) {
        char *end;
        long v = strtol(rest, &end, 10);
        if (end == rest || v < MORTAL_LEVEL_MIN || v > IMMORTAL_LEVEL_MAX) {
            snprintf(msg, sizeof(msg), "Level must be between %d and %d.\r\n",
                     MORTAL_LEVEL_MIN, IMMORTAL_LEVEL_MAX);
            return msg;
        }
        w->progress.level = (int)v;
        if (!player_progress_save(w->player_id, &w->progress))
            return "Save failed -- the DB rejected it.\r\n";
        snprintf(msg, sizeof(msg), "%s's level is now %ld.\r\n", w->base.name, v);
        return msg;
    }

    if (strcasecmp(field, "xp") == 0 || strcasecmp(field, "experience") == 0) {
        char *end;
        long v = strtol(rest, &end, 10);
        if (end == rest || v < 0)
            return "Experience must be a non-negative number.\r\n";
        w->progress.experience = v;
        if (!player_progress_save(w->player_id, &w->progress))
            return "Save failed -- the DB rejected it.\r\n";
        snprintf(msg, sizeof(msg), "%s's experience is now %ld.\r\n", w->base.name, v);
        return msg;
    }

    if (strcasecmp(field, "alignment") == 0) {
        char *end;
        long v = strtol(rest, &end, 10);
        if (end == rest || v < -1000 || v > 1000)
            return "Alignment must be between -1000 (evil) and 1000 (good).\r\n";
        w->progress.alignment = (int)v;
        if (!player_progress_save(w->player_id, &w->progress))
            return "Save failed -- the DB rejected it.\r\n";
        snprintf(msg, sizeof(msg), "%s's alignment is now %ld (%s).\r\n",
                 w->base.name, v, alignment_word((int)v));
        return msg;
    }

    if (strcasecmp(field, "hp") == 0) {
        int hp = 0, max_hp = 0;
        if (sscanf(rest, "%d %d", &hp, &max_hp) != 2 || hp < 0 || max_hp < 1 || hp > max_hp)
            return "Usage: set <name> hp <hp> <max hp>, with 0 <= hp <= max hp and max hp >= 1.\r\n";
        w->progress.hp = hp;
        w->progress.max_hp = max_hp;
        if (!player_progress_save(w->player_id, &w->progress))
            return "Save failed -- the DB rejected it.\r\n";
        snprintf(msg, sizeof(msg), "%s's HP is now %d/%d.\r\n", w->base.name, hp, max_hp);
        return msg;
    }

    int *attr = attrs_field(&w->attrs, field);
    if (attr) {
        char *end;
        long v = strtol(rest, &end, 10);
        if (end == rest || v < 1 || v > ATTR_MAX) {
            snprintf(msg, sizeof(msg), "Value must be between 1 and %d.\r\n", ATTR_MAX);
            return msg;
        }
        *attr = (int)v;
        if (!player_attrs_save(w->player_id, &w->attrs))
            return "Save failed -- the DB rejected it.\r\n";
        snprintf(msg, sizeof(msg), "%s's %s is now %ld.\r\n", w->base.name, field, v);
        return msg;
    }

    if (strcasecmp(field, "gender") == 0) {
        gender_t g;
        if (strcasecmp(rest, "male") == 0 || strcasecmp(rest, "m") == 0) g = GENDER_MALE;
        else if (strcasecmp(rest, "female") == 0 || strcasecmp(rest, "f") == 0) g = GENDER_FEMALE;
        else if (strcasecmp(rest, "neuter") == 0 || strcasecmp(rest, "n") == 0) g = GENDER_NEUTER;
        else return "Usage: set <name> gender male|female|neuter\r\n";
        w->gender = g;
        if (!player_set_gender_by_name(w->base.name, g))
            return "Save failed -- the DB rejected it.\r\n";
        snprintf(msg, sizeof(msg), "%s's gender is now %s.\r\n", w->base.name, gender_name(g));
        return msg;
    }

    if (strcasecmp(field, "title") == 0) {
        const char *title = strcasecmp(rest, "none") == 0 ? "" : rest;
        snprintf(w->title, sizeof(w->title), "%s", title);
        if (!player_set_title(w->base.name, w->account_id, w->title))
            return "Save failed -- the DB rejected it.\r\n";
        snprintf(msg, sizeof(msg), "%s's title is now %s.\r\n", w->base.name, title[0] ? title : "(none)");
        return msg;
    }

    if (strcasecmp(field, "loadroom") == 0) {
        char *end;
        long v = strtol(rest, &end, 10);
        if (end == rest || v < 0)
            return "Load room must be a non-negative vnum.\r\n";
        if (!player_set_load_room(w->base.name, w->account_id, (int)v))
            return "Save failed -- the DB rejected it.\r\n";
        *out_load_room = (int)v;
        snprintf(msg, sizeof(msg), "%s's load room is now %ld.\r\n", w->base.name, v);
        return msg;
    }

    if (strcasecmp(field, "handed") == 0) {
        int hr;
        if (strcasecmp(rest, "left") == 0 || strcasecmp(rest, "l") == 0) hr = 0;
        else if (strcasecmp(rest, "right") == 0 || strcasecmp(rest, "r") == 0) hr = 1;
        else return "Usage: set <name> handed left|right\r\n";
        w->handed_right = hr;
        if (!player_set_handed_by_name(w->base.name, hr))
            return "Save failed -- the DB rejected it.\r\n";
        snprintf(msg, sizeof(msg), "%s is now %s handed.\r\n", w->base.name, hr ? "right" : "left");
        return msg;
    }

    (void)load_room;
    return "Unknown field. Try: level, xp, hp, alignment, str/dex/con/int/wis/cha, gender, title, loadroom, handed.\r\n";
}

bool cmd_set(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char name[64], field[32], rest[128];
    rest[0] = '\0';
    int got = sscanf(args, "%63s %31s %127[^\r\n]", name, field, rest);
    if (got < 2) {
        descriptor_send(d, "Usage: set <name> <field> <value>\r\n"
                            "Fields: level, xp, hp, alignment, str/dex/con/int/wis/cha, "
                            "gender, title, loadroom, handed\r\n");
        return true;
    }

    int load_room = -1;
    being_t *target = player_load_admin(name, &load_room);
    if (!target) {
        char msg[96];
        snprintf(msg, sizeof(msg), "No player named '%s' exists.\r\n", name);
        descriptor_send(d, msg);
        return true;
    }

    const char *result = apply_field(target, load_room, &load_room, field, rest);
    descriptor_send(d, result);

    /* If that player is online right now, sync their live being_t too --
     * same courtesy `promote` and `edplayer` already give. */
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        /* NOT `state == CONN_PLAYING` -- that misses a target mid-edit
         * (Session 43 audit), leaving their live copy stale. */
        if (it->character
            && strcasecmp(it->character->base.name, target->base.name) == 0) {
            it->character->progress = target->progress;
            it->character->attrs = target->attrs;
            snprintf(it->character->title, sizeof(it->character->title), "%s", target->title);
            it->character->gender = target->gender;
            it->character->handed_right = target->handed_right;
            break;
        }
    }

    being_destroy(target);
    return true;
}
