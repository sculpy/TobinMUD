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
#include "body.h"
#include "descriptor.h"
#include "extraction.h"
#include "log.h"
#include "material.h"
#include "obj.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "practice.h"
#include "room.h"
#include "skill.h"
#include "thing.h"
#include "trigger.h"
#include "world.h"

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

/* A destroyed limb (0% HP) penalizes its owner's own offense (and, as of
 * this session, their defense too -- see the mirrored check in
 * combat_strike() below) -- flat, non-stacking (doesn't get worse with
 * more than one destroyed limb), placeholder amount. A destroyed limb can
 * be repaired mid-game at a Hospital (see being_has_destroyed_limb()'s doc
 * comment in being.h) -- it's not a permanent penalty. */
#define DESTROYED_LIMB_HIT_PENALTY 15

/* Positions polish (TODO backlog) -- see combat_strike(). Same magnitude as
 * DESTROYED_LIMB_HIT_PENALTY, no particular reason but consistency. */
#define NON_STANDING_HIT_BONUS 15

/* Mounted attack bonus (Mount / riding system, Sneezy → Tobin feature
 * audit) -- loosely mirrors the original's real mounted to-hit bonus
 * (roughly level/4+1, misc/combat.cc), simplified to one flat number
 * since Tobin has no Deikhan "mounted knight" class to scale it further
 * for. About half of NON_STANDING_HIT_BONUS's magnitude -- a real but
 * modest edge, not a dominant one. */
#define MOUNTED_ATTACK_BONUS 8

/* Object maintenance (Sneezy -> Tobin feature audit) -- corpses and
 * severed limbs used to sit in their room forever, since obj.h's new
 * decay_time field didn't exist until now. ~15/~20 real minutes at
 * obj_decay_tick()'s ~60s cadence -- a deliberate, documented value (the
 * original's exact real-world corpse lifespan isn't in this bundled
 * source), long enough to loot but not indefinite. A limb outlasts the
 * corpse it came from a little, being the rarer "proof of a decapitation"
 * trophy. */
#define CORPSE_DECAY_TICKS 15
#define LIMB_DECAY_TICKS 20
#define SCRAP_DECAY_TICKS 10 /* the least significant of the three -- scrap
                              * left from a destroyed item is pure flavor,
                              * not lootable gear, so it clears fastest. */

/* Combat structure damage (Object maintenance, Sneezy -> Tobin feature
 * audit) -- 30% base chance, matching the original's own documented
 * genericDamCheck() rate, that any given hit wears down whatever the
 * defender has equipped on the LIMB that got hit, if anything. */
#define EQUIP_DAMAGE_CHANCE_PCT 30

/* Per-limb hit likelihood (user 2026-07-12: "some limbs are harder to
 * decapitate... this should be based on the likelihood that a limb
 * could be damaged"), Sneezy's own real slot_chance[] table (body.c,
 * Body types 2026-07-26) -- a bigger target (the torso) is hit far more
 * often than a small one (a finger), so it also survives more cumulative
 * hits before its own HP share runs out. Now genuinely per-BODY-TYPE
 * (body_limb_weight()), not a single flat humanoid table -- a
 * BODY_SPIDER's own row weights its EX_* legs/feet for real and its
 * arms/wrists/hands at 0, the exact reverse of a human. */
static limb_t pick_weighted_limb(body_type_t bt) {
    int total = 0;
    for (int i = 0; i < LIMB_COUNT; i++)
        total += body_limb_weight(bt, (limb_t)i);
    if (total <= 0)
        return LIMB_BODY; /* defensive -- every real row has SOME weight */
    int roll = rand() % total;
    for (int i = 0; i < LIMB_COUNT; i++) {
        roll -= body_limb_weight(bt, (limb_t)i);
        if (roll < 0)
            return (limb_t)i;
    }
    return LIMB_BODY; /* unreachable -- weights always sum to `total` */
}

/* Major limbs (user 2026-07-12: "head neck waist body are all major
 * limbs"): destroying one is instant death, not just a survivable
 * dismemberment -- unlike an arm, leg, finger, or foot. */
static bool is_major_limb(limb_t limb) {
    return limb == LIMB_HEAD || limb == LIMB_NECK || limb == LIMB_WAIST || limb == LIMB_BODY;
}

/* Sneezy-inspired critical hit (Session 42, user: "copy sneezys crit hit
 * system, complete with object creation upon decapitation"). No separate
 * crit-chance roll -- this port triggers purely on a limb's HP actually
 * crossing to 0% from ongoing combat damage (see combat_strike()), reusing
 * the existing per-hit random-limb/damage system rather than adding a new
 * RNG layer. PCs only for now (a dying mob is destroyed outright already,
 * see combat_defeat()); scope confirmed with the user 2026-07-09. Drops a
 * lootable severed-part object in the room; any MAJOR limb (2026-07-12:
 * head/neck/waist/body, not just a decapitation specifically) is instant
 * death, reported back to the caller (combat_strike()'s return value) so
 * it can route through the existing combat_defeat() "slain" path rather
 * than duplicating it. */
