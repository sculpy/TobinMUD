/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "being.h"
#include "log.h"
#include "obj.h"
#include "thing.h"

#define DRINK_POISON_CHANCE_PCT 30
#define DRINK_POISON_MIN_DMG 2
#define DRINK_POISON_MAX_DMG 8

/* Disease infection roll (TODO.md: "Diseases -- modest list affecting
 * players... cure path TBD"), independent of the poison roll above --
 * getting lucky on one doesn't protect against the other. Only puddles
 * carry this risk, same as poison; a fountain's clean water never rolls
 * for either. Durations are in COMBAT_ROUND_PULSES rounds (~1.2s each,
 * see affect.h) -- picked so DISEASE_TICK_EVERY (affect.c) divides them
 * evenly, landing a damage tick right as each one expires too. */
#define DRINK_DISEASE_CHANCE_PCT 15
static const affect_type_t DRINK_DISEASES[] = {
    AFFECT_DISEASE_COLD, AFFECT_DISEASE_FLU, AFFECT_DISEASE_FOOD_POISONING, AFFECT_DISEASE_PLAGUE,
};
static const int DRINK_DISEASE_DURATIONS[] = { 30, 50, 40, 80 };

/* Same keyword-abbreviation matching spirit as cmd_object.c's
 * obj_name_matches() (a case-insensitive prefix of any individual
 * space-separated keyword) -- duplicated locally rather than shared,
 * same precedent as cmd_object.c's own cap_first() duplication note. */
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

/* `drink` (user, 2026-07-11: "yu should be able tto drink from the pools,
 * chance to get poisoned"). Targets the ground puddles obj_grow_pool()
 * makes (obj.c) -- both `pee`'s and combat.c's bleeding-triggered blood
 * pools carry the "puddle" keyword, so that's the marker used to find them
 * among ordinary OBJ_CAT_TRASH litter rather than drinking just any piece
 * of trash. A puddle is never consumed/removed by drinking it. Poison is a
 * flavor scare, not lethal: damage is clamped so it can never drop the
 * drinker below 1 HP (no death-outside-combat handling exists yet).
 *
 * Also targets any real OBJ_CAT_DRINK object in the room -- fountains and
 * drink containers, already-seeded content (user, 2026-07-11 bug report:
 * "i just tried to drink from a fountain ... it failed"). Clean water, no
 * poison roll, never runs dry -- liquid-unit depletion (val[0]/val[1],
 * obj.h) isn't wired up yet and is out of scope for this fix. */
bool cmd_drink(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Usage: drink <puddle|fountain>\r\n");
        return true;
    }

    obj_t *pool = NULL, *fount = NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!keyword_matches(o->base.name, tok))
            continue;
        if (!pool && keyword_matches(o->base.name, "puddle")) {
            pool = o;
            break;
        }
        if (!fount && o->category == OBJ_CAT_DRINK)
            fount = o;
    }

    if (!pool && !fount) {
        descriptor_send(d, "You don't see that here to drink.\r\n");
        return true;
    }

    if (fount && !pool) {
        const char *label = fount->base.short_descr[0] ? fount->base.short_descr : fount->base.name;
        char msg[320]; /* name (64) + short_descr (128) + fixed text */
        snprintf(msg, sizeof(msg), "You drink some water from %s. Refreshing!\r\n", label);
        descriptor_send(d, msg);
        snprintf(msg, sizeof(msg), "%s drinks from %s.\r\n", ch->base.name, label);
        descriptor_room_echo(ch->base.roomp, ch, msg);
        return true;
    }

    const char *label = pool->base.short_descr[0] ? pool->base.short_descr : pool->base.name;
    char msg[320]; /* name (64) + short_descr (128) + fixed text */
    snprintf(msg, sizeof(msg), "You scoop up some of %s and drink it. Blech!\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s scoops up some of %s and drinks it.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    if (rand() % 100 < DRINK_POISON_CHANCE_PCT) {
        int dmg = DRINK_POISON_MIN_DMG + rand() % (DRINK_POISON_MAX_DMG - DRINK_POISON_MIN_DMG + 1);
        ch->progress.hp -= dmg;
        if (ch->progress.hp < 1)
            ch->progress.hp = 1;
        descriptor_send(d, "You feel a sharp pain as poison courses through you!\r\n");
        game_log(LOG_SILENT, "%s was poisoned drinking %s (vnum %d) in room %d",
                 ch->base.name, label, pool->vnum, ch->base.roomp->vnum);
    } else {
        descriptor_send(d, "Thankfully, it doesn't seem to have made you sick.\r\n");
    }

    if (!being_is_immortal(ch) && rand() % 100 < DRINK_DISEASE_CHANCE_PCT) {
        int idx = rand() % (int)(sizeof(DRINK_DISEASES) / sizeof(DRINK_DISEASES[0]));
        affect_type_t disease = DRINK_DISEASES[idx];
        if (!being_has_affect(ch, disease)) {
            being_apply_affect(ch, disease, DRINK_DISEASE_DURATIONS[idx]);
            char dmsg[96];
            snprintf(dmsg, sizeof(dmsg), "Ugh -- you feel like you've caught %s.\r\n", affect_name(disease));
            descriptor_send(d, dmsg);
            game_log(LOG_SILENT, "%s caught %s drinking %s (vnum %d) in room %d",
                     ch->base.name, affect_name(disease), label, pool->vnum, ch->base.roomp->vnum);
        }
    }

    return true;
}
