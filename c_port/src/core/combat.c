/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "combat.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "balance.h"
#include "descriptor.h"
#include "log.h"
#include "obj.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "room.h"
#include "skill.h"
#include "thing.h"
#include "trigger.h"

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
    /* defender->base.name is a mob's raw keyword list ("lady stroll
     * walk"), not a display string, if defender is a mob (2026-07-11
     * capitalization audit) -- being_display_name() gives the lowercase
     * short_descr-based form (matching short_descr's own "stored
     * lowercase-first" convention), being_display_name_cap() the
     * capitalized sentence-initial form long_descr needs. */
    char sever_capbuf[128];
    snprintf(short_descr, sizeof(short_descr), "%s's severed %s", being_display_name(defender), ln);
    /* No trailing \r\n -- cmd_look.c's room-floor listing and `look <item>`
     * both append their own "\r\n" after long_descr (matching every real
     * seeded object's long_desc convention, plain text with no terminator);
     * baking one in here doubled up into a blank line, same bug class the
     * user reported for the pee/blood pools (2026-07-11). */
    snprintf(long_descr, sizeof(long_descr), "%s's severed %s lies here, still twitching.",
             being_display_name_cap(defender, sever_capbuf, sizeof(sever_capbuf)), ln);

    obj_t *part = obj_create_ephemeral(ln, short_descr, long_descr, OBJ_CAT_TRASH);
    if (part)
        thing_move_to(&part->base, &defender->base.roomp->base);

    tell(attacker, "%s's %s is severed clean off!\r\n",
         being_display_name_cap(defender, sever_capbuf, sizeof(sever_capbuf)), ln);
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

    int base_roll = rand() % 100;
    int modifier = (attacker->attrs.dexterity - defender->attrs.dexterity) / 4;
    modifier += weapon_hitroll;
    if (being_has_destroyed_limb(attacker))
        modifier -= DESTROYED_LIMB_HIT_PENALTY;
    /* Positions polish (TODO backlog): a defender who isn't standing --
     * sitting, resting, sleeping, or any of the lower reserved-for-future
     * rungs (see position_t, being.h) -- is an easier target, mirroring
     * the original. Attacking never auto-stands the DEFENDER (only the
     * attacker, cmd_attack.c), so this stays in effect for as long as they
     * choose to stay down. */
    if (defender->position != POSITION_STANDING)
        modifier += NON_STANDING_HIT_BONUS;
    /* Armor class (user 2026-07-11: "Armor & protection... complete the
     * to-hit/defense formula depth"): a defender's total worn AC
     * (being_total_ac(), obj.h) makes them harder to hit. Scaled by half
     * so it sits in the same rough magnitude as the other single
     * modifiers above rather than dominating them. */
    modifier -= being_total_ac(defender) / 2;

    /* Gamewide to-hit modifier (user 2026-07-12's `balance` command) --
     * a PC's own class+race, or a guildmaster mob's known class (mobs
     * have no race). Neutral (0) until an immortal actually balances
     * that class/race. */
    if (attacker->base.kind == THING_PC) {
        modifier += class_balance_get(attacker->char_class)->tohit_mod;
        modifier += race_balance_get(attacker->race)->tohit_mod;
    } else if (attacker->mob_class_known) {
        modifier += class_balance_get(attacker->char_class)->tohit_mod;
    }

    /* Guaranteed hit/miss zones (Sneezy's to-hit model): clamp the
     * modifier total, not the final roll, so an extreme stat/gear
     * mismatch can never make a hit or a miss impossible outright --
     * base_roll alone always keeps a ~6% chance of the "wrong" outcome
     * either way. */
    if (modifier > 44)
        modifier = 44;
    else if (modifier < -44)
        modifier = -44;

    int hit_roll = base_roll + modifier;
    if (hit_roll < 50) {
        /* nospam (user 2026-07-11, ported from Sneezy's AUTO_NOSPAM): each
         * viewer's own toggle decides whether THEY see a miss -- the
         * attacker and defender are checked independently, same as the
         * original. */
        if (!(attacker->pflags & PLR_NOSPAM))
            tell(attacker, "You miss %s!\r\n", being_display_name(defender));
        if (!(defender->pflags & PLR_NOSPAM)) {
            char miss_capbuf[128];
            tell(defender, "%s misses you!\r\n",
                 being_display_name_cap(attacker, miss_capbuf, sizeof(miss_capbuf)));
        }
        return false;
    }

    int dmg = 1 + (attacker->attrs.strength - ATTR_BASE) / 4 + (rand() % 6) + weapon_damroll;

    /* Handedness (Session 21): strikes alternate hands; the primary hand
     * hits harder (+1), the off-hand weaker (-1). Which hand is primary
     * comes from handed_right chosen at creation. Weapon depth (user
     * 2026-07-12): the "dual wield" skill (skill.c's roster --
     * Warrior/Thief) "passively reduces the damage penalty for your
     * off-hand weapon", so a dual-wield-trained attacker's off-hand
     * strike loses its -1 (a plain 0, same as bare-handed) instead of
     * being worse than their main-hand default. */
    if (attacker->off_hand_next)
        dmg += being_knows_skill(attacker, "dual wield") ? 0 : -1;
    else
        dmg += 1;
    attacker->off_hand_next = !attacker->off_hand_next;

    /* Weapon sharpness (user 2026-07-12, weapon depth): an edged/
     * piercing weapon (anything weapon_verb() calls slice/chop/stab/
     * pierce, not the blunt "bludgeon"/bare-handed "hit") lands a
     * cleaner, more consistent wound than a blunt one -- a small flat
     * bonus, reusing the verb classification already computed above
     * for messaging rather than adding a new weapon property. */
    if (weapon && strcmp(verb, "bludgeon") != 0)
        dmg += 1;

    if (dmg < 1)
        dmg = 1;

    /* Gamewide damage multiplier (user 2026-07-12's `balance` command) --
     * same class/race rule as the to-hit modifier above. */
    float dmg_mult = 1.0f;
    if (attacker->base.kind == THING_PC) {
        dmg_mult = class_balance_get(attacker->char_class)->dmg_mult
                 * race_balance_get(attacker->race)->dmg_mult;
    } else if (attacker->mob_class_known) {
        dmg_mult = class_balance_get(attacker->char_class)->dmg_mult;
    }
    dmg = (int)(dmg * dmg_mult);
    if (dmg < 1)
        dmg = 1;

    /* Affects system (user 2026-07-11's "buffs/debuffs/status" backlog
     * item) -- the Cleric spell "sanctuary" ("a strong aura that
     * reduces incoming damage") halves whatever a protected defender
     * takes, applied last so it discounts the fully-modified hit
     * rather than being folded into (and lost among) the earlier flat
     * modifiers above. */
    if (being_has_affect(defender, AFFECT_SANCTUARY)) {
        dmg /= 2;
        if (dmg < 1)
            dmg = 1;
    }

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
    char hit_capbuf[128];
    /* Damage numbers (user 2026-07-12: "dont report damage. messages
     * should read You stab a messenger from the goblins's left finger.")
     * -- a plain mortal never sees the raw number, but an immortal still
     * does (useful for balancing/testing), checked independently per
     * viewer same as the nospam toggle above. */
    if (being_is_immortal(attacker))
        tell(attacker, "You %s %s's %s for %d damage!\r\n", verb, being_display_name(defender), ln, dmg);
    else
        tell(attacker, "You %s %s's %s.\r\n", verb, being_display_name(defender), ln);
    if (being_is_immortal(defender))
        tell(defender, "%s %s your %s for %d damage!\r\n",
             being_display_name_cap(attacker, hit_capbuf, sizeof(hit_capbuf)), verb_3rd, ln, dmg);
    else
        tell(defender, "%s %s your %s.\r\n",
             being_display_name_cap(attacker, hit_capbuf, sizeof(hit_capbuf)), verb_3rd, ln);

    const char *status_before = limb_status_text(pct_before);
    const char *status_after = limb_status_text(pct_after);
    if (status_after && status_after != status_before) {
        char status_capbuf[128];
        tell(attacker, "%s's %s %s!\r\n",
             being_display_name_cap(defender, status_capbuf, sizeof(status_capbuf)), ln, status_after);
        tell(defender, "Your %s %s!\r\n", ln, status_after);

        /* Bleeding (user, 2026-07-11: "goes with limb damage and
         * bleeding" -- said in the context of adding pools/pee). A limb
         * crossing into a bad-enough tier (limb_status_text() returning
         * non-NULL, i.e. <20% HP) leaves a blood pool via the same
         * obj_grow_pool() ground-puddle infra `pee` uses (grows an
         * existing blood pool in the room instead of a separate object),
         * reusing this function's existing tier-crossing guard so it only
         * fires once per crossing, not on every hit while already in that
         * tier. */
        if (defender->base.roomp) {
            obj_grow_pool(defender->base.roomp, "blood", "puddle pool blood", "blood");
            char msg[128];
            snprintf(msg, sizeof(msg), "Blood pools around %s!\r\n", being_display_name(defender));
            descriptor_room_echo(defender->base.roomp, NULL, msg);
        }
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
        tell(winner, "You have slain %s!\r\n", being_display_name(loser));
        tell(loser, "You have been slain by %s!\r\nYou are DEAD!\r\n", being_display_name(winner));
    } else {
        tell(winner, "You have defeated %s!\r\n", being_display_name(loser));
        tell(loser, "You have been defeated by %s!\r\nYou are DEAD!\r\n", being_display_name(winner));
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
        if (levels_gained > 0) {
            /* Bug found 2026-07-12 (weapon-depth testing): progress_add_xp()
             * only bumps `level` -- it works on a bare progress_t, with no
             * access to attrs/kind, so it can't call being_calc_max_hp()
             * itself. Without this, a leveled-up character's max_hp (and
             * every limb's own max_hp, being_limbs_full_heal()) stayed
             * stuck at their level-1 values forever, leaving even a
             * high-level character just as fragile -- and just as prone to
             * a lucky decapitation -- as a brand new one. Recomputed here,
             * with the full being_t winner is already, and a full heal as
             * the level-up's reward (same spirit as the "You feel more
             * experienced!" message). */
            winner->progress.max_hp = being_calc_max_hp(winner);
            winner->progress.hp = winner->progress.max_hp;
            being_limbs_full_heal(winner);
            tell(winner, "You feel more experienced!\r\n");
        }
        player_progress_save(winner->player_id, &winner->progress);
    }

    /* Death goes to the log with the loser's IP (user requirement --
     * immortal-visible only, via the log command's gate). base.name would
     * be a mob's raw keyword list rather than a display string for
     * either side (2026-07-11 capitalization audit) -- utilitarian log
     * text, so the plain lowercase form is fine for both. */
    log_info("%s has been %s by %s. [%s]", being_display_name(loser),
             slain ? "slain" : "defeated", being_display_name(winner),
             loser->desc ? descriptor_display_host(loser->desc) : "?");

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
        /* loser is always a PC here (loser_is_pc gate above), so its name
         * is already properly cased -- but winner could be a mob, whose
         * base.name is a raw keyword list, not a display string (2026-07-11
         * capitalization audit). winner sits at a sentence-initial
         * position in some templates and mid-sentence in others; the
         * plain lowercase form reads fine (or only mildly informally
         * uncapitalized) in every template, which beats hand-tracking
         * per-template capitalization for a random flavor-text line. */
        int n = snprintf(taunt, sizeof(taunt), "\r\n<c>[INFO]<z> ");
        n += snprintf(taunt + n, sizeof(taunt) - (size_t)n, DEATH_TAUNTS[t],
                      loser->base.name, being_display_name(winner));
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
        /* loser->base.name is a mob's raw keyword list, not a display
         * string, if loser is a mob -- would otherwise leave a permanent,
         * visible-to-everyone "the corpse of lady stroll walk lies here"
         * (2026-07-11 capitalization audit). The name sits mid-sentence
         * in both strings ("the/The corpse of <name>"), so the lowercase
         * form is correct in both -- only the leading "The" is capitalized,
         * already a literal in the format string. */
        snprintf(short_descr, sizeof(short_descr), "the corpse of %s", being_display_name(loser));
        /* No trailing \r\n -- see the severed-limb long_descr comment above. */
        snprintf(long_descr, sizeof(long_descr), "The corpse of %s lies here.", being_display_name(loser));
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
        /* "death" triggers (user, 2026-07-11: "interaction with mobs objs
         * and room via scripts") fire here, before being_destroy() frees
         * `loser` -- actor is the killer (`winner`), room is wherever the
         * mob died. */
        if (loser->base.roomp) {
            trigger_t trigs[8];
            int n = trigger_repo_load_for("mob", loser->base.id, "death", trigs, 8);
            if (n > 0) {
                /* short_descr may start with a color tag (e.g. "<o>a dirty
                 * refuse hauler<1>") -- skip it before capitalizing, same
                 * bug class already fixed in cmd_look.c/cmd_object.c/
                 * cmd_scan.c/mob_ai.c/trigger.c's own cap_first() copies. */
                char capbuf[128];
                snprintf(capbuf, sizeof(capbuf), "%s", loser->base.short_descr);
                size_t ci = 0;
                while (capbuf[ci] == '<' && capbuf[ci + 1] != '\0' && capbuf[ci + 2] == '>')
                    ci += 3;
                if (capbuf[ci])
                    capbuf[ci] = (char)toupper((unsigned char)capbuf[ci]);
                for (int i = 0; i < n; i++)
                    trigger_run(&trigs[i], winner, loser->base.roomp, capbuf[0] ? capbuf : NULL);
            }
        }

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

    /* "N.name" ordinal prefix (user 2026-07-11: "mob 2.mob 3.mob etc
     * should attack the 1st 2nd and 3rd") -- when explicitly given,
     * skip the exact-match-priority rule below entirely and just count
     * matches in room order, since "2.clau" only makes sense as "the
     * second thing matching clau", not "prefer an exact name". Plain
     * "clau" (ordinal defaults to 1) keeps the exact-match behavior
     * fully unchanged for backward compatibility. */
    const char *rest;
    int ordinal = thing_parse_ordinal(name, &rest);
    size_t name_len = strlen(rest);

    if (ordinal > 1) {
        int seen = 0;
        for (thing_t *t = self->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t == &self->base)
                continue;
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            if (t->kind == THING_PC && !((being_t *)t)->desc)
                continue;
            if (thing_name_matches(t->name, rest, name_len)) {
                seen++;
                if (seen == ordinal)
                    return (being_t *)t;
            }
        }
        return NULL;
    }

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
        if (strcasecmp(t->name, rest) == 0)
            return (being_t *)t;
        if (!prefix_match && thing_name_matches(t->name, rest, name_len))
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
        /* target->base.name is a mob's raw keyword list, not a display
         * string, if target is a mob (2026-07-11 capitalization audit). */
        char debug_capbuf[128];
        tell(actor, "%s's %s %s!\r\n",
             being_display_name_cap(target, debug_capbuf, sizeof(debug_capbuf)), ln, status_after);
        tell(target, "Your %s %s!\r\n", ln, status_after);

        /* Same bleeding tier-crossing guard as combat_strike() -- this
         * debug path (hurtlimb) has its own duplicated tier-crossing
         * check, so the blood pool has to be duplicated here too or
         * `hurtlimb` couldn't deterministically test it. */
        if (target->base.roomp) {
            obj_grow_pool(target->base.roomp, "blood", "puddle pool blood", "blood");
            char msg[128];
            snprintf(msg, sizeof(msg), "Blood pools around %s!\r\n", being_display_name(target));
            descriptor_room_echo(target->base.roomp, NULL, msg);
        }
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
