/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "combat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "descriptor.h"
#include "log.h"
#include "obj.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "room.h"
#include "thing.h"

/* Best-effort message to b's connection, if any -- no-op for a mob (once
 * mobs exist) or a being whose descriptor already went away. */
static void tell(being_t *b, const char *fmt, ...) {
    if (!b || !b->desc)
        return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    descriptor_notify(b->desc, buf); /* held if the recipient is editing */
}

/* A destroyed limb (0% HP) penalizes its owner's own offense -- flat,
 * non-stacking (doesn't get worse with more than one destroyed limb),
 * placeholder amount. There's no hospital system yet to repair it mid-game
 * -- see being_has_destroyed_limb()'s doc comment in being.h. */
#define DESTROYED_LIMB_HIT_PENALTY 15

/* Positions polish (TODO backlog) -- see combat_strike(). Same magnitude as
 * DESTROYED_LIMB_HIT_PENALTY, no particular reason but consistency. */
#define NON_STANDING_HIT_BONUS 15

/* Sneezy-inspired critical hit (Session 42, user: "copy sneezys crit hit
 * system, complete with object creation upon decapitation"). No separate
 * crit-chance roll -- this port triggers purely on a limb's HP actually
 * crossing to 0% from ongoing combat damage (see combat_strike()), reusing
 * the existing per-hit random-limb/damage system rather than adding a new
 * RNG layer. PCs only for now (a dying mob is destroyed outright already,
 * see combat_defeat()); scope confirmed with the user 2026-07-09. Drops a
 * lootable severed-part object in the room; the head specifically is a
 * decapitation, reported back to the caller (combat_strike()'s return
 * value) so it can route through the existing combat_defeat() "slain" path
 * rather than duplicating it. */
static void combat_sever_limb(being_t *attacker, being_t *defender, limb_t limb) {
    if (!defender->base.roomp)
        return;

    const char *ln = limb_name(limb);
    char short_descr[128]; /* matches thing_t.short_descr's own cap (thing.h) --
                               big enough for the longest name (64) + "'s severed "
                               + longest limb name ("left finger") with room to spare */
    char long_descr[200];
    snprintf(short_descr, sizeof(short_descr), "%s's severed %s", defender->base.name, ln);
    snprintf(long_descr, sizeof(long_descr), "%s's severed %s lies here, still twitching.\r\n",
             defender->base.name, ln);

    obj_t *part = obj_create_ephemeral(ln, short_descr, long_descr, OBJ_CAT_TRASH);
    if (part)
        thing_move_to(&part->base, &defender->base.roomp->base);

    tell(attacker, "%s's %s is severed clean off!\r\n", defender->base.name, ln);
    tell(defender, "Your %s is severed clean off!\r\n", ln);
}

/* Case-insensitive "does haystack contain needle" (strcasestr is GNU-only,
 * same style already duplicated in cmd_exec.c/cmd_scan.c/cmd_who.c). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

/* Whichever wielded weapon should flavor THIS attacker's strike message
 * (user, Session 43 continued: "when in combat wielded items should modify
 * messaging for example wield sword, you slice instead of hit"). Checks the
 * dominant hand first, then the off hand -- so a single wielded weapon
 * (the overwhelmingly common case) always drives the verb, and a true
 * dual-wielder's primary-hand weapon wins over their off-hand one rather
 * than alternating with combat_strike()'s hand-swap bookkeeping (which only
 * ever affects damage, not which weapon gets named). NULL if nothing
 * weapon-category is held (bare-handed). */
static obj_t *combat_wielded_weapon(const being_t *attacker) {
    int primary = attacker->handed_right ? 0 : 1;
    int secondary = attacker->handed_right ? 1 : 0;
    if (attacker->held[primary] && attacker->held[primary]->category == OBJ_CAT_WEAPON)
        return attacker->held[primary];
    if (attacker->held[secondary] && attacker->held[secondary]->category == OBJ_CAT_WEAPON)
        return attacker->held[secondary];
    return NULL;
}

/* Keyword-substring verb bucket (same style as sector_color()/
 * room_ground_type() in room.c), matched against the weapon's keyword
 * string and short description -- covers every already-seeded weapon vnum
 * without needing the original's per-item itemTypeT subtype column back.
 * Most-specific/most-common rules first; unrecognized weapon or bare hands
 * both fall through to the plain "hit". */
