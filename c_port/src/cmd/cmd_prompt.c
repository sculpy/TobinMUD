/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "log.h"
#include "player_repo.h"

/* `prompt [stat]`: per-player prompt customization (user spec, Session
 * 21; expanded 2026-07-18, user: "expand prompt command toggles to
 * include mana, piety, vitality, gold, etc" -- mana/piety/vitality stay
 * blocked on those stats not existing at all yet, see being.h's
 * PROMPT_FLAG_* comment; gold unblocked once the Money system shipped).
 * `prompt hp`/`prompt gold` each toggle one stat in the prompt ("HP: 25
 * Gold: 40 > ", either or both); bare `prompt` shows the current setup.
 * Flags persist in player.prompt_flags and are rendered by the game
 * loop's prompter (game_loop.c) -- designed so more stats can join the
 * bitmask later without touching the plumbing. */
bool cmd_prompt(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char msg[160];
    if (!*args) {
        snprintf(msg, sizeof(msg), "Prompt: hp %s, gold %s, vit %s. Usage: prompt hp|gold|vit\r\n",
                 (ch->prompt_flags & PROMPT_FLAG_HP) ? "ON" : "off",
                 (ch->prompt_flags & PROMPT_FLAG_GOLD) ? "ON" : "off",
                 (ch->prompt_flags & PROMPT_FLAG_VIT) ? "ON" : "off");
        descriptor_send(d, msg);
        return true;
    }

    char tok[16];
    if (sscanf(args, "%15s", tok) != 1) {
        descriptor_send(d, "Usage: prompt hp|gold|vit   (toggles that stat in your prompt)\r\n");
        return true;
    }

    int flag;
    const char *label;
    if (strcasecmp(tok, "hp") == 0) {
        flag = PROMPT_FLAG_HP;
        label = "hit points";
    } else if (strcasecmp(tok, "gold") == 0) {
        flag = PROMPT_FLAG_GOLD;
        label = "gold";
    } else if (strcasecmp(tok, "vit") == 0) {
        flag = PROMPT_FLAG_VIT;
        label = "vitality";
    } else {
        descriptor_send(d, "Usage: prompt hp|gold|vit   (toggles that stat in your prompt)\r\n");
        return true;
    }

    ch->prompt_flags ^= flag;
    player_set_prompt_flags(ch->player_id, ch->prompt_flags);
    bool now_on = (ch->prompt_flags & flag) != 0;
    game_log(LOG_SILENT, "%s turns %s %s in their prompt", ch->base.name,
             now_on ? "on" : "off", label);
    snprintf(msg, sizeof(msg), "Your prompt will %s show %s.\r\n",
             now_on ? "now" : "no longer", label);
    descriptor_send(d, msg);
    return true;
}
