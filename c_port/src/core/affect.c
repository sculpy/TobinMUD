/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "affect.h"

#include <stdio.h>
#include <stdlib.h>

#include "being.h"
#include "descriptor.h"
#include "world.h"

/* Indexed identically to affect_type_t -- see affect.h's enum comment for
 * where each of these 26 disease names comes from (upstream DiseaseInfo[],
 * misc/disease.cc), trimmed to fit a 16-wide `affects` column. */
static const char *const AFFECT_NAMES[AFFECT_COUNT] = {
    "(none)",
    "Sanctuary",
    "Poison",
    "Cold",
    "Flu",
    "Frostbite",
    "Bleeding",
    "Infection",
    "Herpes",
    "Broken Bone",
    "Numbed Limb",
    "Voicebox",
    "Eyeball",
    "Lung",
    "Stomach Wound",
    "Internal Bleeding",
    "Leprosy",
    "Plague",
    "Suffocation",
    "Food Poisoning",
    "Drowning",
    "Garrotte",
    "Syphilis",
    "Bruised",
    "Scurvy",
    "Dysentery",
    "Pneumonia",
    "Gangrene",
    "Extreme Pain",
};

/* HP drained per damage sub-tick for AFFECT_POISON -- its own faster gate
 * (POISON_TICK_EVERY below) than a disease's, so a poisoning reads as
 * more urgent than catching a cold. */
#define POISON_HP_DRAIN 2
#define POISON_TICK_EVERY 5

/* HP drained per damage sub-tick (see affect_tick_run()'s DISEASE_TICK_
 * EVERY gate) for each AFFECT_DISEASE_* value, indexed the same as
 * AFFECT_NAMES. Non-disease entries are unused (0). Roughly ranked by the
 * upstream DiseaseInfo[].cure_cost ordering (misc/disease.cc) -- NOT a
 * linear rescale of it (the real range spans 100 to 100000), just the
 * same relative "worse diseases hit harder" ordering, rescaled into
 * Tobin's own "modest list" numbers like the original 4 always were. */
static const int DISEASE_HP_DRAIN[AFFECT_COUNT] = {
    [AFFECT_DISEASE_COLD] = 1,
    [AFFECT_DISEASE_FLU] = 2,
    [AFFECT_DISEASE_FROSTBITE] = 3,
    [AFFECT_DISEASE_BLEEDING] = 2,
    [AFFECT_DISEASE_INFECTION] = 2,
    [AFFECT_DISEASE_HERPES] = 2,
    [AFFECT_DISEASE_BROKEN_BONE] = 3,
    [AFFECT_DISEASE_NUMBED_LIMB] = 2,
    [AFFECT_DISEASE_VOICEBOX] = 3,
    [AFFECT_DISEASE_EYEBALL] = 3,
    [AFFECT_DISEASE_LUNG] = 4,
    [AFFECT_DISEASE_STOMACH] = 4,
    [AFFECT_DISEASE_HEMORRHAGE] = 5,
    [AFFECT_DISEASE_LEPROSY] = 3,
    [AFFECT_DISEASE_PLAGUE] = 4,
    [AFFECT_DISEASE_SUFFOCATE] = 6,
    [AFFECT_DISEASE_FOOD_POISONING] = 3,
    [AFFECT_DISEASE_DROWNING] = 6,
    [AFFECT_DISEASE_GARROTTE] = 6,
    [AFFECT_DISEASE_SYPHILIS] = 4,
    [AFFECT_DISEASE_BRUISED] = 1,
    [AFFECT_DISEASE_SCURVY] = 2,
    [AFFECT_DISEASE_DYSENTERY] = 2,
    [AFFECT_DISEASE_PNEUMONIA] = 3,
    [AFFECT_DISEASE_GANGRENE] = 4,
    [AFFECT_DISEASE_EXTREME_PAIN] = 1,
};

/* Hospital cure price (cmd_shop.c) for AFFECT_POISON and every
 * AFFECT_DISEASE_* value -- see affect_cure_price()'s header comment. */
