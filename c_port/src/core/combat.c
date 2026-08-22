/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
#include "pulse.h"
#include "room.h"
#include "skill.h"
#include "spellcast.h"
#include "thing.h"
#include "trigger.h"
#include "trophy.h"
#include "trophy_repo.h"
#include "world.h"

/* Forward decl -- combat_strike() (below) calls this on every landed hit,
 * but it's defined further down (needs group_recipients()), and
 * practice.h's practice_points_for_level()/being_calc_max_hp() etc. */
static void combat_award_hit_xp(being_t *attacker, being_t *victim, int dmg);

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
    descriptor_notify(b->desc, buf); /* dropped, not held, if the recipient is editing -- combat is ambient, not communication */
}

/* A destroyed limb (0% HP) penalizes its owner's own offense (and, as of
 * this session, their defense too -- see the mirrored check in
 * combat_strike() below) -- flat, non-stacking (doesn't get worse with
 * more than one destroyed limb). No upstream SneezyMUD precedent for this
 * mechanic exists at all -- it's a Tobin-original addition -- so the
 * flat-vs-scaling question was a pure design call, not a bug: user decided
 * 2026-07-26 to keep it flat (see TODO.md), not a placeholder anymore. A
 * destroyed limb can be repaired mid-game at a Hospital (see
 * being_has_destroyed_limb()'s doc comment in being.h) -- it's not a
 * permanent penalty. */
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

/* Weapon-category-flavored severing phrase (TODO.md, user 2026-08-04:
 * "port the original SneezyMUD critical-hit combat messages"). Tobin's
 * own crit mechanic (below) stays the scoped-down "limb HP crosses to
 * 0%" trigger, not upstream's separate crit-chance roll/broken-bone
 * system (crit_combat.cc's critBlunt()/critSlash()/critPierce(), ~2500
 * lines of weapon-type-specific outcomes) -- porting that whole
 * mechanic is out of scope, but the one-size-fits-all "is severed clean
 * off!" wording regardless of weapon type wasn't a real port of
 * anything, just Tobin's own placeholder. This reuses the SAME
 * weapon_verb() category buckets combat_strike() already computes for
 * its per-hit message (sword/axe/mace/dagger/spear/whip), with phrasing
 * drawn from upstream's own critBlunt()/critSlash()/critPierce() wording
 * for each bucket -- `verb` is one of weapon_verb()'s return values. */
static const char *sever_verb_phrase(const char *verb) {
    if (strcmp(verb, "slice") == 0)
        return "is sliced clean off!";
    if (strcmp(verb, "chop") == 0)
        return "is hacked clean off in a spray of gore!";
    if (strcmp(verb, "bludgeon") == 0)
        return "is crushed and torn away!";
    if (strcmp(verb, "stab") == 0)
        return "is punctured through and comes free!";
    if (strcmp(verb, "pierce") == 0)
        return "is skewered clean through and torn loose!";
    if (strcmp(verb, "lash") == 0)
        return "is torn off in the lash's wake!";
    return "is severed clean off!"; /* bare hands / unclassified weapon */
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
 * than duplicating it. `verb` (weapon_verb()'s category, or "hit" if the
 * caller has no weapon context, e.g. the hurtlimb debug path) selects the
 * severing phrase's flavor via sever_verb_phrase() above. */
static void combat_sever_limb(being_t *attacker, being_t *defender, limb_t limb, const char *verb) {
    if (!defender->base.roomp)
        return;
    defender->limbs[limb].bleeding = false;

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

    const char *phrase = sever_verb_phrase(verb);
    tell(attacker, "%s's %s %s\r\n",
         being_display_name_cap(defender, sever_capbuf, sizeof(sever_capbuf)), ln, phrase);
    tell(defender, "Your %s %s\r\n", ln, phrase);

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
        combat_sever_limb(attacker, defender, LIMB_HEAD, verb);
    }
}

