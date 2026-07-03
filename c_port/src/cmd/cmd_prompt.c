#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "player_repo.h"

/* `prompt [stat]`: per-player prompt customization (user spec, Session
 * 21). `prompt hp` toggles hit points in the prompt ("HP: 25 > ");
 * bare `prompt` shows the current setup. Flags persist in
 * player.prompt_flags and are rendered by the game loop's prompter
 * (game_loop.c) -- designed so vitality/xp/gold can join the bitmask
 * later without touching the plumbing. */
bool cmd_prompt(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char msg[128];
    if (!*args) {
        snprintf(msg, sizeof(msg), "Prompt: hp %s. Usage: prompt hp\r\n",
                 (ch->prompt_flags & PROMPT_FLAG_HP) ? "ON" : "off");
        descriptor_send(d, msg);
        return true;
    }

    char tok[16];
    if (sscanf(args, "%15s", tok) == 1 && strcasecmp(tok, "hp") == 0) {
        ch->prompt_flags ^= PROMPT_FLAG_HP;
        player_set_prompt_flags(ch->player_id, ch->prompt_flags);
        snprintf(msg, sizeof(msg), "Your prompt will %s show hit points.\r\n",
                 (ch->prompt_flags & PROMPT_FLAG_HP) ? "now" : "no longer");
        descriptor_send(d, msg);
        return true;
    }

    descriptor_send(d, "Usage: prompt hp   (toggles hit points in your prompt)\r\n");
    return true;
}
