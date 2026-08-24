/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
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
    const char *category;            /* personal (game=false) toggles only --
                                         one of the CATEGORIES[] names below;
                                         ignored for game=true rows */
    bool (*get)(descriptor_t *);
    void (*set)(descriptor_t *, bool);
} toggle_t;

/* Personal-toggle categories, in listing order (user 2026-07-12: "split
 * the toggle listing into categories: Preferences, Prompt,
 * Communication"). Only `toggle` (game=false) groups by these; `gametog`
 * (game=true, global switches) keeps its single flat list. */
static const char *const CATEGORIES[] = { "Preferences", "Prompt", "Communication" };
#define NUM_CATEGORIES (sizeof(CATEGORIES) / sizeof(CATEGORIES[0]))

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
/* --- gold/vit/mana/exp/expneed: same shape as hp above, one per
 * remaining PROMPT_FLAG_* bit (user request, 2026-08-22: "update the
 * toggles related to prompt" -- `toggle hp` already duplicated
 * `prompt hp`, but the other five stats were reachable only through
 * `prompt <stat>`, never `toggle`; this closes that gap for
 * consistency). `prompt <stat>`/`prompt all` still work unchanged --
 * both commands read/write the exact same player.prompt_flags bits. */
static bool tg_gold_get(descriptor_t *d) {
    return d->character && (d->character->prompt_flags & PROMPT_FLAG_GOLD);
}
static void tg_gold_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->prompt_flags |= PROMPT_FLAG_GOLD;
    else
        d->character->prompt_flags &= ~PROMPT_FLAG_GOLD;
    player_set_prompt_flags(d->character->player_id, d->character->prompt_flags);
}
static bool tg_vit_get(descriptor_t *d) {
    return d->character && (d->character->prompt_flags & PROMPT_FLAG_VIT);
}
static void tg_vit_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->prompt_flags |= PROMPT_FLAG_VIT;
    else
        d->character->prompt_flags &= ~PROMPT_FLAG_VIT;
    player_set_prompt_flags(d->character->player_id, d->character->prompt_flags);
}
static bool tg_mana_get(descriptor_t *d) {
    return d->character && (d->character->prompt_flags & PROMPT_FLAG_MANA);
}
static void tg_mana_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->prompt_flags |= PROMPT_FLAG_MANA;
    else
        d->character->prompt_flags &= ~PROMPT_FLAG_MANA;
    player_set_prompt_flags(d->character->player_id, d->character->prompt_flags);
}
static bool tg_exp_get(descriptor_t *d) {
    return d->character && (d->character->prompt_flags & PROMPT_FLAG_EXP);
}
static void tg_exp_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->prompt_flags |= PROMPT_FLAG_EXP;
    else
        d->character->prompt_flags &= ~PROMPT_FLAG_EXP;
    player_set_prompt_flags(d->character->player_id, d->character->prompt_flags);
}
static bool tg_expneed_get(descriptor_t *d) {
    return d->character && (d->character->prompt_flags & PROMPT_FLAG_EXPNEED);
}
static void tg_expneed_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->prompt_flags |= PROMPT_FLAG_EXPNEED;
    else
        d->character->prompt_flags &= ~PROMPT_FLAG_EXPNEED;
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

/* --- nospam: hide "miss" combat messages on your own screen (player.pflags
 * bit) -- ported from Sneezy's AUTO_NOSPAM (see combat.c's combat_strike()
 * for where it's actually checked, independently per viewer). */
static bool tg_nospam_get(descriptor_t *d) {
    return d->character && (d->character->pflags & PLR_NOSPAM);
}
static void tg_nospam_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags |= PLR_NOSPAM;
    else
        d->character->pflags &= ~PLR_NOSPAM;
    player_set_pflags(d->character->player_id, d->character->pflags);
}

/* --- autoloot: automatically loot a defeated opponent's corpse
 * (player.pflags bit) -- checked in combat.c's combat_defeat(), right
 * after the corpse is populated. */
