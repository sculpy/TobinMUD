/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "cmd.h"
#include "obj.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* Substitutes `goto`'s bamfin/bamfout tokens into `tmpl`, rendering into
 * `out`: `$g`/`$$g` (room's ground-surface word, obj_apply_ground_token())
 * and `$p` (gender_possess() pronoun) -- same idea as cmd_move.c's
 * poofin/poofout tokens, but no `$d` (a teleport has no direction) -- plus
 * `<N>`/`<n>` (the mover's name, same convention as a player's `title`,
 * cmd_who.c's title_with_name()), which may appear anywhere in the
 * message. Returns true iff `<N>`/`<n>` was found and substituted, so the
 * caller knows the name is already embedded and shouldn't prefix it again
 * (user 2026-07-11: "bamfin|out should modify goto messaging"; follow-up:
 * "<N> should work in this as well as $g"; "and $p"). */
static bool apply_bamf_tokens(const char *tmpl, room_t *room, gender_t gender,
                               const char *name, char *out, size_t outsz) {
    char stage[BEING_BAMF_LEN + 64];
    obj_apply_ground_token(tmpl, room, stage, sizeof(stage));

    size_t o = 0;
    bool named = false;
    for (size_t i = 0; stage[i] != '\0' && o + 1 < outsz;) {
        if (stage[i] == '<' && (stage[i + 1] == 'N' || stage[i + 1] == 'n')
            && stage[i + 2] == '>') {
            for (const char *p = name; *p && o + 1 < outsz; p++)
                out[o++] = *p;
            i += 3;
            named = true;
        } else if (stage[i] == '$' && stage[i + 1] == 'p') {
            for (const char *p = gender_possess(gender); *p && o + 1 < outsz; p++)
                out[o++] = *p;
            i += 2;
        } else {
            out[o++] = stage[i++];
        }
    }
    out[o < outsz ? o : outsz - 1] = '\0';
    return named;
}

/* Broadcasts `tmpl` (bamfout for a departure, bamfin for an arrival) to
 * everyone else in `room`, falling back to the default puff-of-smoke
 * wording when empty. */
static void announce_bamf(being_t *ch, room_t *room, const char *tmpl, bool arriving) {
    char body[BEING_BAMF_LEN + 96];
    char msg[sizeof(body) + sizeof(ch->base.name) + 16];
    if (tmpl[0]) {
        bool named = apply_bamf_tokens(tmpl, room, ch->gender, ch->base.name, body, sizeof(body));
        if (named)
            snprintf(msg, sizeof(msg), "%s\r\n", body);
        else
            snprintf(msg, sizeof(msg), "%s %s.\r\n", ch->base.name, body);
    } else {
        snprintf(msg, sizeof(msg), "%s %s in a puff of smoke.\r\n",
                 ch->base.name, arriving ? "appears" : "disappears");
    }
    descriptor_room_echo(room, ch, msg);
}

/* `goto <vnum>` or `goto <player>`: immortal-only teleport. A number goes
 * straight to that room; a name goes to that online being's current room
 * (players now; mobs once they exist). Mirrors the original doGoto's
 * vnum-or-name behavior (cmd/cmd_goto.cc). Room lookup mirrors enter_world()'s
 * lazy load-and-register pattern. */
bool cmd_goto(descriptor_t *d, const char *args) {
    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Usage: goto <room vnum | player name>\r\n");
        return true;
    }

    room_t *r = NULL;

    if (isdigit((unsigned char)args[0])) {
        int vnum = atoi(args);
        r = world_get_room(vnum);
        if (!r) {
            r = room_repo_load(vnum);
            if (r)
                world_register_room(r);
        }
        if (!r) {
            char msg[80];
            snprintf(msg, sizeof(msg), "No room with vnum %d exists.\r\n", vnum);
            descriptor_send(d, msg);
            return true;
        }
    } else {
        /* Teleport to an online being by name (case-insensitive prefix). */
        char tok[64];
        sscanf(args, "%63s", tok);
        size_t len = strlen(tok);
        being_t *target = NULL;
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (it->character && it->character != d->character
                && it->character->base.roomp
                && strncasecmp(it->character->base.name, tok, len) == 0) {
                target = it->character;
                break;
            }
        }
        if (!target) {
            char msg[128];
            snprintf(msg, sizeof(msg), "No one named '%s' is in the game.\r\n", tok);
            descriptor_send(d, msg);
            return true;
        }
        r = target->base.roomp;
    }

    room_t *from = d->character->base.roomp;
    if (from)
        announce_bamf(d->character, from, d->character->bamfout, false);

    thing_set_room(&d->character->base, r);
    descriptor_send(d, "You vanish in a puff of smoke.\r\n");

    announce_bamf(d->character, r, d->character->bamfin, true);

    return cmd_dispatch(d, "look");
}