/* See combat.h's doc comment. */
bool combat_egotrip_crit(being_t *immortal, being_t *target) {
    if (!target || target->base.kind != THING_PC || !target->base.roomp)
        return false;

    limb_t candidates[LIMB_COUNT];
    int n = 0;
    for (int i = 0; i < LIMB_COUNT; i++) {
        if (is_major_limb((limb_t)i))
            continue;
        if (target->limbs[i].hp <= 0)
            continue;
        candidates[n++] = (limb_t)i;
    }
    if (n == 0)
        return false;

    limb_t limb = candidates[rand() % n];
    target->limbs[limb].hp = 0;
    combat_sever_limb(immortal, target, limb, "hit");
    return true;
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
obj_t *combat_wielded_weapon(const being_t *attacker) {
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

/* MSP hit-sound selection, randomized by class or weapon type (user,
 * 2026-08-06: "randomize sounds to play by weapon type or class" --
 * uploaded a real sound pack under client/sounds/). Class-specific
 * pools take priority (Cleric/Monk/Thief/Mage have dedicated audio);
 * everything else falls back to weapon_verb()'s own slash-vs-generic
 * split. `verb` is NULL for skill/spell damage (combat_apply_skill_
 * damage() has no weapon context), non-NULL for an ordinary melee
 * strike (combat_strike() already computed it). Backstab has no
 * separate damage-source signal to key off (combat_apply_skill_
 * damage() is shared by every skill/spell in the game, not just
 * backstab -- adding one would mean threading a new parameter through
 * every caller) -- backstab.wav is folded into the Thief pool instead,
 * still true to "randomize by class". */
static const char *pick_hit_sound(const being_t *attacker, const char *verb) {
    /* Re-mapped to the user's 2026-08-07 sound pack, which renamed/
     * reorganized the whole file set (old hit.wav->barehand1.wav,
     * thief.wav/thief2.wav->stab4.wav/stab5.wav folded into a shared
     * weapon-type pool, slash.wav->slash8.wav, spell_fireball.wav->
     * spell3.wav, adventure1.wav & co->music1.wav-music5.wav) and added
     * real content for weapon types this never had before: a dedicated
     * "stab" pool for dagger/knife (weapon_verb() already returns
     * "stab" for those, previously falling through to the generic
     * pool) and a "bludgeon" pool for mace/hammer/club/staff. Thief no
     * longer gets its own class pool -- the old 3-file thief_pool
     * (thief/thief2/backstab) is now folded into the shared stab pool
     * instead, so a Thief's sound now reflects the weapon they're
     * actually using (dagger->stab, sword->slash, etc) same as a
     * Warrior's, rather than being fixed regardless of weapon. */
    static const char *cleric_pool[] = { "cleric.wav", "cleric2.wav", "cleric3.wav", "cleric4.wav", 
					 "cleric5.wav", "cleric6.wav", "cleric7.wav" };
    static const char *monk_pool[] = { "monk1.wav", "monk2.wav", "monk3.wav", "monk4.wav",
					"barehand1.wav", "barehand2.wav", "barehand3.wav", "barehand4.wav", "barehand5.wav" };
    static const char *mage_pool[] = { "spell.wav", "spell2.wav", "spell3.wav", "spell4.wav", "spell5.wav", "spell6.wav",
					"spell7.wav", "spell8.wav", "spell9.wav", "spell10.wav", "spell11.wav" };
    static const char *slash_pool[] = {
        "slash1.wav", "slash2.wav", "slash3.wav", "slash4.wav",
        "slash5.wav", "slash6.wav", "slash7.wav", "slash8.wav",
    };
    static const char *stab_pool[] = {
        "stab1.wav", "stab2.wav", "stab3.wav", "stab4.wav", "stab5.wav", "backstab.wav",
    };
    static const char *staff_pool[] = { "staff1.wav", "smash1.wav", "smash2.wav", "smash3.wav", "smash4.wav", "smash5.wav" };
    static const char *generic_pool[] = {
        "barehand1.wav", "barehand2.wav", "barehand3.wav", "barehand4.wav", "barehand5.wav",
    };

    const char **pool = NULL;
    int count = 0;
    switch (attacker->char_class) {
        case CLASS_CLERIC: pool = cleric_pool; count = 1; break;
        case CLASS_MONK:   pool = monk_pool;   count = 4; break;
        case CLASS_MAGE:   pool = mage_pool;   count = 3; break;
        default: break;
    }
    if (!pool) {
        if (verb && (strcmp(verb, "slice") == 0 || strcmp(verb, "chop") == 0)) {
            pool = slash_pool;
            count = 8;
        } else if (verb && strcmp(verb, "stab") == 0) {
            pool = stab_pool;
            count = 6;
        } else if (verb && strcmp(verb, "bludgeon") == 0) {
            pool = staff_pool;
            count = 1;
        } else {
            pool = generic_pool;
            count = 5;
        }
    }
    return pool[rand() % count];
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

    /* Null EVERY slot pointing at `item` (a WEAR_PAIRED item sits in two)
     * before obj_destroy() below, or the partner slot dangles. */
    for (int si = 0; si < LIMB_COUNT; si++)
        if (defender->equipment[si] == item)
            defender->equipment[si] = NULL;
    for (int si = 0; si < 2; si++)
        if (defender->held[si] == item)
            defender->held[si] = NULL;
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
    /* Riposte (spell/skill functional-completeness audit continued,
     * level 20) -- consumes a bonus set by an earlier successful parry
     * (see the parry block below and being.h's riposte_ready doc
     * comment). Read and cleared immediately so it can never linger
     * into a later round. */
    bool riposte_bonus = attacker->riposte_ready;
    attacker->riposte_ready = false;

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

    /* Weapon/barehand proficiency (user, 2026-08-08: "all skills/spells
     * should be learn by doing, linked to use and player stats") -- the 5
     * SKILL_TIER_COMBAT proficiency skills (slash/blunt/pierce/barehand,
     * all classes) used to just mirror combat_disc_pct because nothing
     * called skill_learn_from_doing() on them. `verb` (weapon_verb(),
     * already computed above for hit-message flavor) gives the same
     * weapon-type classification the Warrior-only specialization bonus
     * below already keys off of -- reused here to pick which proficiency
     * skill this swing exercises, for every class, not just Warrior. PCs
     * only (mobs have no player_id/practice-points system to hang skill
     * gain on, same reasoning as the dual-wield learn-by-doing gate
     * above). skill_learn_from_doing() itself is a no-op read if the
     * character hasn't trained any Combat discipline yet (skill_ceiling()
     * returns combat_disc_pct, which is 0 until then). */
    if (attacker->base.kind == THING_PC && !being_is_immortal(attacker)) {
        const char *prof_name = "barehand proficiency";
        if (strcmp(verb, "slice") == 0 || strcmp(verb, "chop") == 0)
            prof_name = "slash proficiency";
        else if (strcmp(verb, "bludgeon") == 0 || strcmp(verb, "lash") == 0)
            prof_name = "blunt proficiency";
        else if (strcmp(verb, "stab") == 0 || strcmp(verb, "pierce") == 0)
            prof_name = "pierce proficiency";
        const skill_def_t *prof_sk = skill_find(attacker->char_class, prof_name, false);
        if (prof_sk)
            skill_learn_from_doing(attacker, prof_sk);
    }

    /* Kubo (Monk, spell/skill functional-completeness audit continued:
     * skill.c's own "Your unarmed strikes scale with skill and level.").
     * Real upstream folds kubo into several separate to-hit/damage
     * combat.cc formulas at once (not individually traced, misc/
     * combat.cc:1928/1933/5981/5985) -- ported as a single proficiency-
     * scaled bonus that fills the exact gap a wielded weapon's own
     * hitroll/damroll would otherwise cover (weapon_hitroll/weapon_
     * damroll both stay 0 bare-handed with no kubo), gated on actually
     * fighting bare-handed so it never stacks with a real weapon's own
     * bonus. Learned-by-doing once per swing here, then reused below for
     * both the to-hit and damage bonus -- calling skill_learn_from_doing()
     * twice per round would be redundant, not double-counted (its own
     * 30s gain cooldown prevents that either way), but one call keeps
     * the intent obviously "used the skill once this swing." */
    int kubo_bonus = 0;
    if (!weapon && !being_is_immortal(attacker) && being_knows_skill(attacker, "kubo")) {
        const skill_def_t *kubo_sk = skill_find(attacker->char_class, "kubo", false);
        if (kubo_sk)
            kubo_bonus = skill_learn_from_doing(attacker, kubo_sk);
    }
    weapon_hitroll += kubo_bonus / 8;
    weapon_damroll += kubo_bonus / 20;

    /* `voplat` (Monk, missing-skill audit, 2026-08-09): real upstream
     * help text -- "the art of redirecting internal energy to your bare
     * hands for use in combat... strike at foes that would not normally
     * be affected by mundane physical attacks... used automatically when
     * fighting barehanded." Tobin has no "immune to nonmagical damage"
     * defender flag to bypass (no mob in this port is ever flagged
     * immune to plain physical hits), so the literal effect has nothing
     * to hook into -- scoped down to the same "empowered bare-handed
     * strike" flavor as `kubo`/`cintai` just above/below: a flat
     * proficiency-scaled hit/damage bonus while fighting unarmed, same
     * shape and magnitude as `kubo`. */
    int voplat_bonus = 0;
    if (!weapon && !being_is_immortal(attacker) && being_knows_skill(attacker, "voplat")) {
        const skill_def_t *voplat_sk = skill_find(attacker->char_class, "voplat", false);
        if (voplat_sk)
            voplat_bonus = skill_learn_from_doing(attacker, voplat_sk);
    }
    weapon_hitroll += voplat_bonus / 8;
    weapon_damroll += voplat_bonus / 20;

    /* Cintai (Monk, spell/skill functional-completeness audit continued,
     * level 5): skill.c's own roster text calls it "A passive to-hit
     * bonus while unarmed," but the real upstream's own attackRound()
     * (misc/combat.cc, found in the fuller peel-sneezymud reference
     * clone) folds it into a GENERAL to-hit bonus function used for
     * every attack, armed or not (alongside level scaling and a mounted
     * Chivalry bonus) -- not gated on being unarmed at all, same
     * "roster text guessed unarmed-only, real source disagrees"
     * correction jirin needed above. Real formula: `(skillValue/20.0)*3.0`,
     * a flat 0-15 bonus at full proficiency -- ported directly rather
     * than scaled down further, since it's already a small, comparable
     * magnitude to every other modifier here. */
    if (!being_is_immortal(attacker) && being_knows_skill(attacker, "cintai")) {
        const skill_def_t *cintai_sk = skill_find(attacker->char_class, "cintai", false);
        if (cintai_sk)
            weapon_hitroll += skill_learn_from_doing(attacker, cintai_sk) * 3 / 20;
    }

    /* Generic combat passives (missing-skill audit, generic/cross-class,
     * 2026-08-10) -- all-class attacker-side to-hit bonuses, same
     * learn-by-doing shape as kubo/cintai just above. `offense` (real
     * combat.cc: bonus += level*skillValue/100) folds into a modest
     * proficiency-scaled hitroll. `advanced offense` (real combat.cc:
     * bonus += skillValue/4*3, 0-75) is larger. `inevitability` (real
     * combat.cc: a repeatedly-activated stacking +hitroll buff capping
     * at +50) ports as a flat passive hitroll instead of true stacking
     * -- same disclosed scope-cut toughness/bloodlust use. `tactics`
     * has no traced upstream effect at all, ported as a small nudge so
     * it is not purely decorative (disclosed). */
    if (!being_is_immortal(attacker) && being_knows_skill(attacker, "offense")) {
        const skill_def_t *off_sk = skill_find(attacker->char_class, "offense", false);
        if (off_sk)
            weapon_hitroll += skill_learn_from_doing(attacker, off_sk) / 10;
    }
    if (!being_is_immortal(attacker) && being_knows_skill(attacker, "advanced offense")) {
        const skill_def_t *aoff_sk = skill_find(attacker->char_class, "advanced offense", false);
        if (aoff_sk)
            weapon_hitroll += skill_learn_from_doing(attacker, aoff_sk) / 6;
    }
    if (!being_is_immortal(attacker) && being_knows_skill(attacker, "inevitability")) {
        const skill_def_t *inev_sk = skill_find(attacker->char_class, "inevitability", false);
        if (inev_sk)
            weapon_hitroll += skill_learn_from_doing(attacker, inev_sk) / 8;
    }
    if (!being_is_immortal(attacker) && being_knows_skill(attacker, "tactics")) {
        const skill_def_t *tac_sk = skill_find(attacker->char_class, "tactics", false);
        if (tac_sk)
            weapon_hitroll += skill_learn_from_doing(attacker, tac_sk) / 12;
    }

    /* Weapon specializations (missing-skill audit, "all level 1 ... all
     * of those should be automatic", user 2026-08-04) -- same passive,
     * learn-by-doing shape as kubo/cintai just above: `verb` (already
     * computed) picks which of the 5 specializations applies to
     * whatever `attacker` is currently wielding (or bare hands). The
     * specializations are Advanced-tier now (user: "specialization skills
     * in advanced"), so their ceiling is advanced_disc_pct -- until the
     * Warrior has mastered Basic+Combat and begun Advanced practice,
     * skill_learn_from_doing() returns 0 here (zero ceiling) and no bonus
     * or gain applies; once unlocked, proficiency grows on every swing and
     * folds straight into this same round's hit/damage bonus, like
     * kubo/cintai do. weapon_verb() has no "ranged" bucket, so this melee
     * block still never trains/applies "ranged proficiency"/"ranged
     * specialization" -- cmd_shoot.c's own `shoot` command (TODO.md's
     * "Common skills" backlog item) does both instead, the same
     * damage-only shape this whole block uses. "lash" (whip/flail) is
     * approximated as blunt -- closer to
     * a flail's real classification than either of the other two. At
     * exactly 100% proficiency ("mastered", user: "bigger passive
     * bonus" rather than a separate advanced-skill unlock), the bonus
     * steps up further rather than just topping out flat. */
    if (!being_is_immortal(attacker) && attacker->char_class == CLASS_WARRIOR) {
        const char *spec_name = NULL;
        if (strcmp(verb, "slice") == 0 || strcmp(verb, "chop") == 0)
            spec_name = "slash specialization";
        else if (strcmp(verb, "bludgeon") == 0 || strcmp(verb, "lash") == 0)
            spec_name = "blunt specialization";
        else if (strcmp(verb, "stab") == 0 || strcmp(verb, "pierce") == 0)
            spec_name = "pierce specialization";
        else if (!weapon)
            spec_name = "barehand specialization";

        if (spec_name) {
            const skill_def_t *spec_sk = skill_find(CLASS_WARRIOR, spec_name, false);
            if (spec_sk) {
                int spec_prof = skill_learn_from_doing(attacker, spec_sk);
                weapon_hitroll += spec_prof / 10;
                weapon_damroll += spec_prof / 25;
                if (spec_prof >= 100) {
                    weapon_hitroll += 3;
                    weapon_damroll += 2;
                }
            }
        }

        /* `bloodlust` (missing-skill audit, 2026-08-05, Warrior): real
         * upstream is a passive per-round chance for a stacking damage
         * buff while fighting (disc_warrior_brawling.cc's
         * doBloodlust()). Ported as a flat passive hitroll/damroll
         * bonus scaling with proficiency instead of true stacking --
         * same shape as the specialization bonus just above, no
         * separate stacking-affect infrastructure needed. */
        const skill_def_t *bloodlust_sk = skill_find(CLASS_WARRIOR, "bloodlust", false);
        if (bloodlust_sk && being_knows_skill(attacker, "bloodlust")) {
            int bloodlust_prof = skill_learn_from_doing(attacker, bloodlust_sk);
            weapon_hitroll += bloodlust_prof / 15;
            weapon_damroll += bloodlust_prof / 10;
        }

        /* Two-handed specialization (user 2026-08-18): an extra DAMAGE bonus
         * while wielding a two-handed (WEAR_PAIRED) weapon -- same passive
         * learn-by-doing shape as the weapon specializations above, but
         * damage-weighted (a two-hander's payoff is the heavy hit, not
         * accuracy). obj_is_paired(NULL) is false, so bare hands get nothing. */
        if (obj_is_paired(weapon)
            && being_knows_skill(attacker, "two-handed specialization")) {
            const skill_def_t *th_sk = skill_find(CLASS_WARRIOR, "two-handed specialization", false);
            if (th_sk) {
                int th_prof = skill_learn_from_doing(attacker, th_sk);
                weapon_damroll += th_prof / 12;
                if (th_prof >= 100)
                    weapon_damroll += 3;
            }
        }
    }

    int base_roll = rand() % 100;
    int modifier = (attacker->attrs.dexterity - defender->attrs.dexterity) / 4;
    modifier += weapon_hitroll;
    if (being_has_destroyed_limb(attacker))
        modifier -= DESTROYED_LIMB_HIT_PENALTY;
    /* Blindness (Cleric, level 21, audit continued): a blinded attacker
     * swings far less accurately. Real upstream checks AFF_BLIND in
     * several places without one single traced to-hit formula -- this
     * is a disclosed approximation, not a literal port, reusing the
     * same flat-penalty shape as the destroyed-limb modifier above. */
    if (being_has_affect(attacker, AFFECT_BLIND)) {
        /* `blindfighting` (Monk, level 25, level-25 audit batch:
         * "Reduces the penalty for fighting while blinded."). Halves the
         * flat penalty above rather than removing it outright -- a
         * skilled blind fighter is still worse off than a sighted one,
         * just not as crippled. */
        modifier -= being_knows_skill(attacker, "blindfighting")
                     ? DESTROYED_LIMB_HIT_PENALTY / 2 : DESTROYED_LIMB_HIT_PENALTY;
    }
    /* `alcoholism` (missing-skill audit batch C, 2026-08-09): real
     * upstream continuously scales DOWN crit chance while drunk
     * (`critChance -= 2 * getCond(DRUNK)`, crit_combat.cc) -- Tobin has
     * no separate crit-chance stat to subtract from (its own crit
     * mechanic is a second weighted-limb draw, see `critical hitting`'s
     * own comment below), so this lands as the closest real equivalent:
     * a continuously-scaling flat to-hit penalty instead, same shape as
     * the destroyed-limb/blindness penalties just above. Zero at
     * drunk=0, up to a full DESTROYED_LIMB_HIT_PENALTY at drunk=100.
     * SKILL_ALCOHOLISM's own real job is reducing how drunk you get in
     * the first place (being_gain_drunk(), vitals.c), not blunting this
     * penalty directly -- same division of labor real upstream uses. */
    if (attacker->progress.drunk > 0)
        modifier -= (attacker->progress.drunk * DESTROYED_LIMB_HIT_PENALTY) / 100;
    /* Meaningful limb damage (TODO.md, user: "make individual limb hits
     * actually hurt"): the mirror image of the attacker-side penalty just
     * above -- a destroyed limb doesn't just throw off YOUR swing, it
     * also makes you an easier target (same flat, non-stacking amount, no
     * particular reason but consistency with the offense-side number). */
    if (being_has_destroyed_limb(defender))
        modifier += DESTROYED_LIMB_HIT_PENALTY;
    /* `faerie fire` (Mage, level 6, stub-audit fix): a pink aura marking
     * the defender, same flat easier-to-hit bonus as a destroyed limb --
     * no particular reason for reusing the same number, just consistency
     * with every other flat to-hit modifier in this function. */
    if (being_has_affect(defender, AFFECT_FAERIE_FIRE))
        modifier += DESTROYED_LIMB_HIT_PENALTY;
    /* `shield of mists` (Shaman/Druid audit batch C, 2026-08-09) -- see
     * AFFECT_SHIELD_OF_MISTS's own doc comment (affect.h). */
    if (being_has_affect(defender, AFFECT_SHIELD_OF_MISTS))
        modifier -= DESTROYED_LIMB_HIT_PENALTY;
    /* Ward-buff split (2026-08-17): AFFECT_ARMOR (armor/stone skin/
     * barkskin/shield/flaming flesh/...) improves the defender's AC --
     * ported as a flat attacker to-hit penalty, same defender-side
     * shape as AFFECT_SHIELD_OF_MISTS just above. */
    if (being_has_affect(defender, AFFECT_ARMOR))
        modifier -= ARMOR_HIT_PENALTY;
    /* AFFECT_BLESS (bless/consecrate/crusade) sharpens the ATTACKER's
     * swing -- an offensive to-hit bonus (its damage half lands in the
     * damage block below). */
    if (being_has_affect(attacker, AFFECT_BLESS))
        modifier += BLESS_HIT_BONUS;
    /* `living vines` (Shaman/Druid audit batch C, 2026-08-09) -- see
     * AFFECT_LIVING_VINES's own doc comment (affect.h). */
    if (being_has_affect(defender, AFFECT_LIVING_VINES))
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
    if (defender->position != POSITION_STANDING && defender->position != POSITION_MOUNTED) {
        modifier += NON_STANDING_HIT_BONUS;
        /* `groundfighting` (Monk, missing-skill audit, 2026-08-09: real
         * upstream disc_monk_leverage.cc is a passive that "reduces the
         * combat penalties normally associated with fighting while
         * prone... at maximum proficiency, can eliminate these penalties
         * entirely" -- ported as a direct proficiency-scaled clawback of
         * the NON_STANDING_HIT_BONUS just applied above, capped so it can
         * fully cancel the penalty at 100% but never flip it into a net
         * bonus. */
        if (!being_is_immortal(defender) && being_knows_skill(defender, "groundfighting")) {
            const skill_def_t *gf_sk = skill_find(defender->char_class, "groundfighting", false);
            if (gf_sk) {
                int gf_prof = skill_learn_from_doing(defender, gf_sk);
                int clawback = NON_STANDING_HIT_BONUS * gf_prof / 100;
                modifier -= clawback;
            }
        }
    }
    /* Mounted ATTACKER bonus -- the height/mobility edge of fighting from
     * horseback (Mount / riding system, Sneezy → Tobin feature audit). */
    if (attacker->position == POSITION_MOUNTED)
        modifier += MOUNTED_ATTACK_BONUS;
    /* `close quarters fighting` (Warrior, missing-skill audit, 2026-08-09):
     * real upstream (disc_warrior_brawling.cc-family help text) "improves
     * both offensive and defensive capabilities when fighting multiple
     * opponents or in cramped quarters." Tobin has no multi-opponent combat
     * engine (`being_t.fighting` is a single 1v1 pointer, see combat.h) to
     * hook a literal "how many are attacking you" count into, so this is
     * scoped to the closest real proxy: whether more than one hostile
     * (mob or PC, excluding the two combatants themselves) is CURRENTLY
     * fighting someone in the same room -- a genuine "chaotic melee"
     * signal Tobin does track (being_t.fighting on other room occupants),
     * just not a per-attacker threat count. Applies to both sides
     * symmetrically (a to-hit boost when attacking, a to-hit reduction
     * when defending), matching the "both offensive and defensive"
     * wording. */
    if (defender->base.roomp) {
        int room_combatants = 0;
        for (thing_t *t = defender->base.roomp->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC && t->kind != THING_MOB)
                continue;
            being_t *bt = (being_t *)t;
            if (bt->fighting)
                room_combatants++;
        }
        if (room_combatants > 2) {
            if (!being_is_immortal(attacker) && attacker->base.kind == THING_PC
                && being_knows_skill(attacker, "close quarters fighting")) {
                const skill_def_t *cqf_sk = skill_find(attacker->char_class, "close quarters fighting", false);
                if (cqf_sk)
                    modifier += skill_learn_from_doing(attacker, cqf_sk) / 12;
            }
            if (!being_is_immortal(defender) && defender->base.kind == THING_PC
                && being_knows_skill(defender, "close quarters fighting")) {
                const skill_def_t *cqf_sk = skill_find(defender->char_class, "close quarters fighting", false);
                if (cqf_sk)
                    modifier -= skill_learn_from_doing(defender, cqf_sk) / 12;
            }
        }
    }
    /* Armor class (user 2026-07-11: "Armor & protection... complete the
     * to-hit/defense formula depth"): a defender's total worn AC
     * (being_total_ac(), obj.h) makes them harder to hit. Scaled by half
     * so it sits in the same rough magnitude as the other single
     * modifiers above rather than dominating them. */
    modifier -= being_total_ac(defender) / 2;

    /* Oomlat (Monk, spell/skill functional-completeness audit continued:
     * skill.c's own "A passive armor bonus while fighting unarmed.").
     * Real upstream (misc/combat.cc:2787) scales the defender's own
     * effective armor value up by `skill/250` before the normal AC-to-
     * bonus conversion runs -- ported as an extra flat subtraction from
     * the to-hit modifier here instead (Tobin's `being_total_ac()` is
     * already a single summary number, not the original's raw 1000-point
     * armor scale to multiply into), gated on the DEFENDER actually
     * fighting bare-handed (no shield/weapon-hand tradeoff to balance
     * against, matching the real comment's own "extra AC to balance the
     * lack of a shield penalty" reasoning). */
    if (!combat_wielded_weapon(defender) && !being_is_immortal(defender)
        && being_knows_skill(defender, "oomlat")) {
        const skill_def_t *oomlat_sk = skill_find(defender->char_class, "oomlat", false);
        if (oomlat_sk)
            modifier -= skill_learn_from_doing(defender, oomlat_sk) / 8;
    }

    /* `iron flesh` (Monk, level 31 -- Session 158 backlog). A hardened,
     * unarmored body is tougher to land a clean blow on: a barehanded
     * defender-side to-hit reduction, same shape/insertion as `oomlat`'s
     * own barehand AC bonus just above (upstream iron flesh is likewise a
     * nekkid-monk armor bonus, misc/being.cc). */
    if (!combat_wielded_weapon(defender) && !being_is_immortal(defender)
        && being_knows_skill(defender, "iron flesh")) {
        const skill_def_t *ifl_sk = skill_find(defender->char_class, "iron flesh", false);
        if (ifl_sk)
            modifier -= skill_learn_from_doing(defender, ifl_sk) / 8;
    }

    /* `focused avoidance` (missing-skill audit, 2026-08-05, generic/
     * cross-class): real upstream (disc_advanced_defense.cc's
     * canFocusedAvoidance()) is a passive dodge check scaled by agility,
     * checked once per incoming attack. Ported as a flat to-hit-modifier
     * reduction against the defender instead -- same shape and insertion
     * point as `oomlat`'s AC bonus just above, no separate dodge-roll
     * layer needed. Not gated on fighting bare-handed (real upstream's
     * own gate is awake+standing only, no weapon restriction). */
    if (!being_is_immortal(defender) && being_knows_skill(defender, "focused avoidance")) {
        const skill_def_t *avoid_sk = skill_find(defender->char_class, "focused avoidance", false);
        if (avoid_sk)
            modifier -= skill_learn_from_doing(defender, avoid_sk) / 6;
    }
    /* `dodge` (Unimplemented skills/spells backlog, Session 158 audit:
     * Thief, level 1 -- "passive avoidance"). Real upstream (SKILL_DODGE)
     * is a per-attack passive avoidance check; ported here as the same
     * flat proficiency-scaled to-hit reduction against the defender as
     * `focused avoidance` just above (its closest existing analog),
     * making a trained dodger genuinely harder to land a blow on. Awake-
     * and-mobile only in spirit; no weapon restriction, same as focused
     * avoidance. Trains on every incoming swing it's checked against. */
    if (!being_is_immortal(defender) && being_knows_skill(defender, "dodge")) {
        const skill_def_t *dodge_sk = skill_find(defender->char_class, "dodge", false);
        if (dodge_sk)
            modifier -= skill_learn_from_doing(defender, dodge_sk) / 6;
    }

    /* `Oomlat Philosophy` (Monk, missing-skill audit, 2026-08-09) -- the
     * advanced-tier sibling of the level-1 `oomlat` skill just above.
     * Real upstream help text: "concentrate to the utmost during combat...
     * a hefty defensive bonus, preventing them from being hit as often."
     * Unlike plain `oomlat`, NOT gated on fighting bare-handed (the
     * roster text is about mental focus, not an unarmed-AC balance
     * trade), so it stacks with plain `oomlat`/weapon use alike -- a
     * bigger divisor than `oomlat`'s own would be redundant, so this
     * uses the same shape as `focused avoidance` (its closest existing
     * "flat mental-focus to-hit reduction" analog) with a slightly
     * larger effect ("hefty" per the roster text). */
    if (!being_is_immortal(defender) && being_knows_skill(defender, "Oomlat Philosophy")) {
        const skill_def_t *oomp_sk = skill_find(defender->char_class, "Oomlat Philosophy", false);
        if (oomp_sk)
            modifier -= skill_learn_from_doing(defender, oomp_sk) / 5;
    }

    /* `defense` (docs/Spell Assignments.xlsx gap audit, 2026-08-08): real
     * upstream is a flat passive AC-style bonus (combat.cc), available
     * from level 1 -- unlike `focused avoidance`'s level 30, this is a
     * character's FIRST passive defensive skill, not a duplicate of one
     * already in the roster. Same to-hit-modifier-reduction shape and
     * insertion point as `oomlat`/`focused avoidance` just above, smaller
     * divisor so it doesn't outweigh either at full proficiency. */
    if (!being_is_immortal(defender) && being_knows_skill(defender, "defense")) {
        const skill_def_t *def_sk = skill_find(defender->char_class, "defense", false);
        if (def_sk)
            modifier -= skill_learn_from_doing(defender, def_sk) / 10;
    }

    /* `advanced defense` (missing-skill audit, generic/cross-class,
     * 2026-08-10): real upstream (combat.cc) is a defense-combat-mode
     * AC bonus scaling with proficiency. Tobin has no combat modes, so
     * ported as a passive to-hit-modifier reduction on the defender,
     * same shape/insertion point as `focused avoidance`/`defense` just
     * above, a touch larger (it is the advanced-tier counterpart). */
    if (!being_is_immortal(defender) && being_knows_skill(defender, "advanced defense")) {
        const skill_def_t *adef_sk = skill_find(defender->char_class, "advanced defense", false);
        if (adef_sk)
            modifier -= skill_learn_from_doing(defender, adef_sk) / 6;
    }

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
    /* Spell/skill functional-completeness audit (2026-07-27): a
     * berserking attacker's hits can't be parried at all (roster's own
     * "much harder to ... parry while raging" description, cmd_berserk.c). */
    if (!being_is_immortal(defender) && being_knows_skill(defender, "parry")
        && !being_has_affect(attacker, AFFECT_BERSERK)) {
        const skill_def_t *parry_sk = skill_find(defender->char_class, "parry", false);
        if (parry_sk && skill_roll_success(skill_learn_from_doing(defender, parry_sk) / 4)) {
            if (!(attacker->pflags & PLR_NOSPAM))
                tell(attacker, "%s parries your attack!\r\n", being_display_name(defender));
            if (!(defender->pflags & PLR_NOSPAM))
                tell(defender, "You parry %s's attack!\r\n", being_display_name(attacker));

            /* Riposte (level 20): real upstream's own 50% dice roll on
             * top of a separate skill check, see being.h's riposte_ready
             * comment for the full mechanism. */
            if (!being_is_immortal(defender) && being_knows_skill(defender, "riposte")
                && rand() % 100 < 50) {
                const skill_def_t *riposte_sk = skill_find(defender->char_class, "riposte", false);
                if (riposte_sk && skill_roll_success(skill_learn_from_doing(defender, riposte_sk))) {
                    defender->riposte_ready = true;
                    if (!(attacker->pflags & PLR_NOSPAM))
                        tell(attacker, "%s uses their parry to execute a riposte!\r\n",
                             being_display_name(defender));
                    if (!(defender->pflags & PLR_NOSPAM))
                        tell(defender, "You use your parry to execute a riposte!\r\n");
                }
            }
            return false;
        }
    }

    /* Jirin (Monk, spell/skill functional-completeness audit continued:
     * skill.c's own "Dodge, block, or deflect an incoming unarmed
     * attack."). Found the real implementation in the fuller peel-
     * sneezymud reference source (disc/disc_monk.cc's monkDodge(),
     * not present in the original bundled sneezymud-master/) -- it's
     * the Monk's own general anti-hit defense, "a replacement for
     * Monk's lack of AC" per its own comment, checked against ANY
     * incoming hit regardless of the ATTACKER's weapon -- NOT gated on
     * the attacker being unarmed the way the roster's own flavor text
     * implies. Reuses parry's proven shape (quartered proficiency roll,
     * checked before the normal hit/miss roll, negates the attack
     * outright) rather than porting monkDodge()'s own two-stage roll
     * (a skill-diff-modifier gate, then a separate bSuccess() -- more
     * involved than any other passive in this audit, without a clear
     * Tobin equivalent for getSkillDiffModifier() to port faithfully). */
    if (!being_is_immortal(defender) && being_knows_skill(defender, "jirin")) {
        const skill_def_t *jirin_sk = skill_find(defender->char_class, "jirin", false);
        if (jirin_sk && skill_roll_success(skill_learn_from_doing(defender, jirin_sk) / 4)) {
            if (!(attacker->pflags & PLR_NOSPAM))
                tell(attacker, "%s deflects your attack!\r\n", being_display_name(defender));
            if (!(defender->pflags & PLR_NOSPAM))
                tell(defender, "You deflect %s's attack!\r\n", being_display_name(attacker));
            return false;
        }
    }

    if (hit_roll < 50 && !riposte_bonus) {
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

    /* `iron fist` (Monk, level 25, level-25 audit batch: "Bonus
     * strength-based damage while your hands are bare."). Doubles the
     * STR-derived term of the base formula above -- only while
     * genuinely bare-handed (no weapon), matching the roster text
     * exactly rather than a flat bonus that would also apply while
     * armed. */
    if (!weapon && being_knows_skill(attacker, "iron fist"))
        dmg += (attacker->attrs.strength - ATTR_BASE) / 4;

    /* `iron muscles` (Monk, level 42 -- Session 158 backlog). Bare-fisted
     * conditioning: a proficiency-scaled flat damage bonus while unarmed,
     * the sibling of `iron fist`'s STR-term double just above (up to +5 at
     * full proficiency). PC attackers only (learn-by-doing). */
    if (!weapon && attacker->base.kind == THING_PC
        && being_knows_skill(attacker, "iron muscles")) {
        const skill_def_t *imu_sk = skill_find(attacker->char_class, "iron muscles", false);
        if (imu_sk)
            dmg += skill_learn_from_doing(attacker, imu_sk) / 20;
    }

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

    /* Weapon sharpness (user 2026-07-12, weapon depth; extended by the
     * `sharpen`/`smooth` skills, missing-skill audit batch C,
     * 2026-08-09): an edged/piercing weapon (anything weapon_verb()
     * calls slice/chop/stab/pierce, not the blunt "bludgeon"/bare-
     * handed "hit") lands a cleaner, more consistent wound than a
     * dull one. Originally a flat +1; now scaled by the weapon's own
     * mutable obj_t.sharpness (0-100, SHARPNESS_DEFAULT=50 out of the
     * box) so `sharpen` raising it actually pays off -- still +1 at
     * the old default, up to +2 at max sharpness. A blunt weapon now
     * gets the SAME scaling off the SAME field (raised by `smooth`
     * instead) rather than a second, separate stat -- real upstream's
     * own curSharp is exactly this, just relabeled "bluntness" for a
     * blunt weapon (obj_base_weapon.cc's changeBaseWeaponValue1()) --
     * a deliberate, disclosed rebalance: blunt weapons previously got
     * ZERO bonus from this mechanic at all, `smooth` now gives them
     * the parallel benefit `sharpen` already gave edged weapons. Bare
     * hands (weapon == NULL) still get nothing, unchanged. */
    if (weapon)
        dmg += weapon->sharpness / 50;

    if (dmg < 1)
        dmg = 1;

    /* Gamewide damage multiplier (user 2026-07-12's `balance` command) --
     * same class/race rule as the to-hit modifier above. */
    /* AFFECT_BLESS damage half (its to-hit half is in the modifier
     * block above) -- a small flat pre-multiplier bonus. */
    if (being_has_affect(attacker, AFFECT_BLESS))
        dmg += BLESS_DAM_BONUS;

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

    /* AFFECT_PROTECTION (protection from */ /* ... / resistance) -- a
     * flat percentage cut, distinct from Sanctuary's halving; both
     * stack (a sanctuary'd AND protected defender gets both), applied
     * right after Sanctuary same as toughness below. */
    if (being_has_affect(defender, AFFECT_PROTECTION)) {
        dmg -= dmg * PROTECTION_DAM_PCT / 100;
        if (dmg < 1)
            dmg = 1;
    }

    /* AFFECT_FORTIFY (Warrior `fortify`, cmd_fortify.c) -- a shield-wall
     * defensive stance takes a flat FORTIFY_DAM_PCT off the incoming hit,
     * stacking with Sanctuary/Protection above, same placement and shape
     * as the AFFECT_PROTECTION cut just above. */
    if (being_has_affect(defender, AFFECT_FORTIFY)) {
        dmg -= dmg * FORTIFY_DAM_PCT / 100;
        if (dmg < 1)
            dmg = 1;
    }

    /* `toughness` (missing-skill audit, 2026-08-05, generic/cross-class):
     * real upstream is a per-hit chance to gain a stacking damage-
     * immunity buff (combat.cc's doToughness()). Ported as a flat
     * passive damage-reduction percentage instead -- up to 20% off at
     * 100% proficiency -- same "flat passive instead of true stacking"
     * scope-cut `bloodlust` already used above. Applied after Sanctuary
     * so a toughened, sanctuary'd defender stacks both reductions. */
    if (defender->base.kind == THING_PC && !being_is_immortal(defender)
        && being_knows_skill(defender, "toughness")) {
        const skill_def_t *tough_sk = skill_find(defender->char_class, "toughness", false);
        if (tough_sk) {
            int tough_prof = skill_learn_from_doing(defender, tough_sk);
            dmg -= dmg * (tough_prof / 5) / 100;
            if (dmg < 1)
                dmg = 1;
        }
    }

    /* `iron skin` (Monk, level 35 -- Session 158 backlog). Skin like
     * hammered iron: a flat passive damage-reduction percentage, same
     * shape as `toughness` just above (upstream iron skin is a general
     * damage-immunity amount, misc/immunity.cc). Stacks with the others. */
    if (defender->base.kind == THING_PC && !being_is_immortal(defender)
        && being_knows_skill(defender, "iron skin")) {
        const skill_def_t *isk_sk = skill_find(defender->char_class, "iron skin", false);
        if (isk_sk) {
            int isk_prof = skill_learn_from_doing(defender, isk_sk);
            dmg -= dmg * (isk_prof / 5) / 100;
            if (dmg < 1)
                dmg = 1;
        }
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

    /* `calm mount` (Deikhan mounted-combat trio, missing-skill audit
     * batch C, 2026-08-09) -- see this block's own doc comment above
     * the function for the real-vs-Tobin mechanic gap this closes.
     * Every real landed hit on a mounted PC defender has a flat 12%
     * base chance of unseating them; `calm mount` (plus a smaller
     * `advanced riding` contribution, matching real upstream's own
     * averaging of the two skills in advancedRidingBonus()) reduces
     * that all the way to 0% at full combined proficiency. */
    if (dmg > 0 && defender->position == POSITION_MOUNTED && defender->mount
        && defender->base.kind == THING_PC && !being_is_immortal(defender)) {
        int spook_chance = 12;
        if (being_knows_skill(defender, "calm mount")) {
            const skill_def_t *calm_sk = skill_find(defender->char_class, "calm mount", false);
            if (calm_sk) {
                int calm_prof = skill_learn_from_doing(defender, calm_sk);
                int adv_prof = 0;
                if (being_knows_skill(defender, "advanced riding")) {
                    const skill_def_t *adv_sk = skill_find(defender->char_class, "advanced riding", false);
                    if (adv_sk)
                        adv_prof = skill_learn_from_doing(defender, adv_sk);
                }
                spook_chance -= ((calm_prof * 2 + adv_prof) / 3) * spook_chance / 100;
                if (spook_chance < 0)
                    spook_chance = 0;
            }
        }
        if (spook_chance > 0 && (rand() % 100) < spook_chance) {
            being_t *spooked_mount = defender->mount;
            defender->mount = NULL;
            spooked_mount->rider = NULL;
            defender->position = POSITION_SITTING;
            spooked_mount->position = POSITION_STANDING;
            if (defender->desc)
                descriptor_notify(defender->desc, "<y>Your mount panics at the blow and throws you to the ground!<z>\r\n");
        }
    }

    /* `thornflesh` (Shaman/Druid audit batch C, 2026-08-09) -- direct
     * port of real upstream's own defender-side reflection check
     * (combat.cc): min(dmg-1, 3) of whatever this hit just dealt
     * bounces back onto the attacker, floored at 0 (a miss/zero-damage
     * hit reflects nothing). Checked here so it uses the FINAL, fully-
     * modified dmg value -- same reasoning Sanctuary's halving above
     * this function already documents. */
    if (dmg > 0 && being_has_affect(defender, AFFECT_THORNFLESH) && !being_is_immortal(attacker)) {
        int reflected = dmg - 1;
        if (reflected > 3)
            reflected = 3;
        if (reflected > 0) {
            attacker->progress.hp -= reflected;
            if (attacker->progress.hp < 1)
                attacker->progress.hp = 1;
            if (attacker->desc)
                descriptor_notify(attacker->desc, "<o>The thorns on their body bite into you as you land the hit!<z>\r\n");
            if (defender->desc)
                descriptor_notify(defender->desc, "<o>Your thorns bite into your attacker!<z>\r\n");
        }
    }

    /* AFFECT_DAMAGE_MIRROR (plasma mirror / reflective shield) -- bounces
     * a percentage of the final hit back onto the attacker, same
     * defender-side reflect plumbing as AFFECT_THORNFLESH just above but
     * %-based rather than a flat min(dmg-1,3). Uses the FINAL modified
     * dmg for the same reason. Never harms an immortal attacker. */
    if (dmg > 0 && being_has_affect(defender, AFFECT_DAMAGE_MIRROR) && !being_is_immortal(attacker)) {
        int reflected = dmg * DAMAGE_MIRROR_PCT / 100;
        if (reflected > 0) {
            attacker->progress.hp -= reflected;
            if (attacker->progress.hp < 1)
                attacker->progress.hp = 1;
            if (attacker->desc)
                descriptor_notify(attacker->desc, "<c>Your blow rebounds off a shimmering mirror shield and strikes you!<z>\r\n");
            if (defender->desc)
                descriptor_notify(defender->desc, "<c>Your mirror shield hurls the attack back at its source!<z>\r\n");
        }
    }

    /* `poison weapon` (Thief, level 25) -- if the attacker's weapon is
     * venom-coated (AFFECT_POISON_BLADE) and this hit actually bit, a
     * proc-chance leaves the victim poisoned. Gated on a real wielded
     * weapon (a coating needs a blade) and never envenoms an immortal.
     * Uses the SAME final-dmg>0 defender-side placement as thornflesh/
     * damage-mirror just above. */
    if (dmg > 0 && !being_is_immortal(defender)
        && being_has_affect(attacker, AFFECT_POISON_BLADE)
        && combat_wielded_weapon(attacker)
        && (rand() % 100) < POISON_BLADE_PROC_PCT) {
        being_apply_affect(defender, AFFECT_POISON, POISON_BLADE_POISON_ROUNDS);
        if (attacker->desc)
            descriptor_notify(attacker->desc, "<g>Your venom-slick edge bites deep -- the poison takes hold!<z>\r\n");
        if (defender->desc)
            descriptor_notify(defender->desc, "<g>The blade's venom courses into your wound!<z>\r\n");
    }

    limb_t limb = pick_weighted_limb((body_type_t)defender->body_type);
    /* `critical hitting` (Monk, level 25, level-25 audit batch:
     * "Improves your access to the harshest critical-hit outcomes.").
     * Tobin's own crit mechanic (this function's own doc comment) is
     * "a hit that crosses a limb's HP to 0%" -- no separate crit-chance
     * roll to boost. Ported instead as a second weighted-limb draw,
     * kept only if it landed on a MAJOR limb (is_major_limb() above) --
     * a real, if modest, lean toward the harsher critical outcomes the
     * roster text describes, without adding a whole new RNG layer. */
    if (attacker->base.kind == THING_PC && being_knows_skill(attacker, "critical hitting")) {
        limb_t reroll = pick_weighted_limb((body_type_t)defender->body_type);
        if (is_major_limb(reroll))
            limb = reroll;
    }
    /* `power move` (Warrior, missing-skill audit, 2026-08-09): real
     * upstream help text describes the Warrior counterpart to the
     * Monk's `critical hitting` -- "a higher number of their basic
     * attacks becoming critical hits... even when compared to monks who
     * learn the critical hitting ability," but with "less of an increase
     * to their overall number of critical hits" (i.e. hits MORE often
     * but each one less reliably severe than the Monk's own version).
     * Ported with the exact same major-limb-reroll mechanic as
     * `critical hitting` just above (no separate roster text guessed a
     * different shape for it), gated on `power move`'s own proficiency
     * so the "achieve it sooner" framing tracks skill growth. */
    if (attacker->base.kind == THING_PC && !being_is_immortal(attacker)
        && being_knows_skill(attacker, "power move")) {
        const skill_def_t *pm_sk = skill_find(attacker->char_class, "power move", false);
        if (pm_sk && skill_learn_from_doing(attacker, pm_sk) > 0) {
            limb_t reroll = pick_weighted_limb((body_type_t)defender->body_type);
            if (is_major_limb(reroll))
                limb = reroll;
        }
    }
    /* Focus attack (Warrior, user 2026-08-03: "in tobin its a warrior
     * skill" / "focused attack should be automatic"). Real upstream
     * (SKILL_FOCUS_ATTACK/AFF_FOCUS_ATTACK, cmd_focus_attack.cc/
     * crit_combat.cc) is a manually-triggered command with a cooldown
     * that forces the NEXT swing to crit. Ported automatic instead, same
     * "no separate command" simplification `critical hitting` (Monk)
     * just above already established for this exact reroll-toward-a-
     * MAJOR-limb mechanic -- gated on Warrior's own skill, with the real
     * upstream's own flavor message when it actually lands on one (real
     * upstream's own "$n executes a focused attack!"). User 2026-08-03:
     * "focused attack is firing too much, decrease success by 50%" -- an
     * extra 50% coin-flip gates the attempt itself (on top of the
     * major-limb reroll odds already below), roughly halving how often
     * it triggers overall. */
    bool focused_attack = false;
    if (attacker->base.kind == THING_PC && being_knows_skill(attacker, "focus attack")
        && rand() % 100 < 50) {
        limb_t reroll = pick_weighted_limb((body_type_t)defender->body_type);
        if (is_major_limb(reroll)) {
            limb = reroll;
            focused_attack = true;
        }
    }
    if (focused_attack) {
        tell(attacker, "You focus intensely, striking with practiced precision!\r\n");
        if (attacker->base.roomp) {
            char capbuf[128], room_msg[192];
            snprintf(room_msg, sizeof(room_msg), "%s focuses intensely, striking with practiced precision!\r\n",
                     being_display_name_cap(attacker, capbuf, sizeof(capbuf)));
            descriptor_room_echo(attacker->base.roomp, attacker, room_msg);
        }
    }
    int pct_before = being_limb_pct(defender, limb);
    int limb_hp_before = defender->limbs[limb].hp; /* pre-hit capacity, for describe_dam() below */
    /* Blood/limb-damage generation rate (TODO.md priority item, user
     * 2026-07-30): overall HP still takes the full `dmg` (combat
     * lethality/pacing is untouched), but the SPECIFIC limb only takes
     * half of it -- limbs decay slower relative to overall vitality, so
     * they cross into a bloody status tier (and spawn the blood pool
     * below) roughly half as often too, without a second, separate
     * probability roll stacked on top. */
    defender->progress.hp -= dmg;
    combat_award_hit_xp(attacker, defender, dmg);
    /* GMCP/MSDP push + MSP hit sound (TobinMUD Client project,
     * 2026-08-05) -- no-op for a mob defender or a descriptor that
     * never opted into GMCP/MSDP/MSP (being_notify_vitals_changed()/
     * descriptor_send_msp_sound() both check that internally). */
    being_notify_vitals_changed(defender);
    if (defender->desc)
        descriptor_send_msp_sound(defender->desc, pick_hit_sound(attacker, verb), 100);
    being_hurt_limb_only(defender, limb, (dmg + 1) / 2);
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
    /* Uses the HALVED limb-only damage, not the full `dmg` -- the message's
     * severity ratio (against limb_hp_before) must match what actually
     * happened to THIS limb, now that it takes only half of a hit's
     * overall damage (see this function's own limb-damage-rate comment
     * above). */
    const char *intensity = describe_dam((dmg + 1) / 2, limb_hp_before, verb);
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

        /* `bandage` (docs/Spell Assignments.xlsx gap audit, 2026-08-08) --
         * this same tier-crossing (blood pool spawns, once per crossing)
         * now also marks the limb `bleeding` so vitals_tick_impl() (vitals.c)
         * can chip away at the owner over time until treated. */
        defender->limbs[limb].bleeding = true;
    }

    bool instadeath = false;
    if (pct_before > 0 && pct_after == 0 && defender->base.kind == THING_PC) {
        combat_sever_limb(attacker, defender, limb, verb);
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

/* Awards XP for `dmg` HP of damage `attacker` (a PC) just landed on
 * `victim`, proportional to victim's max_hp -- user, 2026-08-03: "we want
 * xp gain calculated per hit, not at the end of a fight" (the OLD design
 * granted the whole reward in one lump at combat_defeat(), so a mid-fight
 * disconnect/crash lost every bit of XP earned that fight, and a group
 * member who would have leveled up mid-fight got no benefit -- max_hp
 * recompute, full heal, practice points -- until the kill actually
 * landed). Same total-reward formula and same level-weighted group split
 * as the old one-shot version (victim_level*50), just credited AS THE
 * DAMAGE LANDS: each hit's share is (dmg / victim->progress.max_hp) of
 * the full reward, so the running total naturally approaches the full
 * amount as the victim's HP is whittled down (a killing blow that
 * overshoots remaining HP slightly overshoots the total too -- an
 * accepted, minor imprecision, not worth a second clamping pass).
 * Deliberately SILENT per hit (no "you gain N experience" message here --
 * combat_defeat() prints ONE summary line for the whole fight instead,
 * reading from/zeroing being_t.xp_gained_this_fight) -- otherwise a
 * multi-round fight would spam a gain message every single swing. */
static void combat_award_hit_xp(being_t *attacker, being_t *victim, int dmg) {
    if (attacker->base.kind != THING_PC || being_is_immortal(attacker) || dmg <= 0)
        return;
    if (victim->progress.max_hp <= 0)
        return;
    /* PK is XP-neutral for both sides (user, 2026-08-04) -- a PC attacker
     * landing hits on another PC earns nothing, matching the loss side's
     * own PC-vs-PC skip in combat_defeat() below. */
    if (victim->base.kind == THING_PC)
        return;

    long total_xp = (long)(victim->progress.level > 0 ? victim->progress.level : 1) * 50;

    being_t *recipients[GROUP_MAX_FOLLOWERS + 1];
    int n = group_recipients(attacker, attacker->base.roomp, recipients, GROUP_MAX_FOLLOWERS + 1);
    long total_weight = 0;
    for (int i = 0; i < n; i++)
        total_weight += recipients[i]->progress.level > 0 ? recipients[i]->progress.level : 1;

    for (int i = 0; i < n; i++) {
        being_t *m = recipients[i];
        long weight = m->progress.level > 0 ? m->progress.level : 1;
        long full_share = (n == 1) ? total_xp : (total_xp * weight) / total_weight;
        long hit_share = (full_share * dmg) / victim->progress.max_hp;

        /* Trophy decay (TODO.md, user: "implement trophy system from
         * sneezy") -- repeat-killing the same mob vnum shrinks its own
         * XP worth, down to a floor, nudging players toward variety.
         * Ported from Sneezy's TTrophy::getExpModVal(), applied here
         * (not in a separate pass) so it naturally scales with the same
         * per-hit crediting combat_award_hit_xp() already does. Reads
         * `m`'s own trophy count, not the group's -- each recipient's
         * modifier is theirs alone, same as Sneezy's per-TBeing
         * TTrophy instance. */
        double trophy_count = 0.0;
        trophy_repo_get_count(m->player_id, victim->base.id, &trophy_count);
        hit_share = (long)(hit_share * trophy_exp_mod((int)victim->base.id, trophy_count));
        if (hit_share < 1)
            hit_share = 1;

        int levels_gained = progress_add_xp(&m->progress, hit_share);
        m->xp_gained_this_fight += hit_share;
        if (levels_gained > 0) {
            /* Same level-up payoff the old one-shot version gave (see its
             * own removed comment in combat_defeat() for the max_hp-recompute
             * rationale) -- now lands the moment the threshold is actually
             * crossed, mid-fight, not stalled until the kill. */
            m->progress.max_hp = being_calc_max_hp(m);
            m->progress.hp = m->progress.max_hp;
            m->progress.max_vit = being_calc_max_vit(m);
            m->progress.vit = m->progress.max_vit;
            being_limbs_full_heal(m);
            tell(m, "You feel more experienced!\r\n");
            int pp = 0;
            for (int j = 0; j < levels_gained; j++)
                pp += practice_points_for_level(m);
            m->progress.practice_points += pp;
            tell(m, "<g>You gain %d practice point%s.<z>\r\n", pp, pp == 1 ? "" : "s");
            /* Save right away only for the (rare) level-up case -- this
             * box is memory-constrained (445MB total, saw MariaDB OOM-killed
             * live during this session's own testing), so a DB write on
             * EVERY landed hit for every group member is real, avoidable
             * load. The two direct combatants already get saved every
             * ROUND regardless (combat_process_run()'s own mid-fight-
             * persistence save, below) -- that's enough durability for the
             * common case; a level-up's max_hp/practice-point payoff is the
             * one thing worth an extra write for immediately. */
            player_progress_save(m->player_id, &m->progress);
        }
    }
}

/* Ends a fight once `loser`'s HP hits 0 (or they're decapitated) --
 * see the doc comment above (starting "No permadeath for a PC...") for
 * the full rationale on PC vs. mob outcomes, XP/gold transfer, and the
 * possessed/polymorphed-body special case handled first below. `slain`
 * only picks which message flavor is shown; both paths end the fight
 * the same way. */
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

    /* ARENA knockout (room-flag effects port): a room flagged
     * ROOM_FLAG_ARENA is a sporting ring, not a killing field -- upstream
     * (misc/combat.cc) gates the whole death pipeline on !ROOM_ARENA: no
     * exp/talens loss, no equipment damage, and the "death" is bookkept as
     * an arena loss rather than a real one. Ported here as a clean
     * non-lethal knockout for a defeated PC: patched to half HP, limbs
     * healed, left STANDING in the ring -- no XP loss, no corpse, no gear
     * drop, no menu eject, no world death-taunt. Returns before all of
     * that. A MOB loser still dies normally (the arena courtesy is for
     * players); Tobin has no equipment-damage or PK-flag system to gate,
     * so those two upstream arena rules are already inert here. 21 live
     * rooms carry this bit. */
    if (loser->base.kind == THING_PC && loser->base.roomp
        && (loser->base.roomp->room_flag & ROOM_FLAG_ARENA)) {
        loser->progress.hp = loser->progress.max_hp / 2;
        if (loser->progress.hp < 1)
            loser->progress.hp = 1;
        being_limbs_full_heal(loser);
        loser->position = POSITION_STANDING;
        tell(loser, "<y>You are knocked senseless -- but this is the arena, so you'll live.<z>\r\n");
        tell(winner, "You have bested %s in the arena!\r\n", being_display_name(loser));
        {
            char ring[160];
            snprintf(ring, sizeof(ring),
                     "<y>%s is knocked out of the bout by %s!<z>\r\n",
                     being_display_name(loser), being_display_name(winner));
            if (loser->base.roomp)
                for (descriptor_t *it = g_descriptors; it; it = it->next)
                    if (it->character && it->character->base.roomp == loser->base.roomp
                        && it->character != loser && it->character != winner)
                        descriptor_notify(it, ring);
        }
        log_info("%s was bested by %s in the arena. [%s]",
                 being_display_name(loser), being_display_name(winner),
                 loser->desc ? descriptor_display_host(loser->desc) : "?");
        if (loser->player_id > 0)
            player_progress_save(loser->player_id, &loser->progress);
        return;
    }

    bool loser_is_pc = (loser->base.kind == THING_PC);
    /* Gold-to-corpse (user 2026-07-28: "gold should be with the corpse,
     * autoloot or player loot should take care of it" -- reverting both
     * gold-drop paths below away from a direct wallet-to-wallet credit).
     * Computed here, actually turned into a real OBJ_CAT_MONEY object
     * once the corpse itself exists further down. No more group-split
     * on the PvP path (see that block's own comment) -- physical loot
     * is inherently first-whoever-loots-it now, same as any other item
     * in the corpse, not a stat auto-distributed to a group. */
    int corpse_gold = 0;

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
         * toward the next one. PvP (a PC winner) is now XP-neutral --
         * user, 2026-08-04: "Player PK should neither gain nor lose
         * experience" -- superseding the old /10-reduced-penalty
         * behavior; a MOB winner (the ordinary "died to a monster" case)
         * still gets the full penalty. Immortals never lose XP (they're
         * already past the mortal ladder, same "immortals don't need XP"
         * precedent as the winner-XP block below). */
        if (!being_is_immortal(loser) && loser->progress.experience > 0
            && winner->base.kind != THING_PC) {
            long base_loss = loser->progress.experience / 5;
            long level_floor = progress_xp_for_level(loser->progress.level);
            long max_loss = loser->progress.experience - level_floor;
            if (max_loss < 0)
                max_loss = 0;
            long xp_loss = base_loss < max_loss ? base_loss : max_loss;
            if (xp_loss > 0) {
                loser->progress.experience -= xp_loss;
                tell(loser, "You lose %ld experience point%s.\r\n", xp_loss, xp_loss == 1 ? "" : "s");
            }
        }

        /* Gold-on-a-PvP-kill (TODO.md, user: "also upon death get all
         * gold from the victim..."; reworked 2026-07-28 per the user's
         * follow-up -- gold now goes into the corpse as a real lootable
         * money-pile object, same as any other item, instead of
         * transferring straight into the killer's wallet). PC-vs-PC
         * only (a mob loser's gold-drop is the separate path below);
         * same non-immortal-winner gate as that path. PK combat itself
         * already requires both sides to have opted in (`toggle pk`),
         * so this can only ever fire with mutual consent. */
        if (winner->base.kind == THING_PC && !being_is_immortal(winner)
            && loser->progress.gold > 0) {
            corpse_gold = loser->progress.gold;
            loser->progress.gold = 0;
        }

        player_progress_save(loser->player_id, &loser->progress);
    }

    /* Gold drop (Money system, user 2026-07-17: "implement money and
     * shops"; reworked 2026-07-28 per the user's follow-up -- a mob
     * loser's gold now goes into the corpse as a real OBJ_CAT_MONEY
     * object, same as the PvP path above, instead of a direct wallet
     * credit). Same PC-winner-only, non-immortal gate as the XP award
     * below; PCs never drop gold on death (no real PK economy to
     * protect yet). A mundane animal-race mob (RODENT, FELINE, BEAR,
     * DEER, BIRD, ...) never drops gold at all -- user 2026-07-19:
     * "animal races should not have wealth, that doesnt make sense" --
     * see mob_race_is_animal() (being.c). XP is unaffected; only the
     * gold drop is gated. Computed here (not down by the corpse-
     * population block) because that block already checks `corpse_gold`
     * to build the coin-pile object -- computing it any later left mob
     * kills with an empty corpse every time (found live-testing
     * 2026-07-28: a mob configured with gold never actually dropped
     * any). */
    if (!loser_is_pc && winner->base.kind == THING_PC && !being_is_immortal(winner)
        && !mob_race_is_animal(loser->mob_race)) {
        int mob_level = loser->progress.level > 0 ? loser->progress.level : 1;
        corpse_gold = mob_level * (1 + rand() % 5);
    }

    if (!loser_is_pc)
        trophy_record_kill(winner, (int)loser->base.id);

    if (slain) {
        tell(winner, "You have slain %s!\r\n", being_display_name(loser));
        tell(loser, "You have been slain by %s!\r\nYou are <r>DEAD<z>!\r\n", being_display_name(winner));
    } else {
        tell(winner, "You have defeated %s!\r\n", being_display_name(loser));
        tell(loser, "You have been defeated by %s!\r\nYou are <r>DEAD<z>!\r\n", being_display_name(winner));
    }

    /* XP on kill -- user, 2026-08-03: "we want xp gain calculated per hit,
     * not at the end of a fight" (so a mid-fight disconnect/crash doesn't
     * erase XP already earned, and a leveling group member gets their
     * max_hp/practice-point payoff as it happens, not stalled until the
     * kill). The actual granting now happens per landed hit
     * (combat_award_hit_xp() below, called from both combat_strike()'s
     * melee path and combat_apply_skill_damage()) -- this block just
     * reports each recipient's own accumulated total as ONE summary line
     * and zeroes the accumulator back out. Immortals never accumulate
     * anything here (combat_award_hit_xp() already gates on that), so
     * nothing prints for an immortal winner. */
    if (winner->base.kind == THING_PC && !being_is_immortal(winner)) {
        being_t *recipients[GROUP_MAX_FOLLOWERS + 1];
        int n = group_recipients(winner, winner->base.roomp, recipients, GROUP_MAX_FOLLOWERS + 1);

        for (int i = 0; i < n; i++) {
            being_t *m = recipients[i];
            if (m->xp_gained_this_fight > 0) {
                tell(m, "You gain a total of %ld experience from that fight.\r\n",
                     m->xp_gained_this_fight);
                m->xp_gained_this_fight = 0;
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

        /* Gold-to-corpse (see corpse_gold's own declaration comment
         * above): a real OBJ_CAT_MONEY object, same category/pick-up
         * mechanic cmd_object.c's pick_up_money() already gives the
         * seeded "pile of gold" treasure props -- picking it up (by
         * hand, or via the autoloot pass just below) credits the
         * wallet and destroys the object, same as any other money pile. */
        if (corpse_gold > 0 && corpse) {
            obj_t *coins = obj_create_ephemeral("gold coins pile",
                "a pile of gold coins", "A pile of gold coins is lying here.",
                OBJ_CAT_MONEY);
            if (coins) {
                coins->val[0] = corpse_gold;
                thing_move_to(&coins->base, &corpse->base);
            }
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
            /* User 2026-08-03: "when looting a mob the game should report
             * any inventory changes" -- autoloot used to print one generic
             * "You automatically loot X's corpse." line with no breakdown
             * of what was actually gained. Now reports each item by name
             * and any gold as its own line, same "You get/find ..." shape
             * cmd_object.c's manual get_all_from_room()/`get all <corpse>`
             * already uses, just worded for the automatic case. */
            bool looted_any = false;
            int looted_gold = 0;
            thing_t *ct = corpse->base.stuff_head;
            while (ct) {
                thing_t *cnext = ct->stuff_next;
                obj_t *cobj = (obj_t *)ct;
                if (ct->kind == THING_OBJ && cobj->category == OBJ_CAT_MONEY) {
                    /* Same credit-and-destroy pick_up_money() (cmd_object.c)
                     * does for a manual `get` -- autoloot doesn't route
                     * through that function, so the money-object special
                     * case is duplicated here. */
                    looted_gold += cobj->val[0];
                    winner->progress.gold += cobj->val[0];
                    obj_destroy(cobj);
                } else {
                    const char *label = cobj->base.short_descr[0] ? cobj->base.short_descr : cobj->base.name;
                    tell(winner, "You loot %s from %s's corpse.\r\n", label, being_display_name(loser));
                    thing_move_to(ct, &winner->base);
                }
                looted_any = true;
                ct = cnext;
            }
            if (looted_gold > 0)
                tell(winner, "You loot %d gold from %s's corpse.\r\n", looted_gold, being_display_name(loser));
            if (looted_any) {
                player_inventory_save(winner->player_id, winner);
                player_progress_save(winner->player_id, &winner->progress);
            }
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

/* See combat.h's doc comment. */
void combat_egotrip_damn(being_t *immortal, being_t *target) {
    if (!target || !target->base.roomp)
        return;
    target->progress.hp = 0;
    combat_defeat(target, immortal, true);
}

/* Applies `dmg` from a skill/spell (as opposed to an ordinary melee
 * strike) to `defender`'s `limb`, zeroing it for an immortal target,
 * and triggers combat_defeat() if it drops them to 0 HP -- returns true
 * if the defender died so the caller can stop processing further
 * effects against a now-gone target. */
bool combat_apply_skill_damage(being_t *attacker, being_t *defender, int dmg, limb_t limb) {
    if (being_is_immortal(defender))
        dmg = 0;
    /* Same limb-damage-generation-rate halving as combat_strike()'s own
     * melee path (TODO.md priority item, user 2026-07-30) -- overall HP
     * still takes the full `dmg`, only the specific limb's own share is
     * halved, kept consistent regardless of whether the damage came from
     * an ordinary swing or a skill. */
    defender->progress.hp -= dmg;
    combat_award_hit_xp(attacker, defender, dmg);
    /* GMCP/MSDP push + MSP hit sound (TobinMUD Client project,
     * 2026-08-05) -- no-op for a mob defender or a descriptor that
     * never opted into GMCP/MSDP/MSP (being_notify_vitals_changed()/
     * descriptor_send_msp_sound() both check that internally). */
    being_notify_vitals_changed(defender);
    if (defender->desc)
        descriptor_send_msp_sound(defender->desc, pick_hit_sound(attacker, NULL), 100);
    being_hurt_limb_only(defender, limb, (dmg + 1) / 2);
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

/* Fatal fall (Sneezy → Tobin feature audit, "catfall/catleap"): called
 * from fall.c's own checkFalling()-equivalent once a fall's own damage
 * roll is unsurvivable (mirrors the real upstream's fallKill()). An
 * environmental death, same "no winner" shape as combat_drown_pc()
 * just above -- duplicates its PC-relevant slice rather than reusing
 * combat_defeat() for the exact same reason that function's own doc
 * comment already gives. */
void combat_fall_kill_pc(being_t *victim) {
    if (!victim || victim->base.kind != THING_PC)
        return;

    victim->fighting = NULL;
    victim->progress.hp = victim->progress.max_hp / 2;
    if (victim->progress.hp < 1)
        victim->progress.hp = 1;
    being_limbs_full_heal(victim);

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

    tell(victim, "The ground rushes up to meet you -- everything goes dark. You have fallen to your death!\r\n");

    room_t *scene = victim->base.roomp;
    if (scene) {
        char namebuf[64], msg[128];
        being_display_name_cap(victim, namebuf, sizeof(namebuf));
        snprintf(msg, sizeof(msg), "%s slams into the ground and lies still.\r\n", namebuf);
        descriptor_room_echo(scene, victim, msg);

        char short_descr[128], long_descr[200];
        snprintf(short_descr, sizeof(short_descr), "the corpse of %s", being_display_name(victim));
        snprintf(long_descr, sizeof(long_descr), "The broken corpse of %s lies here.", being_display_name(victim));
        obj_t *corpse = obj_create_ephemeral("corpse", short_descr, long_descr, OBJ_CAT_CONTAINER);
        if (corpse) {
            corpse->wear_flag = 0;
            corpse->val[0] = 0;
            corpse->val[1] = 0;
            corpse->val[2] = victim->mob_race;
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

    log_info("%s has fallen to death. [%s]", being_display_name(victim),
             victim->desc ? descriptor_display_host(victim->desc) : "?");

    player_progress_save(victim->player_id, &victim->progress);
    if (victim->desc)
        descriptor_leave_to_menu(victim->desc);
}

/* Death-trap room (user 2026-08-16, "room flags ... intended effects from
 * sneezy"): a mortal who steps into a ROOM_FLAG_DEATH room is slain on the
 * spot -- real upstream ROOM_DEATH. Called from cmd_move.c's do_move()
 * right after the victim enters such a room. Same "environmental death, no
 * winner" shape as combat_drown_pc()/combat_fall_kill_pc() above (a third
 * sibling, deliberately duplicating that PC-relevant slice with its own
 * flavor rather than reusing combat_defeat(), same as they do). No-op for
 * anything but a PC; the caller already excludes immortals. */
void combat_death_room_kill_pc(being_t *victim) {
    if (!victim || victim->base.kind != THING_PC)
        return;

    victim->fighting = NULL;
    victim->progress.hp = victim->progress.max_hp / 2;
    if (victim->progress.hp < 1)
        victim->progress.hp = 1;
    being_limbs_full_heal(victim);

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

    tell(victim, "The unseen hazards of this place close in around you -- everything goes black. You have DIED!\r\n");

    room_t *scene = victim->base.roomp;
    if (scene) {
        char namebuf[64], msg[128];
        being_display_name_cap(victim, namebuf, sizeof(namebuf));
        snprintf(msg, sizeof(msg), "%s staggers, then crumples to the ground, lifeless.\r\n", namebuf);
        descriptor_room_echo(scene, victim, msg);

        char short_descr[128], long_descr[200];
        snprintf(short_descr, sizeof(short_descr), "the corpse of %s", being_display_name(victim));
        snprintf(long_descr, sizeof(long_descr), "The lifeless corpse of %s lies crumpled here.", being_display_name(victim));
        obj_t *corpse = obj_create_ephemeral("corpse", short_descr, long_descr, OBJ_CAT_CONTAINER);
        if (corpse) {
            corpse->wear_flag = 0;
            corpse->val[0] = 0;
            corpse->val[1] = 0;
            corpse->val[2] = victim->mob_race;
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

    log_info("%s has died to a death-trap room. [%s]", being_display_name(victim),
             victim->desc ? descriptor_display_host(victim->desc) : "?");

    player_progress_save(victim->player_id, &victim->progress);
    if (victim->desc)
        descriptor_leave_to_menu(victim->desc);
}

/* SPEC_TUSK_GORING (spec_mobs_goring.cc's `tuskGoring`) -- id 153, the
 * first proc ported under the new per-round mob combat-action hook
 * above (SPEC_PROCS.md previously logged this hook as entirely missing,
 * blocking both this proc and SPEC_HORSE=16's kick). On its own swing
 * each round, a goring mob (boar/tusker) has a 1-in-8 chance (upstream
 * `::number(0, 7)`, kept verbatim) to attempt a gore against whoever
 * it's fighting, requiring the victim standing and not mounted -- an
 * 80% chance to land (upstream's `!isAgile(0) && ::number(0, 4)`
 * simplified to a flat chance; Tobin has no agility-check primitive to
 * hang the original's exact condition on, a disclosed scope note). A
 * successful gore deals a real damage roll and knocks the victim to
 * POSITION_SITTING; a failed attempt is flavor-only (a dodge message,
 * no damage). */
#define SPEC_TUSK_GORING 153

static bool mob_spec_tusk_goring_combat(being_t *m, being_t *victim) {
    if (victim->position != POSITION_STANDING || victim->mount)
        return false;
    if (rand() % 8 != 0)
        return false;

    char capbuf[128];
    if (rand() % 5 == 0) {
        char msg[192];
        snprintf(msg, sizeof(msg), "%s charges towards you, but you easily dodge them.\r\n",
                 being_display_name_cap(m, capbuf, sizeof(capbuf)));
        tell(victim, "%s", msg);
        if (victim->base.roomp)
            descriptor_room_echo(victim->base.roomp, victim, msg);
        return false;
    }

    int dmg = (m->progress.level * 5 + (rand() % 21 - 10)) / 2;
    if (dmg < 10)
        dmg = 10;

    tell(victim, "%s barrels down on you, impaling you painfully! You are knocked to the ground.\r\n",
         being_display_name_cap(m, capbuf, sizeof(capbuf)));
    if (victim->base.roomp) {
        char msg[192];
        snprintf(msg, sizeof(msg), "%s charges into %s, impaling them fiercely!\r\n",
                 being_display_name_cap(m, capbuf, sizeof(capbuf)), being_display_name(victim));
        descriptor_room_echo(victim->base.roomp, victim, msg);
    }

    bool defeated = combat_apply_skill_damage(m, victim, dmg, LIMB_BODY);
    if (!defeated)
        victim->position = POSITION_SITTING;
    return defeated;
}

/* Caster-mob per-round combat spellcasting (user 2026-08-16: "Mage and
 * druid mobs arent casting, mages druids and clerics should be taking
 * advantage of their spells ... fix it so mobs use skills/spells
 * available to them" -- the casters-first slice of that item). A
 * Mage/Druid/Cleric mob now has a per-round chance to throw a real class
 * spell at whoever it's fighting, ON TOP OF its normal melee swing (the
 * same additive per-round-action shape as the SPEC_TUSK_GORING proc
 * above), instead of only ever punching.
 *
 * The spell is a genuine roster entry the mob's class + level actually
 * qualifies for (skill_find()/min_level gate), and its damage comes from
 * the same spell_damage_for_level() formula a PC casting that spell uses
 * (a local copy of cmd_cast.c's helper, following this codebase's
 * small-static-helper duplication convention -- see cmd_use.c's own copy
 * and its note) -- so a caster mob hits as hard as an equivalent player.
 * A wounded
 * Cleric mob (below half HP, and high enough level to actually know
 * `heal`) mends itself instead of attacking -- the one healer behavior
 * that makes a cleric mob feel different from a mage.
 *
 * Deliberately scoped for this first pass: only fires for a mob fighting
 * a PC (combat_process_run()'s PC-centric walk calls this with the PC as
 * `victim`); no component/mana gate (mobs have never paid those for their
 * innate attacks); no multi-round cast delay (a mob's spell lands the
 * round it's rolled, like every other mob combat action); and no
 * per-spell affect fidelity (blind/curse/slow/etc.) -- a caster mob's
 * spells resolve as their damage/heal CORE only. Non-caster mobs return
 * immediately, so the per-round cost for everyone else is a single class
 * check. */
/* Local copy of cmd_cast.c's per-spell damage formula (this codebase
 * duplicates this tiny helper per file rather than sharing it -- see
 * cmd_use.c's identical copy and its comment). Scaled by the spell's own
 * min_level so a caster mob's spell hits exactly as hard as a PC's. */
static int spell_damage_for_level(int min_level) {
    return 4 + min_level + (rand() % (min_level / 3 + 4));
}

static const char *const MAGE_MOB_SPELLS[] = {
    "gust", "flare", "mystic darts", "fireball", "ice storm",
    "lightning bolt", "chain lightning",
};
static const char *const DRUID_MOB_SPELLS[] = {
    "harm light", "thorn barrage", "storm call", "feral wrath",
    "nature's wrath",
};
static const char *const CLERIC_MOB_SPELLS[] = {
    "harm light", "harm serious", "harm critical", "harm", "call lightning",
};

/* Debuff spells a caster mob can inflict for per-spell affect fidelity
 * (2026-08-17, TODO "caster-mob per-spell affect fidelity"). Before this,
 * mob_cast_combat() below resolved every spell as its damage/heal CORE
 * only -- a Cleric mob's `curse`/`blindness`, a Mage mob's `fear`/`faerie
 * fog`/`silence` all just landed as generic elemental damage. Each entry
 * names a REAL roster spell (skill_find() gates it by the mob's level) and
 * the affect its PC-side cast applies (cmd_pray.c/cmd_cast.c) at the same
 * magnitude/duration -- so a mob's curse is the same DEX debuff a Cleric
 * PC's curse is. Druid mobs stay damage-only (upstream Druid combat is
 * nature-damage, not afflictions -- no clean debuff to mirror). */
typedef enum {
    MOB_DBF_BLIND,   /* AFFECT_BLIND   -- to-hit penalty + can't see the room */
    MOB_DBF_CURSE,   /* AFFECT_CURSE   -- level-scaled DEX penalty */
    MOB_DBF_FEAR,    /* AFFECT_FEAR    -- can't swing back while afraid */
    MOB_DBF_SILENCE, /* AFFECT_SILENCE -- can't cast/pray */
} mob_debuff_kind_t;

typedef struct {
    const char *name;   /* roster spell name, exact */
    mob_debuff_kind_t kind;
} mob_debuff_t;

static const mob_debuff_t MAGE_MOB_DEBUFFS[] = {
    { "fear", MOB_DBF_FEAR },
    { "faerie fog", MOB_DBF_BLIND },
    { "silence", MOB_DBF_SILENCE },
};
static const mob_debuff_t CLERIC_MOB_DEBUFFS[] = {
    { "curse", MOB_DBF_CURSE },
    { "blindness", MOB_DBF_BLIND },
};

/* The AFFECT_* a debuff kind applies -- used to skip a victim who already
 * carries it (no wasted action / doubled message). */
static affect_type_t mob_debuff_affect(mob_debuff_kind_t k) {
    switch (k) {
        case MOB_DBF_BLIND:   return AFFECT_BLIND;
        case MOB_DBF_CURSE:   return AFFECT_CURSE;
        case MOB_DBF_FEAR:    return AFFECT_FEAR;
        case MOB_DBF_SILENCE: return AFFECT_SILENCE;
    }
    return AFFECT_BLIND;
}

/* Applies debuff `db` from caster mob `m` onto `victim`, mirroring the PC
 * cast's own affect + magnitude (cmd_pray.c curse/blindness, cmd_cast.c
 * fear/silence; faerie fog blinds only the victim here, not the whole
 * room, so a mob never blinds its own allies). Announces to victim + room.
 * Unlike the PC `fear` cast this does NOT force an immediate flee:
 * mob_cast_combat() runs inside combat_process_run()'s fighter loop, and
 * moving the PC to another room mid-iteration would mutate the very
 * fighting pointers that loop is walking -- the affect alone still stops
 * the feared victim swinging back (cmd_attack.c). */
static void mob_apply_debuff(being_t *m, being_t *victim, const mob_debuff_t *db,
                             int min_level) {
    char capbuf[128];
    const char *vseen = "";
    const char *rseen = "";
    switch (db->kind) {
        case MOB_DBF_BLIND:
            being_apply_affect(victim, AFFECT_BLIND, 20 + min_level);
            vseen = "and your eyes go white and unseeing";
            rseen = "and their eyes go white and unseeing";
            break;
        case MOB_DBF_CURSE: {
            int penalty = min_level / 3;
            if (penalty < 1) penalty = 1;
            if (penalty > 5) penalty = 5;
            being_apply_stat_affect(victim, AFFECT_CURSE, 100, -penalty);
            vseen = "and a dark aura settles over you";
            rseen = "and a dark aura settles over them";
            break;
        }
        case MOB_DBF_FEAR:
            being_apply_affect(victim, AFFECT_FEAR, 20 + min_level);
            vseen = "and terror grips you -- you dare not strike back";
            rseen = "and terror grips them";
            break;
        case MOB_DBF_SILENCE:
            being_apply_affect(victim, AFFECT_SILENCE, 30 * COMBAT_ROUND_PULSES);
            vseen = "and your voice dies in your throat";
            rseen = "and their voice dies in their throat";
            break;
    }
    if (victim->desc) {
        char vmsg[256];
        snprintf(vmsg, sizeof(vmsg), "<o>%s intones %s at you, %s!<z>\r\n",
                 being_display_name_cap(m, capbuf, sizeof(capbuf)), db->name, vseen);
        descriptor_notify(victim->desc, vmsg);
    }
    if (m->base.roomp) {
        char rmsg[256];
        snprintf(rmsg, sizeof(rmsg), "%s intones %s at %s, %s!\r\n",
                 being_display_name_cap(m, capbuf, sizeof(capbuf)), db->name,
                 being_display_name(victim), rseen);
        descriptor_room_echo(m->base.roomp, victim, rmsg);
    }
}

static bool mob_cast_combat(being_t *m, being_t *victim) {
    player_class_t cls = m->char_class;
    if (cls != CLASS_MAGE && cls != CLASS_DRUID && cls != CLASS_CLERIC)
        return false;
    /* ~1-in-3 rounds the mob reaches for a spell; the rest of the time it
     * just fights normally (its melee already resolved this round before
     * this hook ran). */
    if (rand() % 3 != 0)
        return false;

    char capbuf[128];

    /* Wounded Cleric mob mends itself -- only once high enough level to
     * know `heal` (keeping the named spell honest), same being_heal() the
     * PC heal branch uses. */
    if (cls == CLASS_CLERIC && m->progress.max_hp > 0
        && m->progress.hp * 2 < m->progress.max_hp
        && m->progress.level >= 17) {
        int amt = spell_damage_for_level(17) + m->progress.level;
        being_heal(m, amt);
        /* Exclude the VICTIM from the room echo, not the (non-PC) mob:
         * the victim already gets its own copy via descriptor_notify()
         * just below, so echoing to the mob's exclusion (a no-op, mobs
         * aren't PCs) would double the line for the PC being fought. */
        if (m->base.roomp) {
            char rmsg[224];
            snprintf(rmsg, sizeof(rmsg),
                     "%s intones a healing prayer, and %s wounds knit closed.\r\n",
                     being_display_name_cap(m, capbuf, sizeof(capbuf)), gender_possess(m->gender));
            descriptor_room_echo(m->base.roomp, victim, rmsg);
        }
        if (victim->desc) {
            char vmsg[224];
            snprintf(vmsg, sizeof(vmsg),
                     "%s intones a healing prayer, and %s wounds knit closed.\r\n",
                     being_display_name_cap(m, capbuf, sizeof(capbuf)), gender_possess(m->gender));
            descriptor_notify(victim->desc, vmsg);
        }
        return false; /* healed, nobody defeated */
    }

    /* Per-spell affect fidelity (2026-08-17): some caster rounds inflict a
     * class debuff (blindness/curse/fear/silence) the mob knows, instead of
     * raw damage -- only if the victim isn't already carrying that affect.
     * Mage and Cleric mobs only; Druid mobs have no debuff table. */
    const mob_debuff_t *dbpool = NULL;
    int dbpool_n = 0;
    if (cls == CLASS_MAGE) {
        dbpool = MAGE_MOB_DEBUFFS;
        dbpool_n = (int)(sizeof(MAGE_MOB_DEBUFFS) / sizeof(MAGE_MOB_DEBUFFS[0]));
    } else if (cls == CLASS_CLERIC) {
        dbpool = CLERIC_MOB_DEBUFFS;
        dbpool_n = (int)(sizeof(CLERIC_MOB_DEBUFFS) / sizeof(CLERIC_MOB_DEBUFFS[0]));
    }
    if (dbpool) {
        const mob_debuff_t *dbq[8];
        int dbqn = 0;
        for (int i = 0; i < dbpool_n && dbqn < (int)(sizeof(dbq) / sizeof(dbq[0])); i++) {
            const skill_def_t *dsk = skill_find(cls, dbpool[i].name, false);
            if (dsk && m->progress.level >= dsk->min_level
                && !being_has_affect(victim, mob_debuff_affect(dbpool[i].kind)))
                dbq[dbqn++] = &dbpool[i];
        }
        /* ~45% of caster rounds with a fresh debuff available go to it;
         * the rest fall through to the damage-spell pick below. */
        if (dbqn > 0 && rand() % 100 < 45) {
            const mob_debuff_t *pick = dbq[rand() % dbqn];
            const skill_def_t *psk = skill_find(cls, pick->name, false);
            mob_apply_debuff(m, victim, pick, psk ? psk->min_level : m->progress.level);
            return false; /* debuff landed; nobody defeated this round */
        }
    }

    /* Pick a random offensive spell the mob's class + level qualifies for. */
    const char *const *pool;
    int pool_n;
    if (cls == CLASS_MAGE) {
        pool = MAGE_MOB_SPELLS;
        pool_n = (int)(sizeof(MAGE_MOB_SPELLS) / sizeof(MAGE_MOB_SPELLS[0]));
    } else if (cls == CLASS_DRUID) {
        pool = DRUID_MOB_SPELLS;
        pool_n = (int)(sizeof(DRUID_MOB_SPELLS) / sizeof(DRUID_MOB_SPELLS[0]));
    } else {
        pool = CLERIC_MOB_SPELLS;
        pool_n = (int)(sizeof(CLERIC_MOB_SPELLS) / sizeof(CLERIC_MOB_SPELLS[0]));
    }

    const skill_def_t *qualifying[16];
    int qn = 0;
    for (int i = 0; i < pool_n && qn < (int)(sizeof(qualifying) / sizeof(qualifying[0])); i++) {
        const skill_def_t *sk = skill_find(cls, pool[i], false);
        if (sk && m->progress.level >= sk->min_level)
            qualifying[qn++] = sk;
    }
    if (qn == 0)
        return false; /* too low-level for any of its class's spells -- just melee */

    const skill_def_t *sk = qualifying[rand() % qn];
    int dmg = spell_damage_for_level(sk->min_level);
    limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
    int limb_hp_before = victim->limbs[limb].hp;

    const char *verb = (cls == CLASS_MAGE) ? "hurls"
                     : (cls == CLASS_DRUID) ? "calls down"
                                            : "invokes";

    /* Announce before applying (same order as SPEC_TUSK_GORING above), so
     * the spell is named even on a killing blow; describe_dam() reads the
     * pre-hit limb HP captured just above. */
    if (victim->desc) {
        char vmsg[256];
        snprintf(vmsg, sizeof(vmsg), "%s %s %s at you, and it catches you %s!\r\n",
                 being_display_name_cap(m, capbuf, sizeof(capbuf)), verb, sk->name,
                 describe_dam(dmg, limb_hp_before, NULL));
        descriptor_notify(victim->desc, vmsg);
    }
    if (m->base.roomp) {
        char rmsg[256];
        snprintf(rmsg, sizeof(rmsg), "%s %s %s at %s!\r\n",
                 being_display_name_cap(m, capbuf, sizeof(capbuf)), verb, sk->name,
                 being_display_name(victim));
        /* Exclude the victim (already notified above), not the mob -- see
         * the cleric-heal branch for why echoing past the mob doubles it. */
        descriptor_room_echo(m->base.roomp, victim, rmsg);
    }

    return combat_apply_skill_damage(m, victim, dmg, limb);
}

/* Non-caster mob per-round combat SKILLS (user 2026-08-16, the second
 * half of "mobs use skills/spells available to them" -- the caster half
 * is mob_cast_combat() above). A Warrior/Thief/Monk mob now has a ~1-in-3
 * per-round chance to use one of its class's real combat maneuvers against
 * its PC opponent, on top of its melee, instead of only ever swinging --
 * the same additive per-round shape mob_cast_combat() and the spec-proc
 * hook use.
 *
 * Each maneuver reuses the EXACT effect core its PC command
 * (cmd_bash.c/cmd_kick.c/cmd_trip.c) applies -- the same str/dex-scaled
 * damage formula off the mob's own attrs (so a mob's bash hits as hard as
 * a PC's), the same POSITION_SITTING knockdown + one-round victim lag, and
 * the same spellcast_distract() nudge that a landed bash/kick/trip gives a
 * PC caster mid-spell -- rather than routing through the descriptor-bound
 * cmd_* handlers (mobs have no descriptor). Deliberately scoped like the
 * caster pass: no skill-roll miss branch (a mob's maneuver just lands, the
 * way its spec-proc and spell actions do), no disarm/object-manipulation
 * maneuver, and only fires for a mob fighting a PC. Non-Warrior/Thief/Monk
 * mobs return immediately. */
static bool mob_skill_combat(being_t *m, being_t *victim) {
    player_class_t cls = m->char_class;
    if (cls != CLASS_WARRIOR && cls != CLASS_THIEF && cls != CLASS_MONK)
        return false;
    if (rand() % 3 != 0)
        return false;

    int str_dmg = 1 + (m->attrs.strength - ATTR_BASE) / 4 + rand() % 4;
    if (str_dmg < 1) str_dmg = 1;
    int dex_dmg = 1 + (m->attrs.dexterity - ATTR_BASE) / 4 + rand() % 4;
    if (dex_dmg < 1) dex_dmg = 1;

    char capbuf[128];
    const char *self_room = NULL;  /* "%s <does something> to <victim>." */
    const char *at_you = NULL;     /* "%s <does something> to you." */
    int dmg = 0;
    bool knockdown = false;
    int distract = 0;

    if (cls == CLASS_WARRIOR) {
        /* bash (knockdown + str dmg), kick (str dmg), or trip (knockdown,
         * no dmg) -- the three low-level Warrior combat maneuvers, same
         * effects cmd_bash/cmd_kick/cmd_trip apply. */
        switch (rand() % 3) {
        case 0:
            self_room = "%s bashes into %s, knocking them to the ground!\r\n";
            at_you    = "%s bashes into you, knocking you to the ground!\r\n";
            dmg = str_dmg; knockdown = true; distract = 2;
            break;
        case 1:
            self_room = "%s boots %s in the head!\r\n";
            at_you    = "%s boots you in the head!\r\n";
            dmg = str_dmg; distract = 1;
            break;
        default:
            self_room = "%s sweeps %s's legs out, dropping them to the ground!\r\n";
            at_you    = "%s sweeps your legs out from under you!\r\n";
            dmg = 0; knockdown = true; distract = 1;
            break;
        }
    } else if (cls == CLASS_MONK) {
        /* Monks fight barehanded -- a dex-scaled kick, cmd_kick's own core. */
        self_room = "%s snaps a spinning kick into %s!\r\n";
        at_you    = "%s snaps a spinning kick into you!\r\n";
        dmg = dex_dmg; distract = 1;
    } else { /* CLASS_THIEF */
        /* A quick opportunistic knife strike -- heavier dex-scaled damage
         * than a plain kick, no knockdown (a thief stabs, it doesn't
         * grapple). */
        self_room = "%s slips in close and drives a blade into %s!\r\n";
        at_you    = "%s slips in close and drives a blade into you!\r\n";
        dmg = dex_dmg * 2; distract = 1;
    }

    /* Announce before applying (same order as the spec-proc/cast paths). */
    if (victim->desc) {
        char vmsg[256];
        snprintf(vmsg, sizeof(vmsg), at_you, being_display_name_cap(m, capbuf, sizeof(capbuf)));
        descriptor_notify(victim->desc, vmsg);
    }
    if (m->base.roomp) {
        char rmsg[256];
        snprintf(rmsg, sizeof(rmsg), self_room,
                 being_display_name_cap(m, capbuf, sizeof(capbuf)), being_display_name(victim));
        /* Exclude the victim (already notified via at_you above), not the
         * mob -- otherwise the PC being fought sees both lines (the reported
         * "sweeps your legs... / sweeps <you>'s legs..." double message). */
        descriptor_room_echo(m->base.roomp, victim, rmsg);
    }

    if (distract > 0)
        spellcast_distract(victim, distract); /* a landed maneuver rattles a PC mid-cast */

    if (dmg > 0) {
        bool defeated = combat_apply_skill_damage(m, victim, dmg, LIMB_BODY);
        if (defeated)
            return true;
    }
    if (knockdown && victim->position != POSITION_SITTING) {
        victim->position = POSITION_SITTING;
        being_set_wait(victim, COMBAT_ROUND_PULSES);
    }
    return false;
}

/* The mob-side per-round combat-action dispatch table -- id -> function,
 * same shape as mob_ai.c's own pulse dispatch table, just keyed off
 * combat.c's own CMD_MOB_COMBAT-equivalent hook instead. */
static bool mob_spec_dispatch_combat(being_t *m, being_t *victim) {
    switch (m->mob_spec_proc) {
    case SPEC_TUSK_GORING:
        return mob_spec_tusk_goring_combat(m, victim);
    default:
        return false;
    }
}

/* Runs on a timer (see main.c), once per combat round: resolves every
 * connected PC's ongoing fight (each pair only once per pulse, via
 * last_combat_pulse), mid-fight-persists both PCs' HP so a disconnect
 * mid-fight can't undo damage by reloading stale HP, then resolves
 * every charmed pet's own strike against whatever it's fighting (see
 * the large comment below for why pets are handled in a separate pass
 * here rather than through mob_ai's own slower pulse). */
/* Multi-attacker support (user 2026-08-16, "fight more than one mob" +
 * "assist friends"). Returns the first LIVE mob in `pc`'s room that is
 * engaging `pc` (its fighting pointer aims at `pc`) other than `except`,
 * or NULL. Tobin's combat is a strict 1v1 `fighting` pointer pair, so a
 * PC only ever swings at ONE foe; but several mobs can each aim their own
 * fighting pointer at the PC (set by combat_recruit_assist() below), and
 * this lets the PC re-engage a lingering attacker once its current foe
 * dies, and lets the extra-attacker pass find them. */
static being_t *combat_next_attacker(being_t *pc, being_t *except) {
    if (!pc || !pc->base.roomp)
        return NULL;
    for (thing_t *t = pc->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *m = (being_t *)t;
        if (m == except || m->fighting != pc)
            continue;
        if (m->progress.hp > 0 && m->position != POSITION_DEAD
            && m->position != POSITION_MORTALLYW && m->position != POSITION_INCAP)
            return m;
    }
    return NULL;
}

/* See combat.h. Cleanly ends every fight `b` is party to, in place: clears
 * b's own `fighting` pointer and scrubs the pointer of every OTHER being in
 * b's current room that was aiming at b (multi-attacker mobs, an assist
 * recruit, or the single foe of a strict 1v1). Called BEFORE a being is
 * relocated (goto_room()), while it's still standing among its attackers,
 * so no mob is left with a dangling `fighting` pointer at a PC that walked
 * away. */
void combat_break_fight(being_t *b) {
    if (!b)
        return;
    b->fighting = NULL;
    if (!b->base.roomp)
        return;
    for (thing_t *t = b->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue; /* objects et al. aren't beings -- don't reinterpret them */
        being_t *other = (being_t *)t;
        if (other != b && other->fighting == b)
            other->fighting = NULL;
    }
}

/* True for a "guard"-type mob, recognized by its keywords -- Tobin never
 * ported an ACT_ guardian flag, so the guard concept lives in the mob's
 * name/keyword list (e.g. "guard city", "castle guard"). Guards defend one
 * another unconditionally (see combat_recruit_assist()). */
static bool being_is_guard(const being_t *m) {
    const char *nm = m ? m->base.name : NULL;
    if (!nm)
        return false;
    for (const char *p = nm; *p; p++)
        if ((*p == 'g' || *p == 'G') && strncasecmp(p, "guard", 5) == 0)
            return true;
    return false;
}

/* Assist recruitment (user 2026-08-16: "Mobs should have a chance to join
 * the fights depending on alignment, assist friends" + "Guard mobs ...
 * assist each other. When you attack a guard you attack ALL guards"). When
 * a PC is fighting `victim_mob`, each un-engaged mob in the room that
 * qualifies leaps to its defense by aiming its OWN fighting pointer at the
 * PC (the PC's own fighting pointer is left alone -- the newcomers become
 * EXTRA attackers resolved by the multi-attacker pass in
 * combat_process_run(), not the PC's melee target). A charmed pet (the
 * PC's own ally) never assists against its master.
 *
 * Two ways to qualify, with different odds:
 *   - GUARDS: if both the newcomer and the mob under attack are guards
 *     (being_is_guard), the newcomer ALWAYS joins (user 2026-08-16:
 *     "guarenteed for guards") -- attack one guard and you take on every
 *     guard in the room, whatever their exact vnum.
 *   - Aligned ALLIES: a mob sharing the victim's non-zero alignment joins
 *     on a per-round RANDOM ROLL, never guaranteed (user 2026-08-16: "the
 *     assist shouldnt be 100% it should have a random chance of assist/
 *     fail"). A failed roll is simply re-rolled next round while the mob
 *     stays un-engaged, so an aligned brawl builds up unevenly. */
static void combat_recruit_assist(being_t *pc, being_t *victim_mob) {
    if (!pc || !pc->base.roomp || pc->progress.hp <= 0)
        return;
    if (victim_mob->base.kind != THING_MOB)
        return;
    for (thing_t *t = pc->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *f = (being_t *)t;
        if (f == victim_mob || f->fighting)
            continue;
        if (f->progress.hp <= 0 || f->position == POSITION_SLEEPING
            || f->position == POSITION_DEAD || f->position == POSITION_MORTALLYW
            || f->position == POSITION_INCAP || f->position == POSITION_STUNNED)
            continue;
        if (being_has_affect(f, AFFECT_CHARMED))
            continue; /* the PC's own summoned pet -- never turns on its master */
        bool both_guards = being_is_guard(f) && being_is_guard(victim_mob);
        bool ally = f->mob_align != 0 && f->mob_align == victim_mob->mob_align;
        if (!both_guards && !ally)
            continue;
        /* Guards defend their own for certain; an aligned ally only joins
         * on a successful per-round roll (re-rolled next round on a miss). */
        if (!both_guards && (rand() % 100) >= 35)
            continue;

        f->fighting = pc;   /* engage the PC as an EXTRA attacker; pc->fighting untouched */
        being_set_wait(f, COMBAT_ROUND_PULSES);
        char capf[128], capv[128];
        if (pc->desc) {
            char m[256];
            snprintf(m, sizeof(m), "<o>%s rushes to %s's aid and joins the attack on you!<z>\r\n",
                     being_display_name_cap(f, capf, sizeof(capf)), being_display_name(victim_mob));
            descriptor_notify(pc->desc, m);
        }
        if (pc->base.roomp) {
            char m[256];
            snprintf(m, sizeof(m), "%s rushes to %s's aid and joins the fight!\r\n",
                     being_display_name_cap(f, capf, sizeof(capf)),
                     being_display_name_cap(victim_mob, capv, sizeof(capv)));
            descriptor_room_echo(pc->base.roomp, pc, m);
        }
    }
}

void combat_process_run(long pulse_num) {
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *a = d->character;
        if (!a)
            continue;
        if (!a->fighting) {
            /* Multi-mob re-target (user 2026-08-16): the PC's foe died last
             * round but other mobs are still swinging at it -- re-engage
             * one so the fight continues instead of ending early. */
            being_t *nxt = combat_next_attacker(a, NULL);
            if (!nxt)
                continue;
            a->fighting = nxt;
        }
        if (a->last_combat_pulse == pulse_num)
            continue; /* already resolved this round via the other participant */

        being_t *b = a->fighting;

        /* Same-room guard (user 2026-08-16: "i used goto 43 to escape a
         * fight, and i kept fighting in a remote room"). A fighter can be
         * relocated mid-fight -- immortal `goto`, a teleport, any future
         * move path -- while its `fighting` pointer still aims at a foe now
         * in another room. Combat is strictly room-local, so end the fight
         * cleanly instead of resolving strikes across rooms. goto_room()
         * already breaks the fight proactively (combat_break_fight); this
         * is the catch-all for every other relocation. */
        if (!a->base.roomp || a->base.roomp != b->base.roomp) {
            a->fighting = NULL;
            if (b->fighting == a)
                b->fighting = NULL;
            continue;
        }

        a->last_combat_pulse = pulse_num;
        b->last_combat_pulse = pulse_num;

        /* Allies/kin of the mob under attack leap in as extra attackers
         * (see combat_recruit_assist()); they're resolved by the
         * multi-attacker pass further down this same round. */
        if (b->base.kind == THING_MOB)
            combat_recruit_assist(a, b);

        bool b_decapitated = combat_strike(a, b);
        if (b->progress.hp <= 0 || b_decapitated) {
            combat_defeat(b, a, b_decapitated);
            continue;
        }
        /* `haste` (AFFECT_HASTE, see affect.h's enum comment) -- a hasted
         * fighter gets one bonus combat_strike() immediately after their
         * normal one, same round. */
        if (being_has_affect(a, AFFECT_HASTE)) {
            b_decapitated = combat_strike(a, b);
            if (b->progress.hp <= 0 || b_decapitated) {
                combat_defeat(b, a, b_decapitated);
                continue;
            }
        }
        /* `chain attack`, `blur`, and `advanced kicking` (Monk, level 25,
         * level-25 audit batch: "A chance at a bonus follow-up strike
         * each round." / "A chance at an extra unarmed attack each round
         * while empty-handed." / "More of your unarmed strikes land as
         * kicks, boosting extra-attack odds."). Tobin's simplified model
         * treats all three the same way (no separate "combo counter",
         * "empty-handed stance", or "kick vs. punch" state to tell them
         * apart) -- a genuine bonus
         * combat_strike(), same mechanic haste's own AFFECT_HASTE uses
         * above, but CHANCE-gated per round (skill_roll_success()) and
         * BAREHANDED-only, rather than a timed affect, matching each
         * skill's own "a chance" wording instead of haste's guaranteed
         * "every round" bonus. */
        if (!combat_wielded_weapon(a)
            && (being_knows_skill(a, "chain attack") || being_knows_skill(a, "blur")
                || being_knows_skill(a, "advanced kicking"))
            && skill_roll_success(50)) {
            b_decapitated = combat_strike(a, b);
            if (b->progress.hp <= 0 || b_decapitated) {
                combat_defeat(b, a, b_decapitated);
                continue;
            }
        }

        /* `advanced berserking` (Warrior, level 35, missing-skill audit).
         * Real upstream (disc_warrior_brawling.cc's doAdvancedBerserk())
         * rolls each round to auto-fire one of several other known
         * Warrior maneuvers (slam/bash/headbutt/.../deathstroke) off a
         * weighted table. Tobin's simplified model reuses the exact same
         * "genuine bonus combat_strike(), CHANCE-gated per round" shape
         * the chain attack/blur/advanced kicking block above already
         * established for this class of skill -- no separate weighted
         * maneuver table or per-maneuver precondition plumbing needed --
         * but gated on AFFECT_BERSERK (only while actually berserking,
         * matching the real skill's "while raging" framing) and scaled by
         * proficiency (skill_learn_from_doing()) rather than a flat 50,
         * same proficiency-scaled shape riposte uses just above, since
         * this is an advanced-tier skill meant to reward practice. */
        if (being_has_affect(a, AFFECT_BERSERK) && being_knows_skill(a, "advanced berserking")) {
            const skill_def_t *adv_bers_sk = skill_find(a->char_class, "advanced berserking", false);
            if (adv_bers_sk && skill_roll_success(skill_learn_from_doing(a, adv_bers_sk))) {
                tell(a, "Your berserker rage lets you lash out again!\r\n");
                b_decapitated = combat_strike(a, b);
                if (b->progress.hp <= 0 || b_decapitated) {
                    combat_defeat(b, a, b_decapitated);
                    continue;
                }
            }
        }

        bool a_decapitated = combat_strike(b, a);
        if (a->progress.hp <= 0 || a_decapitated) {
            combat_defeat(a, b, a_decapitated);
            continue;
        }
        if (being_has_affect(b, AFFECT_HASTE)) {
            a_decapitated = combat_strike(b, a);
            if (a->progress.hp <= 0 || a_decapitated) {
                combat_defeat(a, b, a_decapitated);
                continue;
            }
        }
        /* `chain attack`/`blur`, `b`'s side -- see the identical block
         * above for `a`'s side. */
        if (!combat_wielded_weapon(b)
            && (being_knows_skill(b, "chain attack") || being_knows_skill(b, "blur")
                || being_knows_skill(b, "advanced kicking"))
            && skill_roll_success(50)) {
            a_decapitated = combat_strike(b, a);
            if (a->progress.hp <= 0 || a_decapitated) {
                combat_defeat(a, b, a_decapitated);
                continue;
            }
        }
        /* `advanced berserking`, `b`'s side -- see the identical block
         * above for `a`'s side. */
        if (being_has_affect(b, AFFECT_BERSERK) && being_knows_skill(b, "advanced berserking")) {
            const skill_def_t *adv_bers_sk_b = skill_find(b->char_class, "advanced berserking", false);
            if (adv_bers_sk_b && skill_roll_success(skill_learn_from_doing(b, adv_bers_sk_b))) {
                tell(b, "Your berserker rage lets you lash out again!\r\n");
                a_decapitated = combat_strike(b, a);
                if (a->progress.hp <= 0 || a_decapitated) {
                    combat_defeat(a, b, a_decapitated);
                    continue;
                }
            }
        }

        /* Mob-side per-round special combat action (SPEC_PROCS.md's own
         * "CMD_MOB_COMBAT hook Tobin has none of yet" blocker note --
         * this closes that gap, added here since combat.c is what owns
         * this per-round hook; a mob-specific dispatch table, same "id
         * -> function" shape as mob_ai.c's own pulse dispatch table).
         * Both sides are confirmed alive at this point (an earlier
         * defeat above would already have `continue`d). */
        if (b->base.kind == THING_MOB && b->mob_spec_proc) {
            if (mob_spec_dispatch_combat(b, a))
                continue;
        }

        /* Caster-mob per-round spellcasting (user 2026-08-16) -- a
         * Mage/Druid/Cleric mob throws a real class spell at its PC
         * opponent some rounds, on top of its melee. Returns true iff that
         * spell defeated `a` (combat_apply_skill_damage handles the kill),
         * so skip the rest of this round's bookkeeping just like the spec
         * hook above. A no-op for non-caster mobs. */
        if (b->base.kind == THING_MOB) {
            if (mob_cast_combat(b, a))
                continue;
        }

        /* Non-caster mob per-round combat skills (user 2026-08-16) -- a
         * Warrior/Thief/Monk mob uses a real class maneuver (bash/kick/
         * trip/stab) some rounds, on top of its melee. Same defeat/skip
         * contract as the hooks above; a no-op for caster mobs. */
        if (b->base.kind == THING_MOB) {
            if (mob_skill_combat(b, a))
                continue;
        }

        /* Multi-attacker pass (user 2026-08-16: "fight more than one mob"
         * + "assist friends"). Beyond `a`'s primary foe `b`, every OTHER
         * mob in the room whose fighting pointer aims at `a` (assist
         * recruits, or a leftover from a re-target) gets its own strike
         * plus its spec/spell/skill action against `a` this round. This is
         * the incoming side of multi-mob combat -- `a` still swings only at
         * `b` (the strict-1v1 offense), but can now be ganged up on. If any
         * extra attacker fells `a`, combat_defeat() ejects the PC (freeing
         * d->character) -- guarded right after, same as every defeat above.
         * `next` is saved before each iteration in case a strike relocates
         * or removes something from the room's contents list. */
        if (a->base.roomp) {
            for (thing_t *t = a->base.roomp->base.stuff_head; t;) {
                thing_t *next = t->stuff_next;
                if (t->kind == THING_MOB) {
                    being_t *e = (being_t *)t;
                    if (e != b && e->fighting == a && e->progress.hp > 0
                        && e->position != POSITION_DEAD && e->position != POSITION_MORTALLYW) {
                        bool a_dead = combat_strike(e, a);
                        if (a->progress.hp <= 0 || a_dead) {
                            combat_defeat(a, e, a_dead);
                            break;
                        }
                        if (e->mob_spec_proc && mob_spec_dispatch_combat(e, a))
                            break;
                        if (mob_cast_combat(e, a))
                            break;
                        if (mob_skill_combat(e, a))
                            break;
                    }
                }
                t = next;
            }
            if (!d->character)
                continue; /* a PC was felled by an extra attacker above */
        }

        /* Vitality drain (user 2026-08-03: "vitality should decrease when
         * fighting to about .75 of a point per round", then "drop the
         * vitality drain when fighting 20%" -- 0.75 * 0.8 = 0.6/round)
         * -- see being.h's vit_fatigue_accum doc comment for why this
         * accumulates a float and spends off whole points rather than
         * draining vit directly. Immortals are exempt, same as
         * movement's vit cost (being.h). */
        for (int i = 0; i < 2; i++) {
            being_t *fighter = i == 0 ? a : b;
            if (fighter->base.kind != THING_PC || being_is_immortal(fighter))
                continue;
            fighter->vit_fatigue_accum += 0.6f;
            while (fighter->vit_fatigue_accum >= 1.0f) {
                being_spend_vit(fighter, 1);
                fighter->vit_fatigue_accum -= 1.0f;
            }
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

/* Combat music (user, 2026-08-05). See combat.h's own comment on why
 * this is a periodic sweep rather than a hook at every `->fighting =
 * ...` site. Six real tracks the user supplied, picked at random per
 * fight (switch/case over a plain `rand() % N`, per the user's own
 * suggested shape) -- a plain array would work identically; kept as a
 * switch since that's what was asked for. */
void combat_music_tick(long pulse_num) {
    (void)pulse_num;
    static const char *TRACKS[] = {
        /* Renamed music1.wav-music5.wav (old adventure1/nastelborn/
         * motivational/atlasaudio/audiodollar-adventure) plus 4 real
         * new tracks (music6-9.wav), 2026-08-07 sound pack. */
        "music1.wav", "music2.wav", "music3.wav", "music4.wav",
        "music5.wav", "music6.wav", "music7.wav", "music8.wav", "music9.wav",
        "music10.wav", "music11.wav", "music12.wav", "music13.wav", "music14.wav",
        "music15.wav", "music16.wav", "music17.wav", "music18.wav", "music19.wav",
        "music20.wav", "music21.wav", "music22.wav", "music23.wav", "music24.wav",
        "music25.wav",
    };
    const int TRACK_COUNT = (int)(sizeof(TRACKS) / sizeof(TRACKS[0]));

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *ch = d->character;
        bool now_fighting = ch && ch->fighting;

        if (now_fighting && !d->music_playing) {
            /* Never repeat the same track twice in a row (user,
             * 2026-08-06: "rotate the music never repeating music twice
             * in a row") -- re-roll until it differs from last time,
             * skipped entirely on the very first pick (last_music_track
             * == -1). */
            int pick;
            do {
                pick = rand() % TRACK_COUNT;
            } while (pick == d->last_music_track);
            descriptor_send_msp_music(d, TRACKS[pick]);
            d->last_music_track = pick;
            d->music_playing = true;
        } else if (!now_fighting && d->music_playing) {
            descriptor_send_msp_music_off(d);
            d->music_playing = false;
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

/* Resolves a typed target name to a being in self's room, for `kill`/
 * `attack`-style commands -- handles the "N.name" ordinal prefix (see
 * the comment inside for why that skips the usual exact-match rule),
 * otherwise prefers an exact name match over the first prefix match in
 * room order. Skips anything combat_pk_allowed() rejects and any
 * linkdead PC (no descriptor), so both are simply untargetable rather
 * than needing a separate check at every call site. */
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
            /* `invisibility` (level-17 audit item) -- untargetable by
             * name for anyone but an immortal, same "can't be found
             * here at all" shape as the linkdead check just above. No
             * `detect invisibility` counter-check exists yet, so this
             * is unconditional for every mortal viewer. */
            if (being_has_affect((being_t *)t, AFFECT_INVISIBLE) && !being_is_immortal(self)
                && !being_has_affect(self, AFFECT_DETECT_INVISIBLE))
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
        /* `invisibility` (level-17 audit item) -- same gate as the
         * ordinal branch above. */
        if (being_has_affect((being_t *)t, AFFECT_INVISIBLE) && !being_is_immortal(self))
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

/* An immortal's instant kill (cmd_kill.c): zeroes target's HP and every
 * limb outright, then runs the normal combat_defeat() pipeline with
 * `slain` true -- bypasses combat_strike() entirely rather than
 * simulating a lucky hit, since an immortal's kill command is meant to
 * be unconditional. */
void combat_instakill(being_t *attacker, being_t *target) {
    if (!attacker || !target)
        return;

    target->progress.hp = 0;
    for (int i = 0; i < LIMB_COUNT; i++)
        target->limbs[i].hp = 0;

    combat_defeat(target, attacker, true);
}

/* Immortal debug command (`hurtlimb`-style) to force `target`'s `limb`
 * HP to an exact value for testing -- clamps to [0, max_hp], announces
 * a status-tier crossing (and its blood-pool side effect) the same way
 * a real strike would via combat_strike(), and returns whether that
 * crossing was an instant-death major-limb loss, so the caller can
 * follow up exactly like a normal hit would. */
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

        /* Same `bandage` bleeding-flag duplication as the blood pool
         * just above -- see this function's own doc comment. */
        target->limbs[limb].bleeding = true;
    }

    bool instadeath = false;
    if (pct_before > 0 && pct_after == 0 && target->base.kind == THING_PC) {
        combat_sever_limb(actor, target, limb, "hit");
        if (is_major_limb(limb))
            instadeath = true;
    }
    if (instadeath)
        combat_defeat(target, actor, true);
    return instadeath;
}
