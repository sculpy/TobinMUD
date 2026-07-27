/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "combat.h"
#include "log.h"
#include "obj.h"
#include "obj_plant.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* Two unrelated mechanics share the real `plant` command (Sneezy →
 * Tobin feature audit, "Planting"): seed farming (`plant <seeds>`) and the
 * Thief's reverse-pickpocket (`plant <item> <victim>`) -- dispatched the
 * same way the original's doPlant() does: an object arg with NO second
 * arg is seed farming, anything with a second arg is the Thief version.
 * See obj_plant.h/planting.h for seed farming's own doc comments. */

static bool obj_name_matches(const char *keywords, const char *tok, size_t tok_len) {
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
        if (!*p)
            break;
    }
    return false;
}

/* First carried object (no ordinal/loose-only filtering) matching `tok`
 * by keyword -- the seed/item lookup shared by both `plant` branches. */
static obj_t *find_carried(const being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        if (obj_name_matches(t->name, tok, len))
            return (obj_t *)t;
    }
    return NULL;
}

/* Deliberately does NOT skip `self` -- do_thief_plant()'s own `vict == ch`
 * check needs to actually be reachable (matches the original's
 * genericCanPlantThief(), which checks self-targeting AFTER a successful
 * find, not by excluding it from the search). */
static being_t *find_in_room(const being_t *self, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = self->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (t->kind == THING_PC && !((being_t *)t)->desc)
            continue; /* linkdead body -- not a valid plant target */
        if (thing_name_matches(t->name, tok, len))
            return (being_t *)t;
    }
    return NULL;
}

/* `plant <seeds>` -- the seed-farming half of `plant` (see the file's top
 * comment for the dispatch rule). Validates the seeds, room, and an
 * 8-plant-per-room cap, then starts a 3-tick planting action tracked on
 * `ch` -- the actual plant object is created once the ticks finish
 * (see obj_plant.h/planting.h). */
static void do_seed_plant(descriptor_t *d, being_t *ch, const char *seed_tok) {
    obj_t *seed = find_carried(ch, seed_tok);
    int type;
    if (!seed || !plant_type_for_seed_vnum(seed->vnum, &type)) {
        descriptor_send(d, "You need to specify some seeds to plant.\r\n");
        return;
    }
    if (ch->planting_ticks_left > 0) {
        descriptor_send(d, "You're already busy planting something.\r\n");
        return;
    }
    if (ch->fighting) {
        descriptor_send(d, "You can't properly plant seeds while under attack.\r\n");
        return;
    }
    if (!room_can_plant(ch->base.roomp)) {
        descriptor_send(d, "You can't plant anything here.\r\n");
        return;
    }

    int count = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next)
        if (t->kind == THING_OBJ && ((obj_t *)t)->raw_type == OBJ_PLANT_RAW_TYPE)
            count++;
    if (count >= 8) {
        descriptor_send(d, "There isn't any room for more plants in here.\r\n");
        return;
    }

    ch->planting_seed = seed;
    ch->planting_type = type;
    ch->planting_ticks_left = 3;
    ch->planting_room = ch->base.roomp;

    descriptor_send(d, "You begin to plant some seeds.\r\n");
    char cap[128], msg[192];
    being_display_name_cap(ch, cap, sizeof(cap));
    snprintf(msg, sizeof(msg), "%s begins planting some seeds.\r\n", cap);
    descriptor_room_echo(ch->base.roomp, ch, msg);
}

/* `plant <item> <victim>` -- the Thief reverse-pickpocket half of `plant`:
 * slips a carried item onto another being's person. Gates on the "plant"
 * skill, empty hands, out-of-combat, a valid non-immortal target, and (for
 * PC victims) mutual PK consent, then rolls a level/sleeping-adjusted
 * success chance -- a failed roll still plants the item but tips the
 * victim off. */
static void do_thief_plant(descriptor_t *d, being_t *ch, const char *obj_tok, const char *vict_tok) {
    /* Gated on the "plant" skill itself, not "steal" (the original's
     * genericCanPlantThief() oddly checks doesKnowSkill(SKILL_STEAL) even
     * though the success chance below uses SKILL_PLANT) -- Tobin's own
     * `steal` isn't wired up as a command yet either, so "plant" is the
     * only real gate available. */
    if (!being_knows_skill(ch, "plant")) {
        descriptor_send(d, "You know nothing about planting things on people.\r\n");
        return;
    }
    if (ch->held[0] || ch->held[1]) {
        descriptor_send(d, "It's impossible to plant something with your hand(s) already full!\r\n");
        return;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return;
    }

    obj_t *obj = find_carried(ch, obj_tok);
    if (!obj) {
        descriptor_send(d, "You don't have that object.\r\n");
        return;
    }

    being_t *vict = find_in_room(ch, vict_tok);
    if (!vict) {
        descriptor_send(d, "You don't see that person.\r\n");
        return;
    }
    if (vict == ch) {
        descriptor_send(d, "Come on now, that's rather stupid!\r\n");
        return;
    }
    if (being_is_immortal(vict)) {
        descriptor_send(d, "You can't plant on an immortal.\r\n");
        return;
    }
    /* Sneezy's real gate here is a peaceful-room flag Tobin has no
     * equivalent of yet -- reusing the SAME mutual `toggle pk` consent
     * gate combat itself already requires for PC-vs-PC hostility (mobs
     * are always fair game) is a disclosed adaptation, not a silent
     * scope cut. */
    if (!combat_pk_allowed(ch, vict)) {
        descriptor_send(d, "You'd need `toggle pk` (and so would they) to try that on a player.\r\n");
        return;
    }

    const skill_def_t *sk = skill_find(CLASS_THIEF, "plant", false);
    int pct = sk ? skill_learn_from_doing(ch, sk) : 0;
    /* Level-difference modifier, same spirit as the original's
     * getPlantThiefChance() (thief/victim level gap, sleeping bonus)
     * simplified to two terms rather than porting its full dex/luck/
     * drunk formula wholesale -- Tobin has no "luck"/drunk-condition
     * stat to plug in here. */
    pct += (ch->progress.level - vict->progress.level) * 2;
    if (vict->position == POSITION_SLEEPING)
        pct += 25;
    if (pct < 5)
        pct = 5;
    if (pct > 95)
        pct = 95;

    thing_move_to(&obj->base, &vict->base);
    const char *label = obj->base.short_descr[0] ? obj->base.short_descr : obj->base.name;

    if (skill_roll_success(pct)) {
        char msg[320];
        snprintf(msg, sizeof(msg), "You slip %.127s onto %.63s without them noticing a thing.\r\n",
                 label, vict->base.name);
        descriptor_send(d, msg);
    } else {
        char msg[320];
        snprintf(msg, sizeof(msg), "You plant %.127s on %.63s, but something seemed suspicious.\r\n",
                 label, vict->base.name);
        descriptor_send(d, msg);
        if (vict->desc)
            descriptor_send(vict->desc, "That seemed suspicious.\r\n");
    }
}

/* The `plant` command: dispatches between seed farming and the Thief's
 * reverse-pickpocket based purely on argument count, matching the
 * original's own doPlant() (see the file's top comment). */
bool cmd_plant(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char obj_tok[64] = "", vict_tok[64] = "";
    int nargs = sscanf(args, "%63s %63s", obj_tok, vict_tok);
    if (nargs < 1) {
        descriptor_send(d, "Plant what (or plant what on whom)?\r\n");
        return true;
    }

    if (nargs == 1)
        do_seed_plant(d, ch, obj_tok);
    else
        do_thief_plant(d, ch, obj_tok, vict_tok);

    return true;
}
