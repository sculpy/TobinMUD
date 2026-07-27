/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
 * include mana, piety, vitality, gold, etc"; expanded again 2026-07-19
 * to add exp/expneed and `prompt all` -- mana/piety stay blocked on
 * those resources not existing at all yet, see being.h's PROMPT_FLAG_*
 * comment). `prompt hp`/`prompt gold`/etc each toggle one stat in the
 * prompt ("HP: 25 Gold: 40 > ", any combination); `prompt all` turns ON
 * every currently-available toggle at once (not a toggle itself --
 * always sets, never clears, since "give me everything" has one obvious
 * meaning but "give me nothing" already has its own name per-stat);
 * bare `prompt` shows the current setup. Flags persist in
 * player.prompt_flags and are rendered by the game loop's prompter
 * (game_loop.c) -- designed so more stats can join the bitmask later
 * without touching the plumbing. */
bool cmd_prompt(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char msg[220];
    if (!*args) {
        snprintf(msg, sizeof(msg),
                 "Prompt: hp %s, gold %s, vit %s, exp %s, expneed %s. "
                 "Usage: prompt hp|gold|vit|exp|expneed|all\r\n",
                 (ch->prompt_flags & PROMPT_FLAG_HP) ? "ON" : "off",
                 (ch->prompt_flags & PROMPT_FLAG_GOLD) ? "ON" : "off",
                 (ch->prompt_flags & PROMPT_FLAG_VIT) ? "ON" : "off",
                 (ch->prompt_flags & PROMPT_FLAG_EXP) ? "ON" : "off",
                 (ch->prompt_flags & PROMPT_FLAG_EXPNEED) ? "ON" : "off");
        descriptor_send(d, msg);
        return true;
    }

    char tok[16];
    if (sscanf(args, "%15s", tok) != 1) {
        descriptor_send(d, "Usage: prompt hp|gold|vit|exp|expneed|all\r\n");
        return true;
    }

    if (strcasecmp(tok, "all") == 0) {
        ch->prompt_flags |= PROMPT_FLAG_HP | PROMPT_FLAG_GOLD | PROMPT_FLAG_VIT
                           | PROMPT_FLAG_EXP | PROMPT_FLAG_EXPNEED;
        player_set_prompt_flags(ch->player_id, ch->prompt_flags);
        game_log(LOG_SILENT, "%s turns on every available prompt stat", ch->base.name);
        descriptor_send(d, "Your prompt will now show every available stat.\r\n");
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
    } else if (strcasecmp(tok, "exp") == 0) {
        flag = PROMPT_FLAG_EXP;
        label = "experience";
    } else if (strcasecmp(tok, "expneed") == 0) {
        flag = PROMPT_FLAG_EXPNEED;
        label = "experience needed to level";
    } else {
        descriptor_send(d, "Usage: prompt hp|gold|vit|exp|expneed|all\r\n");
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