static const char *weapon_verb(const obj_t *weapon) {
    if (!weapon)
        return "hit";
    const char *n = weapon->base.name;
    const char *s = weapon->base.short_descr;
    if (ci_contains(n, "sword") || ci_contains(s, "sword")
        || ci_contains(n, "blade") || ci_contains(s, "blade")
        || ci_contains(n, "saber") || ci_contains(s, "saber"))
        return "slice";
    if (ci_contains(n, "axe") || ci_contains(s, "axe"))
        return "chop";
    if (ci_contains(n, "mace") || ci_contains(s, "mace")
        || ci_contains(n, "hammer") || ci_contains(s, "hammer")
        || ci_contains(n, "club") || ci_contains(s, "club")
        || ci_contains(n, "staff") || ci_contains(s, "staff"))
        return "bludgeon";
    if (ci_contains(n, "dagger") || ci_contains(s, "dagger")
        || ci_contains(n, "knife") || ci_contains(s, "knife"))
        return "stab";
    if (ci_contains(n, "spear") || ci_contains(s, "spear")
        || ci_contains(n, "pike") || ci_contains(s, "pike")
        || ci_contains(n, "lance") || ci_contains(s, "lance"))
        return "pierce";
    if (ci_contains(n, "whip") || ci_contains(s, "whip")
        || ci_contains(n, "flail") || ci_contains(s, "flail"))
        return "lash";
    return "hit";
}

/* Placeholder damage formula, built from the existing 6-attribute set --
 * not the original's weapon/class/skill-driven system (none of that
 * exists yet). Hit chance skews on relative DEX (and is penalized if the
 * attacker has a destroyed limb); damage scales with STR above ATTR_BASE
 * plus a small random component. Each hit lands on a uniformly-random limb
 * (not the original's slotChance()-weighted roll, see being.h) -- damage
 * is applied to both that limb's HP and the defender's overall HP via
 * being_hurt_limb(). Crossing into a worse limb_status_text() tier (see
 * being.h) announces it to both sides. Returns true iff this hit just
 * decapitated the defender (their head crossed to 0% HP for the first
 * time) -- the caller must route that straight to combat_defeat(). */
static bool combat_strike(being_t *attacker, being_t *defender) {
    /* Weapon-aware messaging + hit/dam bonuses (user, Session 43 continued:
     * "when in combat wielded items should modify messaging for example
     * wield sword, you slice instead of hit. This should apply to all
     * weapon types and add or subtract any hit bonuses placed on the
     * weapon"). Bare-handed (weapon == NULL) yields verb "hit" and 0/0
     * bonuses, so this is a no-op extension of the old formula rather than
     * a behavior change for an unarmed attacker. */
    obj_t *weapon = combat_wielded_weapon(attacker);
    const char *verb = weapon_verb(weapon);
    int weapon_hitroll = 0, weapon_damroll = 0;
    if (weapon)
        obj_load_combat_mods(weapon->vnum, &weapon_hitroll, &weapon_damroll);

    int hit_roll = (rand() % 100) + (attacker->attrs.dexterity - defender->attrs.dexterity) / 4;
    hit_roll += weapon_hitroll;
    if (being_has_destroyed_limb(attacker))
        hit_roll -= DESTROYED_LIMB_HIT_PENALTY;
    /* Positions polish (TODO backlog): a defender who isn't standing --
     * sitting, resting, sleeping, or any of the lower reserved-for-future
     * rungs (see position_t, being.h) -- is an easier target, mirroring
     * the original. Attacking never auto-stands the DEFENDER (only the
     * attacker, cmd_attack.c), so this stays in effect for as long as they
     * choose to stay down. */
    if (defender->position != POSITION_STANDING)
        hit_roll += NON_STANDING_HIT_BONUS;
    if (hit_roll < 50) {
        tell(attacker, "You miss %s!\r\n", defender->base.name);
        tell(defender, "%s misses you!\r\n", attacker->base.name);
        return false;
    }

    int dmg = 1 + (attacker->attrs.strength - ATTR_BASE) / 4 + (rand() % 6) + weapon_damroll;

    /* Handedness (Session 21): strikes alternate hands; the primary hand
     * hits harder (+1), the off-hand weaker (-1). Which hand is primary
     * comes from handed_right chosen at creation. */
    dmg += attacker->off_hand_next ? -1 : 1;
    attacker->off_hand_next = !attacker->off_hand_next;

    if (dmg < 1)
        dmg = 1;

    limb_t limb = (limb_t)(rand() % LIMB_COUNT);
    int pct_before = being_limb_pct(defender, limb);
    being_hurt_limb(defender, limb, dmg);
    int pct_after = being_limb_pct(defender, limb);

    const char *ln = limb_name(limb);
    char verb_3rd[16];
    {
        size_t vlen = strlen(verb);
        bool needs_es = vlen >= 2 && (verb[vlen - 1] == 's' || verb[vlen - 1] == 'x'
                        || verb[vlen - 1] == 'z'
                        || (verb[vlen - 1] == 'h' && (verb[vlen - 2] == 'c' || verb[vlen - 2] == 's')));
        snprintf(verb_3rd, sizeof(verb_3rd), "%s%s", verb, needs_es ? "es" : "s");
    }
    tell(attacker, "You %s %s's %s for %d damage!\r\n", verb, defender->base.name, ln, dmg);
    tell(defender, "%s %s your %s for %d damage!\r\n", attacker->base.name, verb_3rd, ln, dmg);

    const char *status_before = limb_status_text(pct_before);
    const char *status_after = limb_status_text(pct_after);
    if (status_after && status_after != status_before) {
        tell(attacker, "%s's %s %s!\r\n", defender->base.name, ln, status_after);
        tell(defender, "Your %s %s!\r\n", ln, status_after);
    }

    bool decapitated = false;
    if (pct_before > 0 && pct_after == 0 && defender->base.kind == THING_PC) {
        combat_sever_limb(attacker, defender, limb);
        if (limb == LIMB_HEAD)
            decapitated = true;
    }
    return decapitated;
}

