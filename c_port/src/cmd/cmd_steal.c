/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "combat.h"
#include "obj.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `steal gold <target>` / `steal <item> <target>` (spell/skill functional-
 * completeness audit, 2026-07-27: Thief roster entry "steal", skill.c
 * level 1, SKILL_TIER_COMBAT). The reverse of cmd_plant.c's Thief
 * reverse-pickpocket (do_thief_plant()) -- reuses its exact gate/chance
 * shape (out of combat, non-immortal target, mutual `toggle pk` consent
 * via combat_pk_allowed(), level-gap-scaled success chance) rather than
 * inventing a new one, just moving an item/gold FROM the victim TO the
 * thief instead of the other way around. "gold" is a reserved keyword
 * (steals a level-scaled percentage of the victim's wallet, same
 * "no per-object weight data" placeholder-formula spirit as bash/kick's
 * own damage rolls); any other token looks up a specific LOOSE (not
 * worn/held -- same is_loose() rule cmd_peek.c's inventory-peek already
 * uses) carried item by keyword. A failed roll takes nothing but tips
 * the victim off (plant's own "something seemed suspicious" precedent);
 * this port does not additionally start a fight on a failed steal, a
 * disclosed simplification versus Sneezy's own doStealFail() (which can
 * provoke the victim into retaliating) -- Tobin has no equivalent of the
 * original's separate "provoked" combat-initiation path to reuse. */

static bool is_loose(const being_t *b, const obj_t *o) {
    if (b->held[0] == o || b->held[1] == o)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (b->equipment[i] == o)
            return false;
    return true;
}

static obj_t *find_loose_carried(const being_t *b, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = b->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!is_loose(b, o))
            continue;
        if (thing_name_matches(t->name, tok, len))
            return o;
    }
    return NULL;
}

static being_t *find_in_room(const being_t *self, const char *tok) {
    size_t len = strlen(tok);
    for (thing_t *t = self->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (t == &self->base)
            continue;
        if (t->kind == THING_PC && !((being_t *)t)->desc)
            continue; /* linkdead body -- not a valid steal target */
        if (thing_name_matches(t->name, tok, len))
            return (being_t *)t;
    }
    return NULL;
}

/* Shared gate/chance logic for both the gold and item branches below --
 * see this file's header comment for the full rationale. Returns the
 * roll chance (5-95) on success, or -1 if the attempt was refused
 * outright (a message was already sent to `d`). */
static int steal_check(descriptor_t *d, being_t *ch, being_t *vict) {
    if (!being_is_immortal(ch) && !being_knows_skill(ch, "steal")) {
        descriptor_send(d, "You don't know how to steal.\r\n");
        return -1;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're fighting!\r\n");
        return -1;
    }
    if (vict == ch) {
        descriptor_send(d, "Come on now, that's rather stupid!\r\n");
        return -1;
    }
    if (being_is_immortal(vict)) {
        descriptor_send(d, "You can't steal from an immortal.\r\n");
        return -1;
    }
    if (!combat_pk_allowed(ch, vict)) {
        descriptor_send(d, "You'd need `toggle pk` (and so would they) to try that on a player.\r\n");
        return -1;
    }

    bool imm = being_is_immortal(ch);
    const skill_def_t *sk = skill_find(ch->char_class, "steal", imm);
    int pct = imm || !sk ? 100 : skill_learn_from_doing(ch, sk);
    /* Same level-gap/sleeping-bonus shape as do_thief_plant()'s
     * getPlantThiefChance() simplification. */
    pct += (ch->progress.level - vict->progress.level) * 2;
    if (vict->position == POSITION_SLEEPING)
        pct += 25;
    /* `counter steal` (Thief, missing-skill audit, 2026-08-09): real
     * upstream help text -- "gives the thief the ability to detect when
     * another thief is attempting to steal from them and to block that
     * attempt. It relies significantly on the thieves knowledge of
     * stealing." Ported as a straight proficiency-vs-proficiency
     * subtraction from the attacker's own chance above: the victim's
     * `counter steal` skill (learned-by-doing like every other roster
     * entry, "relies... on stealing knowledge" reflected by also
     * requiring the victim actually knows `steal` itself, real
     * upstream's own thief-vs-thief framing) directly eats into the
     * thief's roll, on top of (not instead of) the normal 5-95 clamp
     * below. */
    if (!being_is_immortal(vict) && being_knows_skill(vict, "counter steal")
        && being_knows_skill(vict, "steal")) {
        const skill_def_t *cs_sk = skill_find(vict->char_class, "counter steal", false);
        if (cs_sk)
            pct -= skill_learn_from_doing(vict, cs_sk) / 2;
    }
    if (pct < 5)
        pct = 5;
    if (pct > 95)
        pct = 95;
    return pct;
}

bool cmd_steal(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char obj_tok[64] = "", vict_tok[64] = "";
    int nargs = sscanf(args, "%63s %63s", obj_tok, vict_tok);
    if (nargs < 2) {
        descriptor_send(d, "Steal what from whom (steal gold <target>, or steal <item> <target>)?\r\n");
        return true;
    }

    being_t *vict = find_in_room(ch, vict_tok);
    if (!vict) {
        descriptor_send(d, "You don't see that person.\r\n");
        return true;
    }

    int pct = steal_check(d, ch, vict);
    if (pct < 0)
        return true;

    char msg[320];
    bool wants_gold = strcasecmp(obj_tok, "gold") == 0 || strcasecmp(obj_tok, "coins") == 0;

    if (wants_gold) {
        if (vict->progress.gold <= 0) {
            descriptor_send(d, "They aren't carrying any gold.\r\n");
            return true;
        }
        if (!skill_roll_success(pct)) {
            descriptor_send(d, "You reach for their purse, but come up empty-handed.\r\n");
            if (vict->desc)
                descriptor_send(vict->desc, "You feel a hand brush against your purse!\r\n");
            return true;
        }
        int share = 10 + rand() % 31; /* 10-40% of their wallet */
        int amount = vict->progress.gold * share / 100;
        if (amount < 1)
            amount = 1;
        if (amount > vict->progress.gold)
            amount = vict->progress.gold;
        vict->progress.gold -= amount;
        ch->progress.gold += amount;
        player_progress_save(vict->player_id, &vict->progress);
        player_progress_save(ch->player_id, &ch->progress);

        snprintf(msg, sizeof(msg), "You deftly lift %d gold from %s's purse!\r\n",
                 amount, being_display_name(vict));
        descriptor_send(d, msg);
        return true;
    }

    obj_t *obj = find_loose_carried(vict, obj_tok);
    if (!obj) {
        descriptor_send(d, "They don't seem to be carrying that.\r\n");
        return true;
    }

    const char *label = obj->base.short_descr[0] ? obj->base.short_descr : obj->base.name;
    if (!skill_roll_success(pct)) {
        snprintf(msg, sizeof(msg), "You try to lift %.127s from %.63s, but they notice!\r\n",
                 label, vict->base.name);
        descriptor_send(d, msg);
        if (vict->desc)
            descriptor_send(vict->desc, "You feel a hand in your belongings!\r\n");
        return true;
    }

    thing_move_to(&obj->base, &ch->base);
    if (vict->base.kind == THING_PC)
        player_inventory_save(vict->player_id, vict);
    if (ch->base.kind == THING_PC)
        player_inventory_save(ch->player_id, ch);

    snprintf(msg, sizeof(msg), "You deftly lift %.127s from %.63s without them noticing a thing.\r\n",
             label, vict->base.name);
    descriptor_send(d, msg);
    return true;
}
