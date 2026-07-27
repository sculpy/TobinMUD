/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "combat.h"
#include "suit.h"
#include "suit_repo.h"

/* `loadsuit <suit name> [target]` -- user 2026-07-26: "report on the vnums
 * used for the new loadsuit immortal 56+ command... suits are defined in
 * the database as a suitset that can be loaded at will." Senior-immortal
 * tier (LOADSUIT_MIN_LEVEL, same "God"-tier as help-edit/addnews) rather
 * than the plain builder tier `load` uses -- suits can outfit a whole
 * character at once, a bigger lever than spawning one prototype. Name is
 * abbreviation-matched against `suit.name` (suit_repo_find_by_name()),
 * same "prefix of a keyword" spirit as `load`'s own vnum-or-name lookup.
 * With no target, outfits the caller; with one, outfits whoever that name
 * matches in the room (mob or PC -- combat_find_room_target(), same
 * lookup `transfer`'s sibling commands already use for "someone here").
 * Reuses the exact same suit_grant() (suit.c) the newbie-suit-at-
 * creation and social-worker-reissue paths already call -- one grant
 * implementation, three ways to trigger it. */
bool cmd_loadsuit(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char suit_tok[64] = "", target_tok[64] = "";
    int got = sscanf(args, "%63s %63s", suit_tok, target_tok);
    if (got < 1) {
        descriptor_send(d, "Usage: loadsuit <suit name> [target]\r\n");
        return true;
    }

    char suit_name[32];
    int suit_id = suit_repo_find_by_name(suit_tok, NULL, suit_name, sizeof(suit_name));
    if (suit_id < 0) {
        descriptor_send(d, "No suit matches that name.\r\n");
        return true;
    }

    being_t *target = ch;
    if (got == 2) {
        target = combat_find_room_target(ch, target_tok);
        if (!target) {
            descriptor_send(d, "No one by that name is here.\r\n");
            return true;
        }
    }

    int n = suit_grant(target, suit_id);
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