static bool tg_autoloot_get(descriptor_t *d) {
    return d->character && (d->character->pflags & PLR_AUTOLOOT);
}
static void tg_autoloot_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags |= PLR_AUTOLOOT;
    else
        d->character->pflags &= ~PLR_AUTOLOOT;
    player_set_pflags(d->character->player_id, d->character->pflags);
}

/* --- pk: willing to fight other players (player.pflags bit) -- BOTH
 * sides need this on for attack/kill/hit to even find each other,
 * enforced in combat.c's combat_find_room_target(), not here. */
static bool tg_pk_get(descriptor_t *d) {
    return d->character && (d->character->pflags & PLR_PK_OPTIN);
}
static void tg_pk_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags |= PLR_PK_OPTIN;
    else
        d->character->pflags &= ~PLR_PK_OPTIN;
    player_set_pflags(d->character->player_id, d->character->pflags);
}

/* --- tips: opt out of the periodic pulse-driven tip echo (player.pflags
 * bit) -- separate from `newbie` on purpose so silencing tips doesn't
 * also drop you off the newbie help channel. Sense is inverted (PLR_NOTIPS
 * means tips are OFF) so the toggle itself still reads naturally: "tips is
 * now on/off". */
static bool tg_tips_get(descriptor_t *d) {
    return d->character && !(d->character->pflags & PLR_NOTIPS);
}
static void tg_tips_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags &= ~PLR_NOTIPS;
    else
        d->character->pflags |= PLR_NOTIPS;
    player_set_pflags(d->character->player_id, d->character->pflags);
}

/* --- notell: block incoming tells, except from your own last_told
 * (player.pflags bit, descriptor.h's last_told) -- the actual block
 * happens in cmd_tell.c/cmd_reply.c, not here. */
static bool tg_notell_get(descriptor_t *d) {
    return d->character && (d->character->pflags & PLR_NOTELL);
}
static void tg_notell_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags |= PLR_NOTELL;
    else
        d->character->pflags &= ~PLR_NOTELL;
    player_set_pflags(d->character->player_id, d->character->pflags);
}

/* --- afk: opt in to an auto-away notice on incoming tells once idle
 * (player.pflags bit) -- the actual idle check + notice happens in
 * cmd_tell.c/cmd_reply.c, not here. */
static bool tg_afk_get(descriptor_t *d) {
    return d->character && (d->character->pflags & PLR_AFK);
}
static void tg_afk_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags |= PLR_AFK;
    else
        d->character->pflags &= ~PLR_AFK;
    player_set_pflags(d->character->player_id, d->character->pflags);
}

/* --- msp: MSP sound/music playback (player.pflags bit) -- TODO.md,
 * "Rework MSP into a toggle so players can turn sound on/off". Sense
 * inverted like `tips` above (PLR_NOMSP means MSP is OFF), so the
 * toggle itself still reads naturally: "msp is now on/off". Turning it
 * off also stops any track already looping (descriptor_send_msp_
 * music_off(), gated on client capability alone, not this preference --
 * see its own doc comment, descriptor.c) rather than leaving the player
 * to sit through it until the next natural fight-end trigger. */
static bool tg_msp_get(descriptor_t *d) {
    return d->character && !(d->character->pflags & PLR_NOMSP);
}
static void tg_msp_set(descriptor_t *d, bool v) {
    if (!d->character)
        return;
    if (v)
        d->character->pflags &= ~PLR_NOMSP;
    else
        d->character->pflags |= PLR_NOMSP;
    player_set_pflags(d->character->player_id, d->character->pflags);
    if (!v)
        descriptor_send_msp_music_off(d);
}

/* --- multiplay: global game toggle (55+) --- */
static bool tg_multiplay_get(descriptor_t *d) { (void)d; return multiplay_allowed(); }
static void tg_multiplay_set(descriptor_t *d, bool v) { (void)d; multiplay_set(v); }

