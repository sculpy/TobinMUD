/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "being.h"
#include "vitals.h"
#include "liquids.h"
#include "log.h"
#include "obj.h"
#include "thing.h"

#define DRINK_POISON_CHANCE_PCT 30
#define DRINK_POISON_DURATION 20

/* Disease infection roll (TODO.md: "Diseases -- modest list affecting
 * players... cure path TBD"), independent of the poison roll above --
 * getting lucky on one doesn't protect against the other. Only puddles
 * carry this risk, same as poison; a fountain's clean water never rolls
 * for either. Durations are in COMBAT_ROUND_PULSES rounds (~1.2s each,
 * see affect.h) -- picked so DISEASE_TICK_EVERY (affect.c) divides them
 * evenly, landing a damage tick right as each one expires too. */
#define DRINK_DISEASE_CHANCE_PCT 15
/* Full upstream diseaseTypeT roster (misc/disease.h) minus DISEASE_NULL/
 * DISEASE_POISON -- see affect.h's enum comment. Duration ordering
 * loosely follows affect.c's DISEASE_HP_DRAIN/AFFECT_CURE_PRICE severity
 * ranking (worse ones both hit harder AND last longer). */
static const affect_type_t DRINK_DISEASES[] = {
    AFFECT_DISEASE_COLD, AFFECT_DISEASE_FLU, AFFECT_DISEASE_FROSTBITE,
    AFFECT_DISEASE_BLEEDING, AFFECT_DISEASE_INFECTION, AFFECT_DISEASE_HERPES,
    AFFECT_DISEASE_BROKEN_BONE, AFFECT_DISEASE_NUMBED_LIMB, AFFECT_DISEASE_VOICEBOX,
    AFFECT_DISEASE_EYEBALL, AFFECT_DISEASE_LUNG, AFFECT_DISEASE_STOMACH,
    AFFECT_DISEASE_HEMORRHAGE, AFFECT_DISEASE_LEPROSY, AFFECT_DISEASE_PLAGUE,
    AFFECT_DISEASE_SUFFOCATE, AFFECT_DISEASE_FOOD_POISONING, AFFECT_DISEASE_DROWNING,
    AFFECT_DISEASE_GARROTTE, AFFECT_DISEASE_SYPHILIS, AFFECT_DISEASE_BRUISED,
    AFFECT_DISEASE_SCURVY, AFFECT_DISEASE_DYSENTERY, AFFECT_DISEASE_PNEUMONIA,
    AFFECT_DISEASE_GANGRENE, AFFECT_DISEASE_EXTREME_PAIN,
};
static const int DRINK_DISEASE_DURATIONS[] = {
    30, 50, 60, 40, 40, 50, 50, 40, 60, 60, 70, 70, 80, 60, 80, 90, 40, 90, 90, 80, 20, 40, 30, 50, 70, 20,
};

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
 * obj.h) isn't wired up yet and is out of scope for this fix.
 *
 * Vital statistics (Sneezy → Tobin feature audit): both branches now also
 * raise thirst (being.h's progress_t, 0-100) -- a clean fountain fully
 * quenches, a grubby puddle only partly does, matching the existing
 * "Blech!"/"Refreshing!" quality distinction already in the messaging. */
#define DRINK_FOUNTAIN_THIRST_GAIN 100
#define DRINK_PUDDLE_THIRST_GAIN 30

/* Liquids (Sneezy -> Tobin feature audit, user 2026-07-26): a real
 * carried OBJ_CAT_DRINK container (waterskin, ale mug, ...) is now also a
 * valid `drink` target, not just a room puddle/fountain -- consumes up to
 * this many val[1] (current) units per drink, applying that fraction of
 * liquid_info()'s per-unit thirst/hunger. See liquids.h's own comment for
 * why the per-unit numbers are scaled the way they are. */
#define DRINK_CONTAINER_UNITS 4
bool cmd_drink(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Usage: drink <puddle|fountain>\r\n");
        return true;
    }
    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);

    /* Ordinal support (user 2026-07-18: "make it true as part of
     * everything that can exist") -- "2.puddle" picks the second
     * qualifying puddle/fountain in room order instead of always the
     * first; plain "puddle" (ordinal defaults to 1) is unchanged. */
    obj_t *pool = NULL, *fount = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!keyword_matches(o->base.name, tok))
            continue;
        bool is_pool = keyword_matches(o->base.name, "puddle");
        bool is_fount = o->category == OBJ_CAT_DRINK;
        if (!is_pool && !is_fount)
            continue;
        seen++;
        if (seen != ordinal)
            continue;
        if (is_pool)
            pool = o;
        else
            fount = o;
        break;
    }

    if (!pool && !fount) {
        obj_t *container = liquid_find_carried_container(ch, raw);
        if (!container) {
            descriptor_send(d, "You don't see that here to drink.\r\n");
            return true;
        }
        if (container->val[1] <= 0) {
            descriptor_send(d, "It's empty.\r\n");
            return true;
        }

        int units = container->val[1] < DRINK_CONTAINER_UNITS ? container->val[1] : DRINK_CONTAINER_UNITS;
        const liquid_type_t *liq = liquid_info(container->val[2]);
        container->val[1] -= units;

        const char *label = container->base.short_descr[0] ? container->base.short_descr : container->base.name;
        char msg[400];
        snprintf(msg, sizeof(msg), "You drink %s from %s.%s\r\n", liq->name, label,
                 container->val[1] <= 0 ? " It's now empty." : "");
        descriptor_send(d, msg);
        snprintf(msg, sizeof(msg), "%s drinks from %s.\r\n", ch->base.name, label);
        descriptor_room_echo(ch->base.roomp, ch, msg);

        if (!being_is_immortal(ch)) {
            int thirst = ch->progress.thirst + liq->thirst * units;
            int hunger = ch->progress.hunger + liq->hunger * units;
            ch->progress.thirst = thirst < 0 ? 0 : (thirst > 100 ? 100 : thirst);
            ch->progress.hunger = hunger < 0 ? 0 : (hunger > 100 ? 100 : hunger);
            /* `alcoholism` (missing-skill audit batch C, 2026-08-09) --
             * see being_gain_drunk()'s own doc comment (vitals.h). */
            being_gain_drunk(ch, liq->drunk * units);
            player_progress_save(ch->player_id, &ch->progress);
        }
        return true;
    }

    if (fount && !pool) {
        const char *label = fount->base.short_descr[0] ? fount->base.short_descr : fount->base.name;
        char msg[320]; /* name (64) + short_descr (128) + fixed text */
        snprintf(msg, sizeof(msg), "You drink some water from %s. Refreshing!\r\n", label);
        descriptor_send(d, msg);
        snprintf(msg, sizeof(msg), "%s drinks from %s.\r\n", ch->base.name, label);
        descriptor_room_echo(ch->base.roomp, ch, msg);
        if (!being_is_immortal(ch)) {
            ch->progress.thirst += DRINK_FOUNTAIN_THIRST_GAIN;
            if (ch->progress.thirst > 100)
                ch->progress.thirst = 100;
            player_progress_save(ch->player_id, &ch->progress);
        }
        return true;
    }

    const char *label = pool->base.short_descr[0] ? pool->base.short_descr : pool->base.name;
    char msg[320]; /* name (64) + short_descr (128) + fixed text */
    snprintf(msg, sizeof(msg), "You scoop up some of %s and drink it. Blech!\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s scoops up some of %s and drinks it.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    if (!being_is_immortal(ch)) {
        ch->progress.thirst += DRINK_PUDDLE_THIRST_GAIN;
        if (ch->progress.thirst > 100)
            ch->progress.thirst = 100;
        player_progress_save(ch->player_id, &ch->progress);
    }

    if (!being_is_immortal(ch) && rand() % 100 < DRINK_POISON_CHANCE_PCT) {
        being_apply_affect(ch, AFFECT_POISON, DRINK_POISON_DURATION);
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