/* No permadeath for a PC in the sense of the character record being
 * deleted -- their HP is patched up to half max (so their next login isn't
 * stuck at 0) and their limbs fully heal. But losing now genuinely ends
 * the play session: the loser is unloaded and dropped at the account menu
 * (same path `quit!`-while-playing uses, descriptor_leave_to_menu() in
 * descriptor.c) rather than respawning in-place still playing -- they can
 * pick the same character back up from there, or create/play another, or
 * leave. `slain` only picks the flavor of the first message line (normal
 * combat loss vs. an immortal's instant kill via combat_instakill()) --
 * both end the same way.
 *
 * A MOB loser (Phase 2D) has no player_id row to save and no menu to
 * return to -- it is destroyed outright (permanent, no respawn without a
 * future zone-reset system) instead of HP-patched-and-ejected. */
static void combat_defeat(being_t *loser, being_t *winner, bool slain) {
    loser->fighting = NULL;
    winner->fighting = NULL;

    bool loser_is_pc = (loser->base.kind == THING_PC);

    if (loser_is_pc) {
        loser->progress.hp = loser->progress.max_hp / 2;
        if (loser->progress.hp < 1)
            loser->progress.hp = 1;
        being_limbs_full_heal(loser);
        player_progress_save(loser->player_id, &loser->progress);
    }

    if (slain) {
        tell(winner, "You have slain %s!\r\n", loser->base.name);
        tell(loser, "You have been slain by %s!\r\nYou are DEAD!\r\n", winner->base.name);
    } else {
        tell(winner, "You have defeated %s!\r\n", loser->base.name);
        tell(loser, "You have been defeated by %s!\r\nYou are DEAD!\r\n", winner->base.name);
    }

    /* XP on kill (TODO backlog) -- placeholder reward scaling with the
     * loser's level, same "placeholder, revisit later" precedent as
     * being_calc_max_hp()/progress_xp_for_level(). Immortals don't need XP
     * (already past the mortal ladder), so this only fires for a
     * non-immortal PC winner -- covers a normal HP-based defeat and a
     * decapitation alike, but not an immortal's cmd_kill instakill (the
     * winner there is always an immortal). */
    if (winner->base.kind == THING_PC && !being_is_immortal(winner)) {
        long xp_gain = (long)(loser->progress.level > 0 ? loser->progress.level : 1) * 50;
        int levels_gained = progress_add_xp(&winner->progress, xp_gain);
        tell(winner, "You gain %ld experience points.\r\n", xp_gain);
        if (levels_gained > 0)
            tell(winner, "You feel more experienced!\r\n");
        player_progress_save(winner->player_id, &winner->progress);
    }

    /* Death goes to the log with the loser's IP (user requirement --
     * immortal-visible only, via the log command's gate). */
    log_info("%s has been %s by %s. [%s]", loser->base.name,
             slain ? "slain" : "defeated", winner->base.name,
             loser->desc ? loser->desc->ip : "?");

    /* A death is world news (user requirement): everyone playing -- not
     * just the room -- gets a teasing announcement. Winner and loser are
     * excluded; they already got their own lines above. PC deaths only
     * (user 2026-07-09: "should only fire when a player dies, skip the
     * mobs unless the mob is the killer") -- a mob dying is routine and
     * would spam the world; a mob DOING the killing still broadcasts,
     * since the taunt is about the (player) loser, not the winner. */
    if (loser_is_pc) {
        static const char *const DEATH_TAUNTS[] = {
            "The gods pause their board game to note that %s has been slain by %s.",
            "%s is dead. %s looks insufferably pleased about it.",
            "A distant bell tolls once for %s. %s rang it.",
            "%s's limbs are now a matter of public record, courtesy of %s.",
        };
        char taunt[224];
        int t = rand() % (int)(sizeof(DEATH_TAUNTS) / sizeof(DEATH_TAUNTS[0]));
        /* [INFO] channel prefix (user requirement) -- cyan when color is on,
         * stripped to plain "[INFO]" when off. */
        int n = snprintf(taunt, sizeof(taunt), "\r\n<c>[INFO]<z> ");
        n += snprintf(taunt + n, sizeof(taunt) - (size_t)n, DEATH_TAUNTS[t],
                      loser->base.name, winner->base.name);
        snprintf(taunt + n, sizeof(taunt) - (size_t)n, "\r\n");
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            /* Everyone whose character is in the world -- including editors,
             * who get it held (descriptor_notify), not lost. */
            if (!it->character)
                continue;
            if (it->character == winner || it->character == loser)
                continue;
            descriptor_notify(it, taunt);
        }
    }

    /* Corpse + drop-on-death (Phase 2C, corpse added Session 43 per user:
     * "make it so the corpse of a char loads into the room upon death...
     * treated like a container...mobs and players alike"): a lootable
     * ephemeral corpse container (obj_create_ephemeral(), same primitive as
     * combat_sever_limb()'s severed limbs -- vnum 0, never persisted) drops
     * in the room, and everything the loser has -- carried, worn, or held
     * -- goes INTO it rather than loose on the floor. Not takeable as a
     * whole (wear_flag left 0) and not closed/locked (val[1] 0), so a
     * looter can `get <item> corpse` immediately, no `open` needed. Not
     * wear_flag WEAR_TAKE like other ephemeral objects (obj_create_ephemeral's
     * default) -- overridden below. Kind-agnostic: PCs and mobs both get
     * one, though a mob's is empty today (mobs carry nothing yet). Same
     * safe-unlink-while-iterating pattern as being_destroy(). */
    if (loser->base.roomp) {
        char short_descr[128];
        char long_descr[200];
        snprintf(short_descr, sizeof(short_descr), "the corpse of %s", loser->base.name);
        snprintf(long_descr, sizeof(long_descr), "The corpse of %s lies here.\r\n", loser->base.name);
        obj_t *corpse = obj_create_ephemeral("corpse", short_descr, long_descr, OBJ_CAT_CONTAINER);
        if (corpse) {
            corpse->wear_flag = 0;   /* not takeable as a whole */
            corpse->val[0] = 0;      /* unlimited capacity (0 == unlimited, see cmd_put.c) */
            corpse->val[1] = 0;      /* not closed, not locked -- loot immediately */
            corpse->val[2] = 0;
            corpse->weight = 50;     /* placeholder body weight */
            thing_move_to(&corpse->base, &loser->base.roomp->base);
        }

        thing_t *t = loser->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next;
            if (t->kind == THING_OBJ)
                thing_move_to(t, corpse ? &corpse->base : &loser->base.roomp->base);
            t = next;
        }
        for (int i = 0; i < LIMB_COUNT; i++)
            loser->equipment[i] = NULL;
        for (int i = 0; i < 2; i++)
            loser->held[i] = NULL;
        if (loser_is_pc)
            player_inventory_save(loser->player_id, loser);
    }

    if (loser_is_pc) {
        if (loser->desc)
            descriptor_leave_to_menu(loser->desc);
    } else {
        /* Mob death is permanent (Phase 2D) -- no persistence, no respawn
         * without a future zone-reset system (2E). This also fixes a
         * latent dormant bug: before mobs existed, defeating any
         * desc == NULL being would have silently left it sitting in the
         * room forever, since only this PC branch ever removed anything. */
        being_destroy(loser);
    }
}