static const toggle_t TOGGLES[] = {
    { "color",     "ANSI color rendering",          false, "Preferences",   tg_color_get,     tg_color_set },
    { "nospam",    "hide combat miss messages",     false, "Preferences",   tg_nospam_get,    tg_nospam_set },
    { "autoloot",  "auto-loot a defeated corpse",   false, "Preferences",   tg_autoloot_get,  tg_autoloot_set },
    { "hp",        "hit points shown in prompt",    false, "Prompt",        tg_hp_get,        tg_hp_set },
    { "gold",      "gold shown in prompt",          false, "Prompt",        tg_gold_get,      tg_gold_set },
    { "vit",       "vitality shown in prompt",      false, "Prompt",        tg_vit_get,       tg_vit_set },
    { "mana",      "mana shown in prompt",          false, "Prompt",        tg_mana_get,      tg_mana_set },
    { "exp",       "experience shown in prompt",    false, "Prompt",        tg_exp_get,       tg_exp_set },
    { "expneed",   "exp to next level in prompt",   false, "Prompt",        tg_expneed_get,   tg_expneed_set },
    { "newbie",    "on the newbie help channel",    false, "Communication", tg_newbie_get,    tg_newbie_set },
    { "noshout",   "opted out of hearing shouts",   false, "Communication", tg_noshout_get,   tg_noshout_set },
    { "notell",    "block incoming tells",          false, "Communication", tg_notell_get,    tg_notell_set },
    { "afk",       "auto-away notice on tells",     false, "Communication", tg_afk_get,       tg_afk_set },
    { "tips",      "periodic newbie tip echoes",    false, "Communication", tg_tips_get,      tg_tips_set },
    { "pk",        "willing to fight other players", false, "Preferences",  tg_pk_get,        tg_pk_set },
    { "msp",       "MSP sound/music playback",      false, "Preferences",   tg_msp_get,       tg_msp_set },
    { "multiplay", "one account, many characters",  true,  NULL,            tg_multiplay_get, tg_multiplay_set },
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
        char out[1024];
        int n = snprintf(out, sizeof(out), "\r\n<c>-- %s --<z>\r\n", header);
        if (game) {
            for (size_t i = 0; i < NUM_TOGGLES && (size_t)n < sizeof(out); i++) {
                if (TOGGLES[i].game != game)
                    continue;
                n += snprintf(out + n, sizeof(out) - (size_t)n,
                              "  %-12s %-3s  <k>%s<z>\r\n",
                              TOGGLES[i].name, onoff(TOGGLES[i].get(d)), TOGGLES[i].desc);
            }
        } else {
            /* Grouped by category (user 2026-07-12: "split the toggle
             * listing into categories: Preferences, Prompt,
             * Communication") -- a category with no matching toggles is
             * skipped entirely rather than printing an empty header. */
            for (size_t c = 0; c < NUM_CATEGORIES && (size_t)n < sizeof(out); c++) {
                bool any = false;
                for (size_t i = 0; i < NUM_TOGGLES; i++) {
                    if (TOGGLES[i].game == game && strcasecmp(TOGGLES[i].category, CATEGORIES[c]) == 0) {
                        any = true;
                        break;
                    }
                }
                if (!any)
                    continue;
                n += snprintf(out + n, sizeof(out) - (size_t)n, "<y>%s:<z>\r\n", CATEGORIES[c]);
                for (size_t i = 0; i < NUM_TOGGLES && (size_t)n < sizeof(out); i++) {
                    if (TOGGLES[i].game != game || strcasecmp(TOGGLES[i].category, CATEGORIES[c]) != 0)
                        continue;
                    n += snprintf(out + n, sizeof(out) - (size_t)n,
                                  "  %-12s %-3s  <k>%s<z>\r\n",
                                  TOGGLES[i].name, onoff(TOGGLES[i].get(d)), TOGGLES[i].desc);
                }
            }
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

/* `toggle [<name>]` command: the mortal-facing entry point into
 * toggle_dispatch() for personal switches only (game=false) -- see
 * file-top comment for why global switches are split into `gametog`
 * below instead of living here too. */
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
