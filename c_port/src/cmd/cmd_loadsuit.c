/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "being.h"
#include "combat.h"
#include "suit.h"
#include "suit_repo.h"
#include "thing.h"

/* `loadsuit <suit id> [target]` -- user 2026-07-26: "report on the vnums
 * used for the new loadsuit immortal 56+ command... suits are defined in
 * the database as a suitset that can be loaded at will." Senior-immortal
 * tier (LOADSUIT_MIN_LEVEL, same "God"-tier as help-edit/addnews) rather
 * than the plain builder tier `load` uses -- suits can outfit a whole
 * character at once, a bigger lever than spawning one prototype.
 *
 * Number-driven (user, 2026-08-02: "loadsuit by name is quirky, lets be
 * number driven") -- was substring-matched against suit.name, same as
 * `edit suit` used to be; both now take the numeric suit id `edit suit`'s
 * no-argument listing shows, so a typo or ambiguous abbreviation can't
 * silently pick the wrong suit.
 *
 * With no target, outfits the caller; with one, outfits whoever that name
 * matches in the room (mob or PC). The target lookup checks the caller's
 * OWN name first (user, 2026-08-02: "load suit human_race jesus produces
 * noone here by that name" -- jesus was the caller's own name) rather than
 * going straight to combat_find_room_target(), which deliberately excludes
 * the caller (it's built for "attack someone else"); loadsuit has no such
 * restriction, so self-targeting by name has to be checked separately.
 * Reuses the exact same suit_grant() (suit.c) the newbie-suit-at-
 * creation and social-worker-reissue paths already call -- one grant
 * implementation, three ways to trigger it. */
bool cmd_loadsuit(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char id_tok[64] = "", target_tok[64] = "";
    int got = sscanf(args, "%63s %63s", id_tok, target_tok);
    if (got < 1) {
        descriptor_send(d, "Usage: loadsuit <suit id> [target]\r\n");
        return true;
    }

    char *end;
    long suit_id = strtol(id_tok, &end, 10);
    char suit_name[32];
    if (end == id_tok || suit_id <= 0 ||
        !suit_repo_get((int)suit_id, suit_name, sizeof(suit_name), NULL, NULL, NULL, 0)) {
        descriptor_send(d, "No such suit id -- `edit suit` with no argument lists them all.\r\n");
        return true;
    }

    being_t *target = ch;
    if (got == 2) {
        const char *rest;
        int ordinal = thing_parse_ordinal(target_tok, &rest);
        size_t name_len = strlen(rest);
        if (ordinal > 1 || !thing_name_matches(ch->base.name, rest, name_len))
            target = combat_find_room_target(ch, target_tok);
        if (!target) {
            descriptor_send(d, "No one by that name is here.\r\n");
            return true;
        }
    }

    int n = suit_grant(target, (int)suit_id);
    if (n == 0) {
        descriptor_send(d, "That suit has no items defined.\r\n");
        return true;
    }

    char msg[256];
    if (target == ch) {
        snprintf(msg, sizeof(msg), "You load %d item%s of suit '%s' onto yourself.\r\n",
                 n, n == 1 ? "" : "s", suit_name);
        descriptor_send(d, msg);
    } else {
        snprintf(msg, sizeof(msg), "You load %d item%s of suit '%s' onto %s.\r\n",
                 n, n == 1 ? "" : "s", suit_name, being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "%s outfits you with a fresh set of gear.\r\n", ch->base.name);
            descriptor_send(target->desc, msg);
        }
    }
    return true;
}
