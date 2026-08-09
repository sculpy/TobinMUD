/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "cook.h"
#include "liquids.h"
#include "log.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* Same case-insensitive keyword-prefix matching duplicated across
 * cmd_drink.c/cmd_object.c/liquids.c -- kept as its own local copy here
 * too, same precedent those files already note. */
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
 * recipe to count. Returns the recipe's index, or -1 if none matched. */
static int cook_find_recipe(const char *args) {
    for (int i = 0; i < COOK_RECIPE_COUNT; i++) {
        const cook_recipe_t *r = cook_recipe_at(i);
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

/* Finds a carried (loose/held/worn -- same "one shared stuff_head chain"
 * convention liquid_find_carried_container() uses) obj matching `vnum`. */
static obj_t *find_carried_vnum(const being_t *ch, int vnum) {
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->vnum == vnum)
            return o;
    }
    return NULL;
}

/* First carried object of category OBJ_CAT_AMMO -- satisfies the
 * COOK_ING_AMMO ingredient kind ("any ammo will do"). */
static obj_t *find_carried_ammo(const being_t *ch) {
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category == OBJ_CAT_AMMO)
            return o;
    }
    return NULL;
}

/* A carried drink container (val[2]=liquid type) with at least `amt`
 * units of the requested liquid. */
static obj_t *find_carried_liquid(const being_t *ch, int liquid_type, int amt) {
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_DRINK)
            continue;
        if (o->val[2] == liquid_type && o->val[1] >= amt)
            return o;
    }
    return NULL;
}

/* A room-floor corpse (obj.h's own CORPSE val[] doc comment: name=="corpse",
 * val[2]=source mob_race) matching `race`. */
static obj_t *find_room_corpse(const room_t *room, int race) {
    for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (strcmp(o->base.name, "corpse") == 0 && o->val[2] == race)
            return o;
    }
    return NULL;
}

/* `cook <recipe>` (Cook profession, Sneezy -> Tobin feature audit, user
 * 2026-07-26: "professions" -- task_cook.h/.cc, real ingredient-matching
 * ported verbatim). Checks every ingredient SLOT (cook.c groups
 * task_cook.h's own (recipe, ingredient-number) rows -- multiple rows
 * sharing a slot are alternatives, "any of these") is satisfiable before
 * consuming anything -- an all-or-nothing check, same as the original's
 * own recipe-matching intent, not a partial-consume-then-fail bug. */
bool cmd_cook(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    /* `cook` skill gate (docs/Spell Assignments.xlsx gap audit, 2026-08-08)
     * -- this command had no skill check at all before. */
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "cook")) {
        descriptor_send(d, "You don't know how to cook.\r\n");
        return true;
    }

    if (!*args) {
        char out[600];
        int n = snprintf(out, sizeof(out), "Usage: cook <recipe>\r\nKnown recipes:\r\n");
        for (int i = 0; i < COOK_RECIPE_COUNT && (size_t)n < sizeof(out); i++)
            n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s\r\n", cook_recipe_at(i)->name);
        descriptor_send(d, out);
        return true;
    }

    int recipe_idx = cook_find_recipe(args);
    if (recipe_idx < 0) {
        descriptor_send(d, "You don't know a recipe like that.\r\n");
        return true;
    }
    const cook_recipe_t *recipe = cook_recipe_at(recipe_idx);

    int row_count;
    const cook_ing_row_t *rows = cook_ingredient_rows(&row_count);

    /* Pass 1: for every distinct slot in this recipe, find ONE satisfying
     * obj (first alternative that matches) without consuming anything
     * yet -- an all-or-nothing check. */
    #define MAX_COOK_SLOTS 8
    int slots[MAX_COOK_SLOTS];
    obj_t *found[MAX_COOK_SLOTS];
    int slot_count = 0;
    bool missing = false;

    for (int i = 0; i < row_count && !missing; i++) {
        const cook_ing_row_t *row = &rows[i];
        if (row->recipe != recipe_idx)
            continue;

        int slot_pos = -1;
        for (int s = 0; s < slot_count; s++)
            if (slots[s] == row->slot) { slot_pos = s; break; }
        if (slot_pos >= 0 && found[slot_pos])
            continue; /* this slot already satisfied by an earlier alternative */

        obj_t *match = NULL;
        switch (row->kind) {
            case COOK_ING_VNUM:   match = find_carried_vnum(ch, row->value); break;
            case COOK_ING_AMMO:   match = find_carried_ammo(ch); break;
            case COOK_ING_LIQUID: match = find_carried_liquid(ch, row->value, row->amt); break;
            case COOK_ING_CORPSE: match = find_room_corpse(ch->base.roomp, row->value); break;
        }

        if (slot_pos < 0) {
            if (slot_count >= MAX_COOK_SLOTS)
                continue;
            slot_pos = slot_count++;
            slots[slot_pos] = row->slot;
            found[slot_pos] = NULL;
        }
        if (match)
            found[slot_pos] = match;
    }

    for (int s = 0; s < slot_count; s++) {
        if (!found[s]) {
            missing = true;
            break;
        }
    }

    if (missing) {
        char msg[128];
        snprintf(msg, sizeof(msg), "You don't have everything you need to cook %s.\r\n", recipe->name);
        descriptor_send(d, msg);
        return true;
    }

    /* Pass 2: consume. Liquid containers just lose `amt` units (stay in
     * inventory, same as cmd_drink.c's own container branch); everything
     * else (vnum items, ammo, the corpse) is fully destroyed -- one row
     * per slot maps 1:1 to `found[]` since pass 1 only ever recorded the
     * FIRST matching row per slot. */
    for (int i = 0, s = 0; i < row_count && s < slot_count; i++) {
        const cook_ing_row_t *row = &rows[i];
        if (row->recipe != recipe_idx)
            continue;
        int slot_pos = -1;
        for (int j = 0; j < slot_count; j++)
            if (slots[j] == row->slot) { slot_pos = j; break; }
        if (slot_pos < 0)
            continue;
        obj_t *o = found[slot_pos];
        if (!o)
            continue;
        if (row->kind == COOK_ING_LIQUID) {
            if (o->val[2] == row->value) {
                o->val[1] -= row->amt;
                if (o->val[1] < 0)
                    o->val[1] = 0;
                found[slot_pos] = NULL; /* consumed, don't repeat on a later duplicate row */
                s++;
            }
        } else {
            obj_destroy(o);
            found[slot_pos] = NULL;
            s++;
        }
    }

    obj_t *result = obj_create_from_proto(recipe->result_vnum);
    if (result)
        thing_move_to(&result->base, &ch->base);

    /* Learn-by-doing (user, 2026-08-08: "all skills/spells should be
     * learn by doing") -- cook always succeeds once ingredients are
     * gathered, no separate roll to gate on, so this just trains the
     * skill on every real use. */
    if (!being_is_immortal(ch)) {
        const skill_def_t *cook_sk = skill_find(ch->char_class, "cook", false);
        if (cook_sk)
            skill_learn_from_doing(ch, cook_sk);
    }

    char msg[160];
    snprintf(msg, sizeof(msg), "You cook up %s!\r\n", recipe->name);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s cooks up %s.\r\n", ch->base.name, recipe->name);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    game_log(LOG_SILENT, "%s cooked %s (vnum %d) in room %d",
             ch->base.name, recipe->name, recipe->result_vnum, ch->base.roomp->vnum);

    return true;
}
