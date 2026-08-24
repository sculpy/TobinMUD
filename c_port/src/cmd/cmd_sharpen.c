/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "combat.h"
#include "obj.h"
#include "player_repo.h"
#include "skill.h"
#include "thing.h"

/* `sharpen`/`smooth` (missing-skill audit batch C, 2026-08-09). Real
 * upstream: SKILL_SHARPEN (obj_base_weapon.cc's sharpenMeStoneWeap(),
 * needs a held TOOL_WHETSTONE) raises an edged/piercing weapon's
 * curSharp toward its maxSharp; SKILL_DULL (dullMeFileWeap(), needs a
 * held TOOL_FILE) does the exact same thing for a BLUNT weapon --
 * despite the name, it doesn't make the weapon worse, it removes nicks/
 * dents ("bluntness" is just curSharp relabeled for that weapon class).
 * Tobin names the second skill `smooth` instead of the upstream's
 * confusingly-named `dull`, since that's what it actually does.
 *
 * Both commands share this one implementation, scoped down from the
 * original's multi-tick TASK_SHARPEN/TASK_DULL continuation (350 real
 * upstream game-ticks of gradual progress) to a single command + single
 * skill roll -- same "one command, one roll" shape this project's other
 * ported combat skills already use. Also dropped: per-weapon `max
 * sharpness` data (obj.h's SHARPNESS_MAX is one flat ceiling for every
 * weapon) and tool-uses/durability on the whetstone/file itself (a
 * carried whetstone or file is reusable forever here). Both are real,
 * disclosed scope-cuts, not fakes. */

/* Case-insensitive "does haystack contain needle" -- same small helper
 * duplicated across several cmd_*.c/combat.c files already (strcasestr
 * is GNU-only). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

/* Same bludgeon-keyword classification combat.c's own (file-static)
 * weapon_verb() uses for its slash/chop/stab/pierce-vs-blunt split,
 * duplicated locally here rather than exported just for this one
 * check -- same "small helper copied, not shared" precedent
 * liquids.c's own keyword_matches() already set. */
static bool weapon_is_blunt(const obj_t *weapon) {
    const char *n = weapon->base.name;
    const char *s = weapon->base.short_descr;
    return ci_contains(n, "mace") || ci_contains(s, "mace")
        || ci_contains(n, "hammer") || ci_contains(s, "hammer")
        || ci_contains(n, "club") || ci_contains(s, "club")
        || ci_contains(n, "staff") || ci_contains(s, "staff");
}

/* Finds a carried tool (OBJ_CAT_TOOL) whose name or short description
 * contains `keyword` ("whetstone"/"file") -- same carried-item scan
 * shape liquids.c's own liquid_find_carried_container() already
 * established for OBJ_CAT_DRINK, generalized to any category here
 * rather than duplicating the ordinal-prefix parsing this simpler case
 * doesn't need. */
static obj_t *find_carried_tool(const being_t *ch, const char *keyword) {
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_TOOL)
            continue;
        if (ci_contains(t->name, keyword) || ci_contains(t->short_descr, keyword))
            return o;
    }
    return NULL;
}

/* Shared body for both commands. `raising_edge` is true for `sharpen`
 * (edged/piercing weapons, needs a whetstone), false for `smooth`
 * (blunt weapons, needs a file). */
static bool do_sharpen_or_smooth(descriptor_t *d, bool raising_edge,
                                  const char *skill_name, const char *tool_kw,
                                  const char *verb_ing, const char *verb_past) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, skill_name)) {
        char msg[96];
        snprintf(msg, sizeof(msg), "You don't know how to %s weapons.\r\n", verb_past);
        descriptor_send(d, msg);
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return true;
    }

    obj_t *weapon = combat_wielded_weapon(ch);
    if (!weapon) {
        descriptor_send(d, "You aren't wielding a weapon.\r\n");
        return true;
    }

    bool blunt = weapon_is_blunt(weapon);
    if (raising_edge && blunt) {
        descriptor_send(d, "Generally, that weapon isn't something you'd want sharp.\r\n");
        return true;
    }
    if (!raising_edge && !blunt) {
        descriptor_send(d, "Generally, that weapon isn't something you'd want dulled.\r\n");
        return true;
    }

    const char *label = weapon->base.short_descr[0] ? weapon->base.short_descr : weapon->base.name;

    if (weapon->sharpness >= SHARPNESS_MAX) {
        char msg[320];
        snprintf(msg, sizeof(msg), "%s looks as %s as it's going to get.\r\n", label,
                 raising_edge ? "sharp" : "smooth");
        descriptor_send(d, msg);
        return true;
    }

    obj_t *tool = find_carried_tool(ch, tool_kw);
    if (!tool) {
        char msg[64];
        snprintf(msg, sizeof(msg), "You need to carry a %s.\r\n", tool_kw);
        descriptor_send(d, msg);
        return true;
    }

    /* Real upstream requires 10+ move points to attempt (Tobin's own
     * move-equivalent resource is Vitality, progress.vit). */
    if (ch->progress.vit < 10 && !imm) {
        descriptor_send(d, "You're much too tired for that right now.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, skill_name, imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    if (!imm) {
        ch->progress.vit -= 10;
        if (ch->progress.vit < 0)
            ch->progress.vit = 0;
    }

    char msg[400];
    if (!success) {
        snprintf(msg, sizeof(msg), "You work %s %s with your %s, but can't get it quite right.\r\n",
                 label, verb_ing, tool_kw);
        descriptor_send(d, msg);
        if (!imm)
            player_progress_save(ch->player_id, &ch->progress);
        return true;
    }

    weapon->sharpness += SHARPNESS_GAIN_PER_USE;
    if (weapon->sharpness > SHARPNESS_MAX)
        weapon->sharpness = SHARPNESS_MAX;

    snprintf(msg, sizeof(msg), "You %s %s with your %s -- it looks %s now.\r\n",
             verb_past, label, tool_kw, weapon->sharpness >= SHARPNESS_MAX
                 ? (raising_edge ? "razor-sharp" : "perfectly smooth")
                 : (raising_edge ? "sharper" : "smoother"));
    descriptor_send(d, msg);

    if (!imm)
        player_progress_save(ch->player_id, &ch->progress);
    return true;
}

/* `sharpen [weapon]` -- Sharpens the weapon held in your wielding
 * hand(s) using a carried whetstone. Edged/piercing weapons only. */
bool cmd_sharpen(descriptor_t *d, const char *args) {
    (void)args;
    return do_sharpen_or_smooth(d, true, "sharpen", "whetstone", "sharpening", "sharpen");
}

/* `smooth [weapon]` -- Smooths the nicks and dents from the blunt
 * weapon held in your wielding hand(s) using a carried file. */
bool cmd_smooth(descriptor_t *d, const char *args) {
    (void)args;
    return do_sharpen_or_smooth(d, false, "smooth", "file", "smoothing", "smooth");
}
