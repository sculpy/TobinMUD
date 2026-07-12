/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "account.h"
#include "being.h"
#include "multiplay.h"
#include "player_repo.h"

/* `toggle` -- one place to see and flip on/off PERSONAL switches (color, hp
 * in prompt, ...). Global GAME toggles (multiplay, ...) live in the
 * separate `gametog` command (58+, cmd_gametog() below) instead -- split
 * out (TODO.md, user-planned) so a mortal-facing `toggle` never even shows
 * a row that could affect other players. Both share the one `TOGGLES[]`
 * table (filtered by the `game` flag) and the same list/set logic; new
 * features register their switch by adding a row to TOGGLES below.
 * Inspired by Sneezy's toggle. */

typedef struct {
    const char *name;
    const char *desc;
    bool game;                       /* true = global game toggle (55+) */
    bool (*get)(descriptor_t *);
    void (*set)(descriptor_t *, bool);
} toggle_t;

/* --- color: per-connection + persisted on the account --- */
static bool tg_color_get(descriptor_t *d) { return d->color_enabled; }
static void tg_color_set(descriptor_t *d, bool v) {
    d->color_enabled = v;
    d->account.color_pref = v;
    if (d->account.account_id > 0)
        account_set_color(d->account.account_id, v);
}

/* --- hp: show hit points in the prompt (player.prompt_flags bit) --- */
static bool tg_hp_get(descriptor_t *d) {
    return d->character && (d->character->prompt_flags & PROMPT_FLAG_HP);
}
static void tg_hp_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->prompt_flags |= PROMPT_FLAG_HP;
    else
        d->character->prompt_flags &= ~PROMPT_FLAG_HP;
    player_set_prompt_flags(d->character->player_id, d->character->prompt_flags);
}

/* --- newbie: on the newbie help channel (player.pflags bit) --- */
static bool tg_newbie_get(descriptor_t *d) {
    return d->character && (d->character->pflags & PLR_NEWBIE);
}
static void tg_newbie_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags |= PLR_NEWBIE;
    else
        d->character->pflags &= ~PLR_NEWBIE;
    player_set_pflags(d->character->player_id, d->character->pflags);
}

/* --- noshout: opted out of hearing `shout` (player.pflags bit). An
 * immortal's shout still gets through regardless of this -- enforced in
 * cmd_shout.c, not here. */
static bool tg_noshout_get(descriptor_t *d) {
    return d->character && (d->character->pflags & PLR_NOSHOUT);
}
static void tg_noshout_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags |= PLR_NOSHOUT;
    else
        d->character->pflags &= ~PLR_NOSHOUT;
    player_set_pflags(d->character->player_id, d->character->pflags);
}

/* --- multiplay: global game toggle (55+) --- */
static bool tg_multiplay_get(descriptor_t *d) { (void)d; return multiplay_allowed(); }
static void tg_multiplay_set(descriptor_t *d, bool v) { (void)d; multiplay_set(v); }

static const toggle_t TOGGLES[] = {
    { "color",     "ANSI color rendering",          false, tg_color_get,     tg_color_set },
    { "hp",        "hit points shown in prompt",    false, tg_hp_get,        tg_hp_set },
    { "newbie",    "on the newbie help channel",    false, tg_newbie_get,    tg_newbie_set },
    { "noshout",   "opted out of hearing shouts",   false, tg_noshout_get,   tg_noshout_set },
    { "multiplay", "one account, many characters",  true,  tg_multiplay_get, tg_multiplay_set },
};
#define NUM_TOGGLES (sizeof(TOGGLES) / sizeof(TOGGLES[0]))

static const char *onoff(bool v) { return v ? "<g>on<z>" : "<r>off<z>"; }

/* Shared by `toggle` (game=false: personal switches only) and `gametog`
 * (game=true: global game switches only) -- everything else about listing/
 * setting a switch is identical, so only which subset of TOGGLES[] is
 * visible differs. */
static bool toggle_dispatch(descriptor_t *d, const char *args, bool game, const char *header) {
    char tok[32] = "";
    sscanf(args, "%31s", tok);

    if (!tok[0]) {
        char out[768];
        int n = snprintf(out, sizeof(out), "\r\n<c>-- %s --<z>\r\n", header);
        for (size_t i = 0; i < NUM_TOGGLES && (size_t)n < sizeof(out); i++) {
            if (TOGGLES[i].game != game)
                continue;
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                          "  %-12s %-3s  <k>%s<z>\r\n",
                          TOGGLES[i].name, onoff(TOGGLES[i].get(d)), TOGGLES[i].desc);
        }
        descriptor_send(d, out);
        return true;
    }

    /* Prefix-match a toggle name within the visible subset, first match
     * wins (abbreviations welcome). */
    const toggle_t *tg = NULL;
    size_t tlen = strlen(tok);
    for (size_t i = 0; i < NUM_TOGGLES; i++) {
        if (TOGGLES[i].game != game)
            continue;
        if (strncasecmp(TOGGLES[i].name, tok, tlen) == 0) {
            tg = &TOGGLES[i];
            break;
        }
    }
    if (!tg) {
        char msg[80];
        snprintf(msg, sizeof(msg), "No such toggle. Type '%s' to see them all.\r\n",
                 game ? "gametog" : "toggle");
        descriptor_send(d, msg);
        return true;
    }

    bool nv = !tg->get(d);
    tg->set(d, nv);
    char msg[128];
    snprintf(msg, sizeof(msg), "%s is now %s<z>.\r\n", tg->name, onoff(nv));
    descriptor_send(d, msg);
    return true;
}

bool cmd_toggle(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;
    return toggle_dispatch(d, args, false, "Toggles");
}

/* `gametog` (58+, TODO.md-planned split from `toggle`): global GAME
 * switches (multiplay, ...) that affect everyone, not just the caller --
 * kept out of the mortal-facing `toggle` entirely rather than merely
 * hidden by level, so there's no row a mortal could ever see. Table-level
 * gate (cmd_table.c) already keeps mortals out; no internal level check
 * needed here. */
bool cmd_gametog(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;
    return toggle_dispatch(d, args, true, "Game Toggles");
}