void combat_process_run(long pulse_num) {
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *a = d->character;
        if (!a || !a->fighting)
            continue;
        if (a->last_combat_pulse == pulse_num)
            continue; /* already resolved this round via the other participant */

        being_t *b = a->fighting;
        a->last_combat_pulse = pulse_num;
        b->last_combat_pulse = pulse_num;

        bool b_decapitated = combat_strike(a, b);
        if (b->progress.hp <= 0 || b_decapitated) {
            combat_defeat(b, a, b_decapitated);
            continue;
        }

        bool a_decapitated = combat_strike(b, a);
        if (a->progress.hp <= 0 || a_decapitated)
            combat_defeat(a, b, a_decapitated);
    }
}

being_t *combat_find_room_target(being_t *self, const char *name) {
    if (!self || !self->base.roomp || !name || !*name)
        return NULL;

    /* Exact name first, so "Clau" always means the player literally named
     * Clau even if a Claudius is also in the room; then fall back to prefix
     * matching ("kill clau" -> Claudius), same abbreviation convention the
     * command parser has used since Session 9 and the original's
     * is_abbrev()-based get_char_room targeting. First prefix match in room
     * order wins. Matches PCs and mobs alike (Phase 2D) -- thing_name_matches()
     * is per-keyword, so a mob's multi-word name ("vrock demon") is reachable
     * by any one of its words, same as "kill vrock" or "kill demon"; a
     * single-word PC name behaves identically to before. */
    being_t *prefix_match = NULL;
    size_t name_len = strlen(name);
    for (thing_t *t = self->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t == &self->base)
            continue;
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        /* A linkdead PC (user requirement: "no one can manipulate a
         * linkdead char") can't be targeted by name at all -- invisible to
         * attack/kill, same as if they weren't there. */
        if (t->kind == THING_PC && !((being_t *)t)->desc)
            continue;
        if (strcasecmp(t->name, name) == 0)
            return (being_t *)t;
        if (!prefix_match && thing_name_matches(t->name, name, name_len))
            prefix_match = (being_t *)t;
    }
    return prefix_match;
}