static const int AFFECT_CURE_PRICE[AFFECT_COUNT] = {
    [AFFECT_POISON] = 20,
    [AFFECT_DISEASE_COLD] = 10,
    [AFFECT_DISEASE_FLU] = 25,
    [AFFECT_DISEASE_FROSTBITE] = 55,
    [AFFECT_DISEASE_BLEEDING] = 15,
    [AFFECT_DISEASE_INFECTION] = 20,
    [AFFECT_DISEASE_HERPES] = 28,
    [AFFECT_DISEASE_BROKEN_BONE] = 30,
    [AFFECT_DISEASE_NUMBED_LIMB] = 25,
    [AFFECT_DISEASE_VOICEBOX] = 90,
    [AFFECT_DISEASE_EYEBALL] = 90,
    [AFFECT_DISEASE_LUNG] = 65,
    [AFFECT_DISEASE_STOMACH] = 70,
    [AFFECT_DISEASE_HEMORRHAGE] = 80,
    [AFFECT_DISEASE_LEPROSY] = 35,
    [AFFECT_DISEASE_PLAGUE] = 75,
    [AFFECT_DISEASE_SUFFOCATE] = 140,
    [AFFECT_DISEASE_FOOD_POISONING] = 35,
    [AFFECT_DISEASE_DROWNING] = 140,
    [AFFECT_DISEASE_GARROTTE] = 150,
    [AFFECT_DISEASE_SYPHILIS] = 130,
    [AFFECT_DISEASE_BRUISED] = 12,
    [AFFECT_DISEASE_SCURVY] = 22,
    [AFFECT_DISEASE_DYSENTERY] = 18,
    [AFFECT_DISEASE_PNEUMONIA] = 32,
    [AFFECT_DISEASE_GANGRENE] = 45,
    [AFFECT_DISEASE_EXTREME_PAIN] = 8,
};

/* Looks up the display name for an affect type, e.g. for the `affects`
 * command or an expiry message. Falls back to "(unknown)" for a bad
 * value rather than reading out of bounds. */
const char *affect_name(affect_type_t type) {
    if (type < 0 || type >= AFFECT_COUNT)
        return "(unknown)";
    return AFFECT_NAMES[type];
}

bool affect_is_disease(affect_type_t type) {
    return type >= AFFECT_DISEASE_COLD && type <= AFFECT_DISEASE_EXTREME_PAIN;
}

int affect_cure_price(affect_type_t type) {
    if (type < 0 || type >= AFFECT_COUNT)
        return 0;
    return AFFECT_CURE_PRICE[type];
}

affect_type_t affect_random_disease(void) {
    int span = AFFECT_DISEASE_EXTREME_PAIN - AFFECT_DISEASE_COLD + 1;
    return (affect_type_t)(AFFECT_DISEASE_COLD + rand() % span);
}

/* Checks whether `b` currently has a given buff/debuff active by
 * scanning their small fixed list of active affects. */
bool being_has_affect(const struct being *b, affect_type_t type) {
    if (!b || type == AFFECT_NONE)
        return false;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++)
        if (b->affects[i].type == type)
            return true;
    return false;
}

/* Starts a buff/debuff on `b`, or -- if it's already active -- just
 * resets its remaining duration back to `rounds` (casting the same
 * spell on yourself again refreshes it instead of stacking a second
 * copy). If none of `b`'s affect slots already hold this type and none
 * are free, this is a no-op -- the fixed-size list is deliberately
 * small (MAX_ACTIVE_AFFECTS) rather than growable. */
void being_apply_affect(struct being *b, affect_type_t type, int rounds) {
    if (!b || type == AFFECT_NONE || rounds <= 0)
        return;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (b->affects[i].type == type) {
            b->affects[i].rounds_left = rounds;
            return;
        }
    }
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (b->affects[i].type == AFFECT_NONE) {
            b->affects[i].type = type;
            b->affects[i].rounds_left = rounds;
            return;
        }
    }
    /* All slots full of OTHER affects -- deliberately dropped, not
     * queued; a being juggling more than MAX_ACTIVE_AFFECTS at once is
     * an edge case this v1 doesn't need to solve. */
}

/* Ends a buff/debuff on `b` right away, if they have it -- used when
 * something explicitly cancels an affect (as opposed to it simply
 * running out, which affect_tick_run() handles). */
void being_remove_affect(struct being *b, affect_type_t type) {
    if (!b || type == AFFECT_NONE)
        return;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (b->affects[i].type == type) {
            b->affects[i].type = AFFECT_NONE;
            b->affects[i].rounds_left = 0;
            return;
        }
    }
}

/* A disease deals its DISEASE_HP_DRAIN[] damage only every 10th round
 * (not every affect_tick_run() call -- COMBAT_ROUND_PULSES is ~1.2s, so
 * every round would drain a "modest" disease's whole duration in HP
 * within seconds) -- stateless, just a modulus on the same rounds_left
 * counter everything else already uses, no extra field needed. */