static void combat_sever_limb(being_t *attacker, being_t *defender, limb_t limb) {
    if (!defender->base.roomp)
        return;

    const char *ln = body_limb_name_override((body_type_t)defender->body_type, limb);
    if (!ln)
        ln = limb_name(limb);
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
    if (part) {
        /* Object maintenance (Sneezy -> Tobin feature audit): same "used
         * to sit forever" gap as corpses -- see CORPSE_DECAY_TICKS'
         * comment above. */
        part->decay_time = LIMB_DECAY_TICKS;
        thing_move_to(&part->base, &defender->base.roomp->base);
    }

    tell(attacker, "%s's %s is severed clean off!\r\n",
         being_display_name_cap(defender, sever_capbuf, sizeof(sever_capbuf)), ln);
    tell(defender, "Your %s is severed clean off!\r\n", ln);

    /* "decapitating a neck should also remove the head" (user 2026-07-12)
     * -- the head has nothing left to hang onto once the neck is gone.
     * One level of recursion only (LIMB_HEAD never cascades further), so
     * this can't loop. Guarded on the head not already being gone, in
     * case some earlier hit had already destroyed it separately. */
    if (limb == LIMB_NECK && defender->limbs[LIMB_HEAD].hp > 0) {
        defender->limbs[LIMB_HEAD].hp = 0;
        tell(attacker, "The blow takes %s's head clean off along with it!\r\n",
             being_display_name(defender));
        tell(defender, "Your head comes off along with your neck!\r\n");
        combat_sever_limb(attacker, defender, LIMB_HEAD);
    }
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

/* Qualitative hit-intensity description (user 2026-07-12: "dont report
 * damage"; follow-up: "take out the damage number and use it to
 * describe how hard the hit was"). Ported from the real upstream's own
 * describe_dam()/REALNUM() (misc/combat.cc) rather than invented: same
 * 11-tier ladder and same wording, `dam` compared against `capacity`
 * (the struck limb's CURRENT, pre-hit HP -- not its max, so the exact
 * same raw damage number reads as more brutal against a limb that's
 * already nearly gone than a fresh one). Cross-multiplied against
 * thousandths rather than using floating point, same precision-safety
 * reasoning as material.c's tier multipliers. The real upstream also
 * branches on damage TYPE for the "at or past 100%" case (TYPE_SLASH/
 * TYPE_SLICE -> "into shreds", everything else -> "into a bloody
 * pulp") -- approximated here off weapon_verb()'s own "slice"/"chop"
 * cutting verbs vs. everything else, since Tobin has no separate damage-
 * type enum. */
const char *describe_dam(int dam, int capacity, const char *verb) {
    if (dam <= 0 || capacity <= 0)
        return "pathetically";
    long d = (long)dam * 1000;
    long cap = (long)capacity;
    if (d >= cap * 1000) {
        bool cutting = verb && (strcmp(verb, "slice") == 0 || strcmp(verb, "chop") == 0);
        return cutting ? "into shreds" : "into a bloody pulp";
    }
    if (d > cap * 640) return "beyond all recognition";
    if (d > cap * 320) return "incredibly well";
    if (d > cap * 160) return "very severely";
    if (d > cap * 80)  return "severely";
    if (d > cap * 60)  return "very hard";
    if (d > cap * 40)  return "hard";
    if (d > cap * 20)  return "lightly";
    if (d > cap * 10)  return "very lightly";
    if (d > cap * 5)   return "only slightly";
    return "pathetically";
}

/* Object maintenance (Sneezy -> Tobin feature audit) -- a Tobin-scale
 * slice of the original's dentItem()/tearItem()/pierceItem(): models the
 * DEFENDER'S WORN GEAR absorbing wear from being hit, not a weapon
 * degrading from being swung (matches the original's own framing -- those
 * three functions are called on the STRUCK being's equipment). No
 * material-susceptibility matrix (that's the separate, still-open
 * Material properties audit item) -- a flat 1-2 point structure loss per
 * triggered hit, same for every material, and no weapon-hardness scaling
 * either, an honest, documented scope-down rather than a silent gap.
 * Skips items with no real max_struct data at all (0 -- most sandbox/
 * test fixtures and some real content, same precedent as cmd_look.c's
 * condition display) and an immortal defender (dmg is already forced to
 * 0 by the caller, so there's nothing to wear down from). Only worn
 * `equipment[]` slots are checked, never `held[]` -- matches Magic
 * items' own equipment-vs-held distinction (stat/AC/HP/Vitality affects
 * only apply to worn gear too). At 0 structure the item is "scrapped":
 * whatever stat/AC/HP/Vitality bonus it was granting reverses first
 * (obj_apply_equip_affects(), same call `remove` makes), then it's
 * replaced with a small ephemeral "scraps of X" trash object on the
 * floor (the original's makeScraps() concept) that itself decays after a
 * while (Task 1's fresh decay-timer infra) rather than piling up
 * forever. */
static void combat_maybe_damage_equipment(being_t *defender, limb_t limb, int dmg) {
    if (dmg <= 0)
        return;
    obj_t *item = defender->equipment[limb];
    if (!item || item->max_struct <= 0)
        return;
    if (rand() % 100 >= EQUIP_DAMAGE_CHANCE_PCT)
        return;

    item->cur_struct -= 1 + rand() % 2;
    if (item->cur_struct > 0)
        return;

    const char *label = item->base.short_descr[0] ? item->base.short_descr : item->base.name;
    tell(defender, "Your %s is destroyed!\r\n", label);
    if (defender->base.roomp) {
        char capbuf[128];
        char room_msg[224];
        snprintf(room_msg, sizeof(room_msg), "%s's %s is destroyed!\r\n",
                 being_display_name_cap(defender, capbuf, sizeof(capbuf)), label);
        descriptor_room_echo(defender->base.roomp, defender, room_msg);
    }

    defender->equipment[limb] = NULL;
    obj_apply_equip_affects(defender, item, -1);
    if (defender->base.kind == THING_PC)
        player_inventory_save(defender->player_id, defender);

    if (defender->base.roomp) {
        char scrap_short[192];
        char scrap_long[256];
        snprintf(scrap_short, sizeof(scrap_short), "scraps of %s", label);
        snprintf(scrap_long, sizeof(scrap_long), "Scraps of %s lie here, ruined.", label);
        obj_t *scrap = obj_create_ephemeral("scraps", scrap_short, scrap_long, OBJ_CAT_TRASH);
        if (scrap) {
            scrap->decay_time = SCRAP_DECAY_TICKS;
            thing_move_to(&scrap->base, &defender->base.roomp->base);
        }
    }
    obj_destroy(item);
}

/* Placeholder damage formula, built from the existing 6-attribute set --
 * not the original's weapon/class/skill-driven system (none of that
 * exists yet). Hit chance skews on relative DEX (and is penalized if the
 * attacker has a destroyed limb); damage scales with STR above ATTR_BASE
 * plus a small random component. Each hit lands on a limb chosen by
 * pick_weighted_limb() (Sneezy's own slotChance() proportions, user
 * 2026-07-12) -- a bigger target (the torso) gets hit far more often
 * than a small one (a finger). Damage is applied to both that limb's HP
 * and the defender's overall HP via being_hurt_limb(). Crossing into a
 * worse limb_status_text() tier (see being.h) announces it to both
 * sides. Returns true iff this hit just destroyed a MAJOR limb (head,
 * neck, waist, or body, see is_major_limb() -- not just a decapitation
 * specifically) -- the caller must route that straight to
 * combat_defeat(). */
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
    /* Meaningful limb damage (TODO.md, user: "make individual limb hits
     * actually hurt"): the mirror image of the attacker-side penalty just
     * above -- a destroyed limb doesn't just throw off YOUR swing, it
     * also makes you an easier target (same flat, non-stacking amount, no
     * particular reason but consistency with the offense-side number). */
    if (being_has_destroyed_limb(defender))
        modifier += DESTROYED_LIMB_HIT_PENALTY;
    /* Positions polish (TODO backlog): a defender who isn't standing --
     * sitting, resting, sleeping, or any of the lower reserved-for-future
     * rungs (see position_t, being.h) -- is an easier target, mirroring
     * the original. Attacking never auto-stands the DEFENDER (only the
     * attacker, cmd_attack.c), so this stays in effect for as long as they
     * choose to stay down. */
    /* POSITION_MOUNTED excluded -- being on horseback isn't the same
     * "harder to defend yourself" situation as sitting/resting/sleeping
     * (Mount / riding system, Sneezy → Tobin feature audit). */
    if (defender->position != POSITION_STANDING && defender->position != POSITION_MOUNTED)
        modifier += NON_STANDING_HIT_BONUS;
    /* Mounted ATTACKER bonus -- the height/mobility edge of fighting from
     * horseback (Mount / riding system, Sneezy → Tobin feature audit). */
    if (attacker->position == POSITION_MOUNTED)
        modifier += MOUNTED_ATTACK_BONUS;
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

    /* Passive parry (Sneezy → Tobin feature audit, "Skill-based
     * combat"). Checked Sneezy's own disc/disc_warrior_dueling.cc
     * first: real `parry` is NOT a player command at all -- the
     * original's own parser stub, `doParry()`, just prints "Parry is
     * not yet supported in this fashion." The actual mechanic
     * (`parryWarrior()`) is entirely passive, checked once per
     * incoming melee hit with no cooldown (LAG_0, never routed through
     * the lag system) -- ported faithfully as a passive check here
     * too, deliberately with no `cmd_parry.c`. Sneezy's real chance is
     * a flat ~4% base gate before a separate learnedness roll;
     * simplified to one proficiency-scaled roll, quartered so even
     * 100% proficiency caps near 25% rather than making a maxed
     * Warrior unhittable. Checked BEFORE the normal hit/miss roll,
     * same "defensive reaction resolves first" ordering Sneezy uses --
     * a parry negates the attack outright regardless of what the to-
     * hit roll below would have been. */
    if (!being_is_immortal(defender) && being_knows_skill(defender, "parry")) {
        const skill_def_t *parry_sk = skill_find(defender->char_class, "parry", false);
        if (parry_sk && skill_roll_success(skill_learn_from_doing(defender, parry_sk) / 4)) {
            if (!(attacker->pflags & PLR_NOSPAM))
                tell(attacker, "%s parries your attack!\r\n", being_display_name(defender));
            if (!(defender->pflags & PLR_NOSPAM))
                tell(defender, "You parry %s's attack!\r\n", being_display_name(attacker));
            return false;
        }
    }

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
    if (attacker->off_hand_next) {
        bool dual_wield_known = being_knows_skill(attacker, "dual wield");
        dmg += dual_wield_known ? 0 : -1;
        /* Per-skill proficiency (Sneezy-style learn-by-doing, user
         * 2026-07-17): dual wield is a passive stance with no discrete
         * success/failure, so only the proficiency number climbs with
         * use -- the mitigation above stays the existing binary
         * know-it-or-don't gate, unaffected. PCs only (mobs have no
         * player_id/practice-points system to hang this on, same
         * THING_PC gate combat_defeat() uses for XP/practice points). */
        if (dual_wield_known && attacker->base.kind == THING_PC && !being_is_immortal(attacker)) {
            const skill_def_t *sk = skill_find(attacker->char_class, "dual wield", false);
            if (sk)
                skill_learn_from_doing(attacker, sk);
        }
    } else {
        dmg += 1;
    }
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
    /* Material property system (Sneezy → Tobin feature audit): a
     * higher-tier weapon material hits harder, folded into the same
     * gamewide multiplier rather than a second separate scaling pass. A
     * bare-handed attacker (weapon == NULL) gets no bonus, same shape as
     * weapon_hitroll/weapon_damroll above. */
    if (weapon)
        dmg_mult *= (float)material_tier_damage_mult(material_tier_for_id(weapon->material));
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

    /* Immortal damage immunity (user 2026-07-12: "an immortal character
     * shouldnt be damaged by hits in a fight, see engage code from
     * sneezy"). Real Sneezy's own rule (setCharFighting()/
     * setVictFighting(), misc/combat.cc) actually refuses a PC from ever
     * INITIATING an attack on an immortal PC in the first place -- not
     * ported as-is, since it would break the existing `hit` command's
     * whole purpose (letting an immortal spar in real combat for testing,
     * task 11/13's own smoke tests rely on it). Landing a hit on an
     * immortal is still allowed, verb/messaging and all -- it just always
     * deals zero damage, applied last so nothing above can un-zero it. */
    if (being_is_immortal(defender))
        dmg = 0;

    limb_t limb = pick_weighted_limb((body_type_t)defender->body_type);
    int pct_before = being_limb_pct(defender, limb);
    int limb_hp_before = defender->limbs[limb].hp; /* pre-hit capacity, for describe_dam() below */
    being_hurt_limb(defender, limb, dmg);
    int pct_after = being_limb_pct(defender, limb);
    combat_maybe_damage_equipment(defender, limb, dmg);

    const char *ln = body_limb_name_override((body_type_t)defender->body_type, limb);
    if (!ln)
        ln = limb_name(limb);
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
     * -- originally a plain mortal never saw the raw number while an
     * immortal still did (for balancing/testing), but the immortal
     * branch never actually got the qualitative treatment the mortal
     * one did (user, follow-up: "take out the damage number and use it
     * to describe how hard the hit was"). Both now go through
     * describe_dam() -- ported from the real upstream's own
     * describe_dam()/normalHitMessage() (misc/combat.cc): the SAME hit
     * reads as more brutal against a limb that's already nearly gone
     * (ratio is against the limb's CURRENT pre-hit HP, not its max) --
     * escalating flavor as a fight wears a limb down, not a flat
     * word-per-damage-number mapping. */
    const char *intensity = describe_dam(dmg, limb_hp_before, verb);
    tell(attacker, "You %s %s's %s %s!\r\n", verb, being_display_name(defender), ln, intensity);
    tell(defender, "%s %s your %s %s!\r\n",
         being_display_name_cap(attacker, hit_capbuf, sizeof(hit_capbuf)), verb_3rd, ln, intensity);

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

    bool instadeath = false;
    if (pct_before > 0 && pct_after == 0 && defender->base.kind == THING_PC) {
        combat_sever_limb(attacker, defender, limb);
        if (is_major_limb(limb))
            instadeath = true;
    }
    return instadeath;
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

/* Group-aware reward split (Sneezy → Tobin feature audit, "Group / party
 * system"): if `winner` is grouped, a kill's XP/gold is shared among
 * every grouped, non-immortal PC member physically present in `room`
 * (same spatial rule the original uses -- only in-room members share a
 * kill). A solo (non-grouped) winner is unaffected: `out[0] = winner`,
 * returns 1, so every call site's pre-group-system single-recipient
 * behavior is preserved exactly unless a real group is active. */
static int group_recipients(being_t *winner, room_t *room, being_t **out, int max) {
    if (winner->grouped) {
        being_t *members[GROUP_MAX_FOLLOWERS + 1];
        int total = being_group_members(winner, members, GROUP_MAX_FOLLOWERS + 1);
        int n = 0;
        for (int i = 0; i < total && n < max; i++) {
            being_t *m = members[i];
            if (m->base.kind == THING_PC && !being_is_immortal(m) && m->base.roomp == room)
                out[n++] = m;
        }
        if (n > 0)
            return n;
    }
    out[0] = winner;
    return 1;
}
static void combat_defeat(being_t *loser, being_t *winner, bool slain) {
    /* A mob loser that's actually a connected player's body -- possessed
     * (cmd_possess.c) or polymorphed (AFFECT_POLYMORPH, being.c) -- needs
     * its descriptor reverted to the REAL underlying character, RIGHT
     * HERE, RETURNING IMMEDIATELY rather than falling through to the
     * normal PC-death pipeline below (menu-kick via descriptor_leave_
     * to_menu(), which itself being_destroy()s the character and resets
     * the descriptor's connection state). Originally this DID fall
     * through, matching Sneezy's own real behavior ("the player survives
     * in their own body" but still takes normal death consequences) --
     * but a real, reproducible crash was traced to that exact
     * combination (revert-then-immediately-run-the-full-death-pipeline,
     * from WITHIN combat_process_run()'s own g_descriptors walk, which
     * doesn't pre-capture a `next` pointer the way safer iterations
     * elsewhere in this codebase do) that root-causing didn't fully
     * resolve in the time available; see STATUS.md for the full
     * writeup. This is the deliberately conservative fallback: heal the
     * player back up and return them to their body with NO further
     * death consequences (no XP loss, no corpse, no menu-kick) --
     * disclosed as a real simplification, not Sneezy's exact behavior,
     * but one that avoids the crash outright rather than risk it in
     * production. `possess` (an immortal's puppet dying) hits this same
     * safe path. */
    if (loser->base.kind == THING_MOB && loser->desc && loser->desc->possess_original) {
        descriptor_t *pd = loser->desc;
        being_t *original = pd->possess_original;
        loser->desc = NULL;
        pd->character = original;
        original->desc = pd;
        pd->possess_original = NULL;
        winner->fighting = NULL;
        original->fighting = NULL;
        original->progress.hp = original->progress.max_hp / 2;
        if (original->progress.hp < 1)
            original->progress.hp = 1;
        being_limbs_full_heal(original);
        descriptor_send(pd, "Your body is destroyed -- you snap back into your own, badly shaken!\r\n");
        being_destroy(loser); /* the temporary/puppeted mob body; safe now that no descriptor points at it */
        return;
    }

    loser->fighting = NULL;
    winner->fighting = NULL;

    bool loser_is_pc = (loser->base.kind == THING_PC);

    if (loser_is_pc) {
        loser->progress.hp = loser->progress.max_hp / 2;
        if (loser->progress.hp < 1)
            loser->progress.hp = 1;
        being_limbs_full_heal(loser);

        /* XP loss on death (Sneezy → Tobin feature audit, "Death
         * processing (XP loss, resurrection)"). User, AskUserQuestion
         * 2026-07-19: XP loss only -- Tobin's PC "death" was already NOT
         * permadeath (see this function's own doc comment above), so
         * there's no corpse to build a resurrection spell around;
         * "resurrection" is already covered by the existing soft-respawn/
         * relog flow. Adapted from Sneezy's own min(20% of current XP,
         * level-scaled cap) formula (docs/systems/important/
         * death-processing.md) -- the cap here is simpler and needs no
         * separate mob-XP curve: never lose more than the XP banked PAST
         * the current level's own threshold (progress_xp_for_level()), so
         * a death can never de-level anyone, only eat into progress
         * toward the next one. PvP (a PC winner) divides the result by
         * 10, same reduction Sneezy applies -- PK combat already requires
         * mutual `toggle pk` opt-in, so this is a consensual penalty, not
         * a punitive one; a MOB winner (the ordinary "died to a monster"
         * case) gets the full penalty. Immortals never lose XP (they're
         * already past the mortal ladder, same "immortals don't need XP"
         * precedent as the winner-XP block below). */
        if (!being_is_immortal(loser) && loser->progress.experience > 0) {
            long base_loss = loser->progress.experience / 5;
            long level_floor = progress_xp_for_level(loser->progress.level);
            long max_loss = loser->progress.experience - level_floor;
            if (max_loss < 0)
                max_loss = 0;
            long xp_loss = base_loss < max_loss ? base_loss : max_loss;
            if (winner->base.kind == THING_PC)
                xp_loss /= 10;
            if (xp_loss > 0) {
                loser->progress.experience -= xp_loss;
                tell(loser, "You lose %ld experience point%s.\r\n", xp_loss, xp_loss == 1 ? "" : "s");
            }
        }

        /* Split gold on kill (TODO.md, user: "also upon death get all
         * gold from the victim and split it between all group members if
         * groupped"). SOLO case only -- no group/party system exists yet
         * to split across, so the winner simply takes everything; the
         * "if grouped" split remains a separate, still-blocked item.
         * PC-vs-PC only (a mob loser's gold-drop-to-killer is the
         * separate, already-existing path below); same non-immortal-
         * winner gate as that path. PK combat itself already requires
         * both sides to have opted in (`toggle pk`), so this can only
         * ever fire with mutual consent -- there's no non-consensual
         * gold-loss path here. Winner's `progress.gold` change is picked
         * up by the XP block's own player_progress_save() below (runs
         * after this, on the same struct) rather than saving twice. */
        if (winner->base.kind == THING_PC && !being_is_immortal(winner)
            && loser->progress.gold > 0) {
            int stolen = loser->progress.gold;
            loser->progress.gold = 0;
            being_t *recipients[GROUP_MAX_FOLLOWERS + 1];
            int n = group_recipients(winner, winner->base.roomp, recipients, GROUP_MAX_FOLLOWERS + 1);
            int share = stolen / n;
            for (int i = 0; i < n; i++) {
                recipients[i]->progress.gold += share;
                tell(recipients[i], "You loot %d gold from %s's body.\r\n",
                     share, being_display_name(loser));
            }
            tell(loser, "%s loots %d gold from your body.\r\n",
                 being_display_name(winner), stolen);
        }

        player_progress_save(loser->player_id, &loser->progress);
    }

    if (slain) {
        tell(winner, "You have slain %s!\r\n", being_display_name(loser));
        tell(loser, "You have been slain by %s!\r\nYou are <r>DEAD<z>!\r\n", being_display_name(winner));
    } else {
        tell(winner, "You have defeated %s!\r\n", being_display_name(loser));
        tell(loser, "You have been defeated by %s!\r\nYou are <r>DEAD<z>!\r\n", being_display_name(winner));
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

        /* Group split (see group_recipients() above): level-weighted, a
         * simplification of the original's mob_exp()-based share -- a
         * solo winner gets recipients={winner}, weight math collapses to
         * the exact same xp_gain as before the group system existed. */
        being_t *recipients[GROUP_MAX_FOLLOWERS + 1];
        int n = group_recipients(winner, winner->base.roomp, recipients, GROUP_MAX_FOLLOWERS + 1);
        long total_weight = 0;
        for (int i = 0; i < n; i++)
            total_weight += recipients[i]->progress.level > 0 ? recipients[i]->progress.level : 1;

        for (int i = 0; i < n; i++) {
            being_t *m = recipients[i];
            long weight = m->progress.level > 0 ? m->progress.level : 1;
            long share = (n == 1) ? xp_gain : (xp_gain * weight) / total_weight;
            if (share < 1)
                share = 1;
            int levels_gained = progress_add_xp(&m->progress, share);
            tell(m, "You gain %ld experience.\r\n", share);
            if (levels_gained > 0) {
                /* Bug found 2026-07-12 (weapon-depth testing): progress_add_xp()
                 * only bumps `level` -- it works on a bare progress_t, with no
                 * access to attrs/kind, so it can't call being_calc_max_hp()
                 * itself. Without this, a leveled-up character's max_hp (and
                 * every limb's own max_hp, being_limbs_full_heal()) stayed
                 * stuck at their level-1 values forever, leaving even a
                 * high-level character just as fragile -- and just as prone to
                 * a lucky decapitation -- as a brand new one. Recomputed here,
                 * with the full being_t m already is, and a full heal as the
                 * level-up's reward (same spirit as the "You feel more
                 * experienced!" message). Applies per-recipient now, not just
                 * the winner, so a leveling-up group member gets the same
                 * treatment the solo winner always did. */
                m->progress.max_hp = being_calc_max_hp(m);
                m->progress.hp = m->progress.max_hp;
                being_limbs_full_heal(m);
                tell(m, "You feel more experienced!\r\n");
                int pp = 0;
                for (int j = 0; j < levels_gained; j++)
                    pp += practice_points_for_level(m);
                m->progress.practice_points += pp;
                tell(m, "<g>You gain %d practice point%s.<z>\r\n",
                     pp, pp == 1 ? "" : "s");
            }
            player_progress_save(m->player_id, &m->progress);
        }
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
            "%s has perished...AGAIN! Way to go %s!",
            "%s is no more! %s made sure of it!",
            "%s is pushing up the daisies, thanks to %s!",
            "%s's corpse is now unidentifiable! %s, what is this?!?",
            "%s doesn't want to go on the cart! %s says, 'Quit being a baby!'",
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
            corpse->val[2] = loser->mob_race; /* source race, `cook`'s TYPE_CORPSE (0/NORACE for a PC) */
            corpse->val[3] = 0;      /* Crafting & extraction: CORPSE_SKINNED(1)/BUTCHERED(2)
                                        bitmask, see cmd_skin.c/cmd_butcher.c */
            corpse->raw_type = loser_is_pc ? CORPSE_KIND_PC : CORPSE_KIND_MOB;
            corpse->weight = 50;     /* placeholder body weight */
            /* Object maintenance (Sneezy -> Tobin feature audit): corpses
             * used to sit in their room forever -- no decay mechanism
             * existed at all. CORPSE_DECAY_TICKS gives a real window to
             * loot (~15 real minutes at obj_decay_tick()'s ~60s cadence)
             * before it crumbles; a still-populated corpse relocates its
             * remaining contents to the room floor first (decay_visit(),
             * obj.c), so unlooted gear is never actually lost, just no
             * longer neatly bagged. */
            corpse->decay_time = CORPSE_DECAY_TICKS;
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

        /* Autoloot (user 2026-07-12: "an autoloot toggle where a player
         * upon opponent death automatically loots all from the corpse")
         * -- winner's own PLR_AUTOLOOT toggle, checked here right after
         * the corpse is populated so it works for both a normal defeat
         * and a decapitation alike. PC winner only (a mob has no
         * inventory to receive into); no-op if the corpse ended up
         * empty (e.g. a mob loser, who carries nothing yet). */
        if (corpse && winner->base.kind == THING_PC && (winner->pflags & PLR_AUTOLOOT)) {
            bool looted_any = false;
            thing_t *ct = corpse->base.stuff_head;
            while (ct) {
                thing_t *cnext = ct->stuff_next;
                thing_move_to(ct, &winner->base);
                looted_any = true;
                ct = cnext;
            }
            if (looted_any) {
                tell(winner, "You automatically loot %s's corpse.\r\n", being_display_name(loser));
                player_inventory_save(winner->player_id, winner);
            }
        }
    }

    /* Gold drop (Money system, user 2026-07-17: "implement money and
     * shops" -- TODO's own "Future: mobs drop them (economy)" note). A
     * mob loser hands its killer a scaled amount of gold directly -- gold
     * is a wallet stat (progress_t.gold), not a pickupable/lootable
     * object, so there's no coin object to place in the corpse. Same PC-
     * winner-only, non-immortal gate as the XP award above; PCs never
     * drop gold on death (no real PK economy to protect yet, see the
     * still-open "PK opt-in flag" TODO entry). A mundane animal-race mob
     * (RODENT, FELINE, BEAR, DEER, BIRD, ...) never drops gold at all --
     * user 2026-07-19: "animal races should not have wealth, that
     * doesnt make sense" -- see mob_race_is_animal() (being.c). XP is
     * unaffected; only the wallet-stat drop is gated. */
    if (!loser_is_pc && winner->base.kind == THING_PC && !being_is_immortal(winner)
        && !mob_race_is_animal(loser->mob_race)) {
        int mob_level = loser->progress.level > 0 ? loser->progress.level : 1;
        int gold_gain = mob_level * (1 + rand() % 5);
        being_t *recipients[GROUP_MAX_FOLLOWERS + 1];
        int n = group_recipients(winner, winner->base.roomp, recipients, GROUP_MAX_FOLLOWERS + 1);
        int share = gold_gain / n;
        for (int i = 0; i < n; i++) {
            recipients[i]->progress.gold += share;
            tell(recipients[i], "You find %d gold.\r\n", share);
            player_progress_save(recipients[i]->player_id, &recipients[i]->progress);
        }
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

bool combat_apply_skill_damage(being_t *attacker, being_t *defender, int dmg, limb_t limb) {
    if (being_is_immortal(defender))
        dmg = 0;
    being_hurt_limb(defender, limb, dmg);
    if (defender->progress.hp <= 0) {
        combat_defeat(defender, attacker, false);
        return true;
    }
    return false;
}

/* Drowning (Sneezy → Tobin feature audit, "Water, drowning, flight").
 * Checked Sneezy's own movement-terrain-navigation doc first: the
 * original's procCharDrowning scheduler deals 1d10 to a PC underwater
 * without AFF_WATERBREATH every 3.6 real seconds, via reconcileDamage()
 * -- genuinely lethal. Vitals_tick_run() (vitals.c) calls this once its
 * own drowning check lands a killing blow; same 1d10 roll, just on
 * Tobin's own slower ~60s vitals cadence instead of the original's
 * 3.6s one, so it's already far gentler in practice without changing
 * the roll itself. User (AskUserQuestion): drowning should be able to
 * genuinely kill, unlike hunger/thirst/poison's non-lethal floor-at-1
 * convention -- there is no `winner` here (an environmental death, not
 * a kill), so this can't reuse combat_defeat() as-is; duplicates just
 * the PC-relevant slice of it instead (half-heal reset, limb heal, XP
 * loss at the FULL rate since there's no PvP consent to halve it for,
 * a corpse with the victim's belongings, eject to the account menu) --
 * no gold transfer, since there's no killer to receive it. */
void combat_drown_pc(being_t *victim) {
    if (!victim || victim->base.kind != THING_PC)
        return;

    victim->fighting = NULL;
    victim->progress.hp = victim->progress.max_hp / 2;
    if (victim->progress.hp < 1)
        victim->progress.hp = 1;
    being_limbs_full_heal(victim);

    /* Same XP-loss formula as combat_defeat()'s PC branch, full rate
     * (no /10 PvP reduction -- there's no consenting opponent here). */
    if (!being_is_immortal(victim) && victim->progress.experience > 0) {
        long base_loss = victim->progress.experience / 5;
        long level_floor = progress_xp_for_level(victim->progress.level);
        long max_loss = victim->progress.experience - level_floor;
        if (max_loss < 0)
            max_loss = 0;
        long xp_loss = base_loss < max_loss ? base_loss : max_loss;
        if (xp_loss > 0) {
            victim->progress.experience -= xp_loss;
            tell(victim, "You lose %ld experience point%s.\r\n", xp_loss, xp_loss == 1 ? "" : "s");
        }
    }

    tell(victim, "Your lungs fill with water -- everything goes dark. You have DROWNED!\r\n");

    room_t *scene = victim->base.roomp;
    if (scene) {
        char namebuf[64], msg[128];
        being_display_name_cap(victim, namebuf, sizeof(namebuf));
        snprintf(msg, sizeof(msg), "%s thrashes weakly, then goes still and sinks.\r\n", namebuf);
        descriptor_room_echo(scene, victim, msg);

        /* Corpse + belongings (same shape as combat_defeat()'s PC branch
         * -- see its own comment for why it's ephemeral/unlocked). */
        char short_descr[128], long_descr[200];
        snprintf(short_descr, sizeof(short_descr), "the corpse of %s", being_display_name(victim));
        snprintf(long_descr, sizeof(long_descr), "The bloated corpse of %s floats here.", being_display_name(victim));
        obj_t *corpse = obj_create_ephemeral("corpse", short_descr, long_descr, OBJ_CAT_CONTAINER);
        if (corpse) {
            corpse->wear_flag = 0;
            corpse->val[0] = 0;
            corpse->val[1] = 0;
            corpse->val[2] = victim->mob_race; /* source race, `cook`'s TYPE_CORPSE */
            corpse->weight = 50;
            thing_move_to(&corpse->base, &scene->base);
        }
        thing_t *t = victim->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next;
            if (t->kind == THING_OBJ)
                thing_move_to(t, corpse ? &corpse->base : &scene->base);
            t = next;
        }
        for (int i = 0; i < LIMB_COUNT; i++)
            victim->equipment[i] = NULL;
        for (int i = 0; i < 2; i++)
            victim->held[i] = NULL;
        player_inventory_save(victim->player_id, victim);
    }

    log_info("%s has drowned. [%s]", being_display_name(victim),
             victim->desc ? descriptor_display_host(victim->desc) : "?");

    player_progress_save(victim->player_id, &victim->progress);
    if (victim->desc)
        descriptor_leave_to_menu(victim->desc);
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
        if (a->progress.hp <= 0 || a_decapitated) {
            combat_defeat(a, b, a_decapitated);
            continue;
        }

        /* Mid-fight persistence (TODO.md): HP was previously only saved at
         * defeat/quit (see descriptor_leave_to_menu()), so a disconnect
         * (crash, or a losing player quietly pulling the plug) mid-fight
         * reloaded at whatever HP was last saved before the fight even
         * started -- a real crash-loss risk, and a soft exploit (disconnect
         * to undo damage taken). Save both PCs' current HP after every
         * round the fight is still ongoing; cheap (local DB, one round per
         * ~1.2s per active fight) and reuses the same player_progress_save()
         * the defeat/gold-drop paths above already call, not a new
         * mechanism. Limb HP is NOT included -- it isn't persisted at all
         * yet, by ANY path, defeat included (see player_repo.c); that's the
         * separate, still-open "Meaningful limb damage" TODO item. */
        if (a->base.kind == THING_PC)
            player_progress_save(a->player_id, &a->progress);
        if (b->base.kind == THING_PC)
            player_progress_save(b->player_id, &b->progress);
    }

    /* Pet/charm (Sneezy → Tobin feature audit): a charmed pet lands its
     * own strike against whatever it's currently fighting, right here,
     * right on COMBAT_ROUND_PULSES -- NOT mob_ai_tick's ~60s wander/
     * scavenge cadence (a first version set pet->fighting from mob_ai.c
     * and found, live, that a pet could go up to a full minute without
     * ever engaging, since combat resolves roughly every 1.2s -- moving
     * the join here as well as the strike fixes that mismatch outright).
     * Mobs have no descriptor, so the main PC-driven loop above never
     * resolves a pet's SIDE of a fight either way.
     *
     * pet->fighting gets set two ways: auto-assist (below, if the pet has
     * no target of its own and its master is fighting someone) or an
     * explicit spoken order (cmd_say.c's try_pet_command(), "say attack
     * <target>", user 2026-07-25) -- once set, EITHER way, the pet keeps
     * fighting that specific target regardless of what the master does
     * next (they might defeat their own opponent, or start a different
     * fight, while the pet is still busy with whoever it was told to
     * attack) until it dies, leaves, or the master says "stop"/"stay"/
     * "guard". Deliberately one-sided: the target's own retaliation still
     * goes entirely to whoever it's ACTUALLY paired with in the main loop
     * above (target->fighting is never touched here), so a pet adds bonus
     * damage without ever drawing aggro onto itself -- a disclosed
     * simplification of Sneezy's real 3-way combat, not an oversight;
     * genuine multi-attacker retaliation is a bigger, separate lift. A
     * kill the pet lands is credited to the MASTER (combat_defeat(target,
     * master, ...)), not the pet -- a mob "winner" would make no sense
     * for XP/gold/kill messages. Safe against a target already destroyed
     * earlier in this same tick (by the main loop above, or by a second
     * pet's own strike below): being_destroy() itself clears any charmed
     * pet's dangling ->fighting pointer to whatever it just freed (see
     * its own comment, being.c), so `pet->fighting` is already NULL by
     * the time this runs if that happened -- no stale dereference. */
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *master = d->character;
        if (!master)
            continue;
        being_t *pet = being_find_charmed_pet(master);
        if (!pet)
            continue;

        if (pet->position != POSITION_STANDING || !pet->base.roomp
            || pet->base.roomp != master->base.roomp) {
            pet->fighting = NULL; /* the pet can't reach its master at all -- stand down */
            continue;
        }
        if (!pet->fighting && master->fighting)
            pet->fighting = master->fighting; /* auto-assist: joins the master's own target */

        being_t *target = pet->fighting;
        if (!target)
            continue;
        if (target->progress.hp <= 0 || !target->base.roomp || target->base.roomp != pet->base.roomp) {
            pet->fighting = NULL; /* target already down, or no longer reachable */
            continue;
        }

        /* combat_strike() itself only messages the two combatants directly
         * (tell(), both no-ops here -- a mob has no descriptor either
         * side) -- the master gets their own explicit line so a pet's
         * contribution is actually visible, not silent extra damage. */
        int hp_before = target->progress.hp;
        bool decapitated = combat_strike(pet, target);
        if (target->progress.hp < hp_before) {
            tell(master, "%s strikes %s!\r\n", being_display_name(pet), being_display_name(target));
        } else {
            tell(master, "%s misses %s!\r\n", being_display_name(pet), being_display_name(target));
        }
        if (target->progress.hp <= 0 || decapitated) {
            pet->fighting = NULL;
            combat_defeat(target, master, decapitated);
        }
    }
}

/* PK opt-in gate (TODO.md: "player flag; BOTH players must have opted
 * in for attack/kill between players"). Mob targets are always fine;
 * an immortal on EITHER side bypasses this entirely -- immortal-vs-
 * mortal combat (instakill) and immortal-vs-immortal (guarded
 * separately, see cmd_kill.c) are governed by their own existing rules,
 * unaffected by PLR_PK_OPTIN. Only mortal-vs-mortal PC combat needs
 * both sides to have `toggle pk` on. */
bool combat_pk_allowed(const being_t *self, const being_t *t) {
    if (t->base.kind != THING_PC)
        return true;
    if (being_is_immortal(self) || being_is_immortal(t))
        return true;
    return (self->pflags & PLR_PK_OPTIN) && (t->pflags & PLR_PK_OPTIN);
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
            if (!combat_pk_allowed(self, (being_t *)t))
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
        if (!combat_pk_allowed(self, (being_t *)t))
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

    const char *ln = body_limb_name_override((body_type_t)target->body_type, limb);
    if (!ln)
        ln = limb_name(limb);
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

    bool instadeath = false;
    if (pct_before > 0 && pct_after == 0 && target->base.kind == THING_PC) {
        combat_sever_limb(actor, target, limb);
        if (is_major_limb(limb))
            instadeath = true;
    }
    if (instadeath)
        combat_defeat(target, actor, true);
    return instadeath;
}