void combat_instakill(being_t *attacker, being_t *target) {
    if (!attacker || !target)
        return;

    target->progress.hp = 0;
    for (int i = 0; i < LIMB_COUNT; i++)
        target->limbs[i].hp = 0;

    combat_defeat(target, attacker, true);
}

bool combat_debug_set_limb_hp(being_t *actor, being_t *target, limb_t limb, int hp) {
    if (!target || limb < 0 || limb >= LIMB_COUNT)
        return false;

    int max_hp = target->limbs[limb].max_hp;
    if (hp < 0) hp = 0;
    if (hp > max_hp) hp = max_hp;

    int pct_before = being_limb_pct(target, limb);
    target->limbs[limb].hp = hp;
    int pct_after = being_limb_pct(target, limb);

    const char *ln = limb_name(limb);
    const char *status_before = limb_status_text(pct_before);
    const char *status_after = limb_status_text(pct_after);
    if (status_after && status_after != status_before) {
        tell(actor, "%s's %s %s!\r\n", target->base.name, ln, status_after);
        tell(target, "Your %s %s!\r\n", ln, status_after);
    }

    bool decapitated = false;
    if (pct_before > 0 && pct_after == 0 && target->base.kind == THING_PC) {
        combat_sever_limb(actor, target, limb);
        if (limb == LIMB_HEAD)
            decapitated = true;
    }
    if (decapitated)
        combat_defeat(target, actor, true);
    return decapitated;
}
