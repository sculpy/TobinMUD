/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "combat.h"

/* Matches `name` against limb_name(), ignoring spaces/case in both, so an
 * immortal can type "leftarm" for "left arm" without quoting. Immortal-only
 * debug tool, see cmd_crit(). */
static limb_t limb_from_name(const char *name) {
    char norm[32];
    int j = 0;
    for (int i = 0; name[i] && j < (int)sizeof(norm) - 1; i++)
        if (!isspace((unsigned char)name[i]))
            norm[j++] = (char)tolower((unsigned char)name[i]);
    norm[j] = '\0';

    for (int limb = 0; limb < LIMB_COUNT; limb++) {
        const char *ln = limb_name((limb_t)limb);
        char cand[32];
        int k = 0;
        for (int i = 0; ln[i] && k < (int)sizeof(cand) - 1; i++)
            if (!isspace((unsigned char)ln[i]))
                cand[k++] = (char)tolower((unsigned char)ln[i]);
        cand[k] = '\0';
        if (strcmp(cand, norm) == 0)
            return (limb_t)limb;
    }
    return (limb_t)-1;
}

/* Immortal-only debug/testing tool (Session 42): `crit <target> <limb>
 * <hp>` sets a limb's HP directly, so the sever/decapitate system (see
 * combat_debug_set_limb_hp() in combat.c) can be exercised deterministically
 * instead of waiting on combat RNG to land on a specific limb. Not a normal
 * gameplay command -- no analogue in the original (Sneezy has no equivalent
 * either; QA there relies on the real RNG at scale). */
bool cmd_crit(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char target_name[64] = "";
    char limb_arg[32] = "";
    int hp = 0;
    if (sscanf(args, "%63s %31s %d", target_name, limb_arg, &hp) != 3) {
        descriptor_send(d, "Usage: crit <target> <limb> <hp>\r\n");
        return true;
    }

    being_t *target = NULL;
    if (strcasecmp(target_name, "self") == 0 || strcasecmp(target_name, d->character->base.name) == 0)
        target = d->character;
    else
        target = combat_find_room_target(d->character, target_name);
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }

    limb_t limb = limb_from_name(limb_arg);
    if (limb == (limb_t)-1) {
        descriptor_send(d, "No such limb.\r\n");
        return true;
    }

    /* Any MAJOR limb destroyed (head/neck/waist/body, user 2026-07-12) is
     * instant death now, not just a decapitation specifically. */
    bool instadeath = combat_debug_set_limb_hp(d->character, target, limb, hp);
    descriptor_send(d, instadeath ? "Instant death (major limb destroyed).\r\n" : "Limb HP set.\r\n");
    return true;
}