#define DISEASE_TICK_EVERY 10

/* The three per-tick notices below (disease flare-up, poison burn, wears
 * off) each need two phrasings: "Your X ..." sent straight to a connected
 * PC's own descriptor, or "<Name>'s X ..." echoed to the room for a mob
 * (user 2026-07-18: "include affects for players and NPCs" -- a mob has
 * no descriptor to send a first-person message to, but everyone standing
 * there can still see it happen to them). `d` is NULL for a mob. */
static void notify_flare(being_t *b, descriptor_t *d, affect_type_t type) {
    if (d) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Your %s flares up, sapping your strength.\r\n", affect_name(type));
        descriptor_send(d, msg);
    } else if (b->base.roomp) {
        char namebuf[64], msg[128];
        being_display_name_cap(b, namebuf, sizeof(namebuf));
        snprintf(msg, sizeof(msg), "%s's %s flares up.\r\n", namebuf, affect_name(type));
        descriptor_room_echo(b->base.roomp, NULL, msg);
    }
}

static void notify_poison_burn(being_t *b, descriptor_t *d) {
    if (d) {
        descriptor_send(d, "The poison in your veins burns anew!\r\n");
    } else if (b->base.roomp) {
        char namebuf[64], msg[128];
        being_display_name_cap(b, namebuf, sizeof(namebuf));
        snprintf(msg, sizeof(msg), "The poison in %s's veins burns anew!\r\n", namebuf);
        descriptor_room_echo(b->base.roomp, NULL, msg);
    }
}

static void notify_wears_off(being_t *b, descriptor_t *d, affect_type_t type) {
    if (d) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Your %s wears off.\r\n", affect_name(type));
        descriptor_send(d, msg);
    } else if (b->base.roomp) {
        char namebuf[64], msg[96];
        being_display_name_cap(b, namebuf, sizeof(namebuf));
        snprintf(msg, sizeof(msg), "%s's %s wears off.\r\n", namebuf, affect_name(type));
        descriptor_room_echo(b->base.roomp, NULL, msg);
    }
}

/* Ticks every one of `b`'s active buffs/debuffs down by one round,
 * clearing (with a "wears off" notice) any that just hit zero. Diseases
 * and poison also drain HP on their own slower sub-tick along the way --
 * see DISEASE_TICK_EVERY/POISON_TICK_EVERY -- clamped so it can never
 * drop anyone below 1 HP outside combat (same non-lethal convention
 * cmd_drink.c's original poison roll used), and skipped outright for an
 * immortal (TODO.md: "immortals immune" -- covers a being promoted
 * mid-disease, not just the infection roll itself). `d` is the being's
 * live descriptor for a connected PC, or NULL for a mob -- see the
 * notify_*() helpers above for how each is announced. */
static void tick_being_affects(being_t *b, descriptor_t *d) {
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (b->affects[i].type == AFFECT_NONE)
            continue;
        affect_type_t type = b->affects[i].type;
        b->affects[i].rounds_left--;
        if (affect_is_disease(type) && !being_is_immortal(b)
            && b->affects[i].rounds_left % DISEASE_TICK_EVERY == 0) {
            b->progress.hp -= DISEASE_HP_DRAIN[type];
            if (b->progress.hp < 1)
                b->progress.hp = 1;
            notify_flare(b, d, type);
        }
        if (type == AFFECT_POISON && !being_is_immortal(b)
            && b->affects[i].rounds_left % POISON_TICK_EVERY == 0) {
            b->progress.hp -= POISON_HP_DRAIN;
            if (b->progress.hp < 1)
                b->progress.hp = 1;
            notify_poison_burn(b, d);
        }
        if (b->affects[i].rounds_left <= 0) {
            notify_wears_off(b, d, type);
            b->affects[i].type = AFFECT_NONE;
            b->affects[i].rounds_left = 0;
        }
    }
}

static void mob_affect_tick_visit(being_t *m) {
    tick_being_affects(m, NULL);
}

/* Runs on a timer (see main.c): ticks affects for every connected PC
 * (regardless of whether they're currently fighting -- a buff keeps
 * wearing off even outside combat) AND every mob in the world
 * (world_for_each_mob(), same iteration primitive mob_ai.c's own pulse
 * uses) -- user 2026-07-18: "include affects for players and NPCs". */
void affect_tick_run(long pulse_num) {
    (void)pulse_num;
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b)
            continue;
        tick_being_affects(b, d);
    }
    world_for_each_mob(mob_affect_tick_visit);
}
