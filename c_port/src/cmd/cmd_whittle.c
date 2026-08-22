/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "log.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"
#include "whittle.h"

/* Real seeded raw-wood-log vnums (material.h's own doc comment: MAT_WOOD
 * = 5, confirmed live against these exact rows) -- the wood "ingredient"
 * task_whittleCreateNew() consumed from a player's TOrganic wood in the
 * real upstream. A fixed vnum whitelist, same convention cook.c's
 * COOK_ING_VNUM rows use, rather than matching any material=5 object
 * (which would also match finished wooden furniture/items). */
#define WOOD_MATERIAL_ID 5
static const int WOOD_LOG_VNUMS[] = { 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88 };
#define WOOD_LOG_VNUM_COUNT (sizeof(WOOD_LOG_VNUMS) / sizeof(WOOD_LOG_VNUMS[0]))

static bool is_wood_log_vnum(int vnum) {
    for (size_t i = 0; i < WOOD_LOG_VNUM_COUNT; i++)
        if (WOOD_LOG_VNUMS[i] == vnum)
            return true;
    return false;
}

/* Same case-insensitive keyword-prefix matching duplicated across
 * cmd_cook.c/cmd_drink.c/cmd_object.c/liquids.c. */
static bool keyword_matches(const char *keywords, const char *tok) {
    size_t tok_len = strlen(tok);
    if (tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}

/* Matches a typed argument string against every known recipe's keyword
 * list, word by word -- ALL typed words must match a keyword for the
 * recipe to count. Same shape as cmd_cook.c's cook_find_recipe(). */
static int whittle_find_recipe(const char *args) {
    for (int i = 0; i < WHITTLE_RECIPE_COUNT; i++) {
        const whittle_recipe_t *r = whittle_recipe_at(i);
        bool all_words_match = true;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", args);
        char *save = NULL;
        char *tok = strtok_r(buf, " ", &save);
        int matched_any = 0;
        while (tok) {
            if (keyword_matches(r->keywords, tok))
                matched_any++;
            else
                all_words_match = false;
            tok = strtok_r(NULL, " ", &save);
        }
        if (matched_any > 0 && all_words_match)
            return i;
    }
    return -1;
}

/* Total carried (loose/held/worn) weight across every real wood-log
 * vnum -- mirrors task_whittleCreateNew()'s own totalWood[] scan. */
static double carried_wood_weight(const being_t *ch) {
    double total = 0.0;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (is_wood_log_vnum(o->vnum))
            total += o->weight;
    }
    return total;
}

/* Destroys whole carried logs, in carry order, until at least `need`
 * weight has been consumed -- whole-item destruction, same "destroy
 * outright, no fractional split" precedent cmd_cook.c's COOK_ING_VNUM
 * rows use, rather than the original's cut-a-chunk-off-one-log math. */
static void consume_wood(being_t *ch, double need) {
    thing_t *t = ch->base.stuff_head;
    while (t && need > 0.0) {
        thing_t *next = t->stuff_next;
        if (t->kind == THING_OBJ) {
            obj_t *o = (obj_t *)t;
            if (is_wood_log_vnum(o->vnum)) {
                need -= o->weight;
                obj_destroy(o);
            }
        }
        t = next;
    }
}

/* `whittle <item>` (Whittle profession, Sneezy -> Tobin feature audit,
 * TODO.md "Deferred decisions" -- the second `task/` profession
 * alongside cook. See whittle.h's own doc comment for the real
 * upstream's multi-tick/bow-arrow scope this cuts.) Requires a weapon
 * wielded in the primary hand (the original's own slash/pierce gate --
 * Tobin's weapon objects carry no damage-type field to check that
 * finely against, so any OBJ_CAT_WEAPON qualifies, a disclosed
 * loosening) and enough carried wood-log weight for the chosen recipe;
 * an all-or-nothing check before anything is consumed, same convention
 * cmd_cook.c's two-pass check/consume split uses. */
bool cmd_whittle(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    /* `whittle` skill gate (docs/Spell Assignments.xlsx gap audit,
     * 2026-08-08) -- this command had no skill check at all before. */
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "whittle")) {
        descriptor_send(d, "You don't know how to whittle.\r\n");
        return true;
    }

    if (!*args) {
        char out[900];
        int n = snprintf(out, sizeof(out), "Usage: whittle <item>\r\nKnown items:\r\n");
        for (int i = 0; i < WHITTLE_RECIPE_COUNT && (size_t)n < sizeof(out); i++)
            n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s\r\n", whittle_recipe_at(i)->name);
        descriptor_send(d, out);
        return true;
    }

    int primary = ch->handed_right ? 0 : 1;
    if (!ch->held[primary] || ch->held[primary]->category != OBJ_CAT_WEAPON) {
        descriptor_send(d, "You need a weapon wielded in your primary hand to whittle.\r\n");
        return true;
    }

    int recipe_idx = whittle_find_recipe(args);
    if (recipe_idx < 0) {
        descriptor_send(d, "You have no idea how to whittle that.\r\n");
        return true;
    }
    const whittle_recipe_t *recipe = whittle_recipe_at(recipe_idx);

    if (carried_wood_weight(ch) < recipe->min_wood_weight) {
        descriptor_send(d, "You don't have enough wood to whittle that.\r\n");
        return true;
    }

    consume_wood(ch, recipe->min_wood_weight);

    obj_t *result = obj_create_from_proto(recipe->result_vnum);
    if (result)
        thing_move_to(&result->base, &ch->base);

    /* Learn-by-doing (user, 2026-08-08) -- same "always succeeds once
     * ingredients are gathered, train on every real use" shape as
     * cmd_cook.c's own identical hook. */
    if (!being_is_immortal(ch)) {
        const skill_def_t *whittle_sk = skill_find(ch->char_class, "whittle", false);
        if (whittle_sk)
            skill_learn_from_doing(ch, whittle_sk);
    }

    char msg[160];
    snprintf(msg, sizeof(msg), "You whittle a %s!\r\n", recipe->name);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s whittles a %s.\r\n", ch->base.name, recipe->name);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    game_log(LOG_SILENT, "%s whittled %s (vnum %d) in room %d",
             ch->base.name, recipe->name, recipe->result_vnum, ch->base.roomp->vnum);

    return true;
}
