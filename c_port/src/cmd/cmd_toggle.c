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

/* `toggle` -- one place to see and flip on/off switches. Players toggle the
 * things that affect only them (color, hp in prompt); immortals 55+ can also
 * flip global GAME toggles (multiplay, ...). Bare `toggle` prints the current
 * value of every toggle the caller may see. New features register their
 * switches by adding a row to TOGGLES below. Inspired by Sneezy's toggle. */

#define TOGGLE_GAME_MIN_LEVEL 55

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

/* --- multiplay: global game toggle (55+) --- */
static bool tg_multiplay_get(descriptor_t *d) { (void)d; return multiplay_allowed(); }
static void tg_multiplay_set(descriptor_t *d, bool v) { (void)d; multiplay_set(v); }

static const toggle_t TOGGLES[] = {
    { "color",     "ANSI color rendering",          false, tg_color_get,     tg_color_set },
    { "hp",        "hit points shown in prompt",    false, tg_hp_get,        tg_hp_set },
    { "multiplay", "one account, many characters",  true,  tg_multiplay_get, tg_multiplay_set },
};
#define NUM_TOGGLES (sizeof(TOGGLES) / sizeof(TOGGLES[0]))

static const char *onoff(bool v) { return v ? "<g>on<z>" : "<r>off<z>"; }

bool cmd_toggle(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;
    bool can_game = ch->progress.level >= TOGGLE_GAME_MIN_LEVEL;

    char tok[32] = "";
    sscanf(args, "%31s", tok);

    if (!tok[0]) {
        char out[768];
        int n = snprintf(out, sizeof(out), "\r\n<c>-- Toggles --<z>\r\n");
        for (size_t i = 0; i < NUM_TOGGLES && (size_t)n < sizeof(out); i++) {
            if (TOGGLES[i].game && !can_game)
                continue; /* hide game toggles from those who can't set them */
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                          "  %-12s %-3s  <k>%s%s<z>\r\n",
                          TOGGLES[i].name, onoff(TOGGLES[i].get(d)),
                          TOGGLES[i].desc, TOGGLES[i].game ? " (game)" : "");
        }
        descriptor_send(d, out);
        return true;
    }

    /* Prefix-match a toggle name, first match wins (abbreviations welcome). */
    const toggle_t *tg = NULL;
    size_t tlen = strlen(tok);
    for (size_t i = 0; i < NUM_TOGGLES; i++) {
        if (strncasecmp(TOGGLES[i].name, tok, tlen) == 0) {
            tg = &TOGGLES[i];
            break;
        }
    }
    if (!tg) {
        descriptor_send(d, "No such toggle. Type 'toggle' to see them all.\r\n");
        return true;
    }
    if (tg->game && !can_game) {
        descriptor_send(d, "That's a game toggle -- only 55+ immortals may change it.\r\n");
        return true;
    }

    bool nv = !tg->get(d);
    tg->set(d, nv);
    char msg[128];
    snprintf(msg, sizeof(msg), "%s is now %s<z>.\r\n", tg->name, onoff(nv));
    descriptor_send(d, msg);
    return true;
}
