/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "material.h"
#include "obj.h"
#include "skill.h"
#include "thing.h"

/* Missing-skill audit (TODO.md, "Generic / cross-class"), 2026-08-05:
 * real upstream SKILL_EVALUATE (cmd_compare.cc) gates how much detail
 * the `compare` command reveals about two items -- Tobin has no
 * `compare` command, so this ports as its own new `evaluate <item>`
 * command instead: always gives a price guess, fuzzed at low
 * proficiency and exact at high proficiency, and unlocks condition +
 * material-tier detail once skilled enough. Immortals (and anyone
 * without the skill at all, per every other appraisal-style command in
 * this codebase) always get the full, exact readout. */

/* Same small local keyword-match helper every cmd_*.c file in this
 * session duplicates rather than shares (cmd_repair.c's own
 * find_any_obj()) -- searches carried AND worn/held items, since the
 * item most worth appraising is often whatever's equipped. */
static obj_t *find_any_obj(const being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    if (len == 0)
        return NULL;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        const char *p = t->name;
        while (*p) {
            while (*p == ' ')
                p++;
            const char *start = p;
            while (*p && *p != ' ')
                p++;
            size_t wlen = (size_t)(p - start);
            if (wlen >= len && strncasecmp(start, tok, len) == 0)
                return (obj_t *)t;
            if (*p == ' ')
                p++;
        }
    }
    return NULL;
}

static const char *condition_label(const obj_t *o) {
    if (o->max_struct <= 0)
        return "not the sort of thing that shows wear";
    int pct = o->cur_struct * 100 / o->max_struct;
    if (pct >= 95)
        return "pristine";
    if (pct >= 75)
        return "lightly worn";
    if (pct >= 50)
        return "worn";
    if (pct >= 25)
        return "damaged";
    return "nearly wrecked";
}

bool cmd_evaluate(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "evaluate")) {
        descriptor_send(d, "You don't know how to evaluate items.\r\n");
        return true;
    }

    char tok[64] = "";
    sscanf(args, "%63s", tok);
    if (!*tok) {
        descriptor_send(d, "Evaluate what?\r\n");
        return true;
    }

    obj_t *o = find_any_obj(ch, tok);
    if (!o) {
        descriptor_send(d, "You don't have that.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "evaluate", imm);
    int prof = (imm || !sk) ? 100 : skill_learn_from_doing(ch, sk);

    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char msg[384];

    int price_guess = o->price;
    if (prof < 60) {
        /* Fuzz the price guess -- wider miss at low proficiency, tightening
         * as it climbs, exact only once truly skilled. */
        int fuzz_pct = 60 - prof; /* up to 60% off at prof 0, 1% off at prof 59 */
        int fuzz = price_guess * fuzz_pct / 100;
        if (fuzz > 0)
            price_guess += (rand() % (2 * fuzz + 1)) - fuzz;
        if (price_guess < 0)
            price_guess = 0;
    }

    snprintf(msg, sizeof(msg), "You examine %s carefully.\r\n"
             "You guess it's worth somewhere around %d gold.\r\n",
             label, price_guess);
    descriptor_send(d, msg);

    if (prof >= 30) {
        snprintf(msg, sizeof(msg), "It looks %s.\r\n", condition_label(o));
        descriptor_send(d, msg);
    }
    if (prof >= 60) {
        material_tier_t tier = material_tier_for_id(o->material);
        snprintf(msg, sizeof(msg), "It's made of %s-tier material.\r\n", material_tier_name(tier));
        descriptor_send(d, msg);
    }

    return true;
}
