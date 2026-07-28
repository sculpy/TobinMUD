/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "cmd.h"
#include "combat.h"
#include "obj.h"
#include "pulse.h"
#include "skill.h"
#include "thing.h"

/* `pray <spell> [target]` -- Cleric spellcasting (user 2026-07-11:
 * "clerics should require a holy symbol to pray successfully...
 * implement task_pray task_cast etc"). Mage/Druid's `cast` (cmd_cast.c)
 * is the sibling command, gated on a consumed component instead of a
 * holy symbol.
 *
 * A "holy symbol" is any object anywhere in the caster's own containment
 * chain (carried, worn, or held) whose keyword list contains the word
 * "symbol" -- a generic convention, same spirit as `cast`'s "component"
 * keyword, so a builder can create any symbol item ("a holy symbol",
 * "a tarnished silver symbol") without a new object category. A symbol
 * now genuinely DECAYS (user 2026-07-18: "the symbols should decay as
 * in sneezy") rather than breaking after one use -- val[0]/val[1] hold
 * current/max strength (obj.h's val[] doc), losing a small random
 * amount on every prayer ATTEMPT (success or fail, same timing
 * components use), only actually shattering once strength runs out.
 * The original's real mechanic (misc/discipline.cc's requireHolySym())
 * costs strength equal to the caster's effective spell level SQUARED,
 * multiplied further if the caster badly overpowers the symbol's own
 * rated level -- that needs a per-symbol "level" rating this port's
 * holy symbol items don't carry, so this keeps the real SHAPE (variable
 * decay, eventual shatter) without that specific formula.
 *
 * Healing prayers ("heal light" etc) may now target someone else in the
 * room ("pray heal light <target>") instead of only the caster -- see
 * find_spell_and_target() below for how the optional trailing target
 * name is told apart from a (possibly multi-word, possibly abbreviated)
 * spell name. A successful heal-type prayer is remembered on the
 * caster (being_t.last_heal_target/last_heal_spell) so the new
 * `continue` command (cmd_continue.c) can keep re-praying it
 * automatically until the target is fully healed or the caster's holy
 * symbols run out ("their holy symbol breaks").
 *
 * v1 scope: same real-generic-effect-per-category approach as cmd_cast.c's
 * task_cast() (see that file's header comment for the full breakdown) --
 * an honest "nothing happens yet" placeholder only for mechanics Tobin
 * has no subsystem for at all yet. */

/* Case-insensitive "does haystack contain needle" -- used to spot
 * keywords like "heal" or "damage" inside a spell's description text. */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

/* Looks through everything `ch` is carrying, wearing, or holding for
 * an item whose name/keywords contain `keyword` -- used here to find
 * a holy symbol. Returns NULL if there isn't one. */
static obj_t *find_keyword_item(const being_t *ch, const char *keyword) {
    size_t len = strlen(keyword);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && thing_name_matches(t->name, keyword, len))
            return (obj_t *)t;
    }
    return NULL;
}

/* Skips a leading inline color tag ("<o>a torch<1>") before capitalizing
 * -- same duplication precedent as cmd_cast.c/cmd_light.c's own
 * cap_first(), needed here for a symbol-shatter message that opens a
 * sentence with the item's own short_descr. */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

/* Spends 1-2 strength from `symbol` (real decay, not a clean counter --
 * see this file's header comment) -- shatters it only once that was the
 * last of it. A pre-existing/never-charged item (val[0]==0) is treated
 * as a single fallback point so it still works once instead of refusing
 * outright. */
static void consume_symbol(descriptor_t *d, obj_t *symbol) {
    int strength = symbol->val[0] > 0 ? symbol->val[0] : 1;
    int decay = 1 + rand() % 2;
    if (strength > decay) {
        symbol->val[0] = strength - decay;
        return;
    }
    char capbuf[128], msg[192];
    const char *label = symbol->base.short_descr[0] ? symbol->base.short_descr : symbol->base.name;
    snprintf(msg, sizeof(msg), "%s shatters from the stress of the prayer!\r\n",
             cap_first(label, capbuf, sizeof(capbuf)));
    descriptor_send(d, msg);
    obj_destroy(symbol);
}

/* `any_class` (immortals -- user 2026-07-12: "immortals can use any
 * skill or spell in game, no class restrictions") searches the whole
 * roster instead of just `cls`. */
static const skill_def_t *find_spell(player_class_t cls, const char *name, bool any_class) {
    size_t len = strlen(name);
    int count = skill_count();
    for (int i = 0; i < count; i++) {
        const skill_def_t *sk = skill_at(i);
        if ((!any_class && sk->cls != cls) || sk->tier == SKILL_TIER_COMBAT)
            continue;
        if (strncasecmp(sk->name, name, len) == 0)
            return sk;
    }
    return NULL;
}

/* Splits `args` into a spell (possibly abbreviated, possibly multi-word)
 * and an optional trailing target name. Tries the WHOLE string against
 * find_spell() first -- if that matches, there's no target (self-pray,
 * the original/common case, left byte-for-byte backward compatible).
 * Only if that fails does it peel off the last word and retry the
 * remainder as the spell name, treating the peeled word as a target
 * ("pray heal light joe" -> spell "heal light", target "joe"; "pray
 * heal lig joe" -> abbreviated "heal lig" still matches "heal light",
 * target "joe"). `*out_target` is set to NULL for a self-pray. */
static const skill_def_t *find_spell_and_target(player_class_t cls, const char *args,
                                                bool any_class, char *target_buf, size_t target_bufsz,
                                                const char **out_target) {
    *out_target = NULL;
    const skill_def_t *sk = find_spell(cls, args, any_class);
    if (sk)
        return sk;

    const char *last_space = strrchr(args, ' ');
    if (!last_space || !last_space[1])
        return NULL;

    size_t spell_len = (size_t)(last_space - args);
    char spell_buf[128];
    if (spell_len >= sizeof(spell_buf))
        return NULL;
    memcpy(spell_buf, args, spell_len);
    spell_buf[spell_len] = '\0';

    sk = find_spell(cls, spell_buf, any_class);
    if (!sk)
        return NULL;

    snprintf(target_buf, target_bufsz, "%s", last_space + 1);
    *out_target = target_buf;
    return sk;
}

/* Damage scales with the SPELL's own min_level, not the caster's current
 * level -- see cmd_cast.c's identical helper (duplicated per this
 * codebase's small-static-helper convention) for the full rationale and
 * calibration notes. Part of "offensive spell breadth" (Sneezy -> Tobin
 * feature audit). */
static int spell_damage_for_level(int min_level) {
    return 4 + min_level + (rand() % (min_level / 3 + 4));
}

/* Real area-effect for prayers whose own description says so verbatim
 * ("area-effect" -- e.g. "plague of locusts", "earthquake") -- see
 * cmd_cast.c's identical function for the full rationale. Duplicated
 * rather than shared, same convention as this file's other helpers. */
static void pray_area_damage(descriptor_t *d, being_t *ch, const skill_def_t *sk) {
    if (!ch->base.roomp) {
        descriptor_send(d, "You aren't anywhere.\r\n");
        return;
    }
    int dmg = spell_damage_for_level(sk->min_level);
    int hit_count = 0;
    room_t *r = ch->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; ) {
        thing_t *next = t->stuff_next;
        if (t == &ch->base || (t->kind != THING_PC && t->kind != THING_MOB)) {
            t = next;
            continue;
        }
        being_t *victim = (being_t *)t;
        if (t->kind == THING_PC && !victim->desc) {
            t = next;
            continue;
        }
        if (being_in_group(ch, victim)) {
            t = next;
            continue;
        }
        hit_count++;
        /* LIMB_REAL_COUNT, not LIMB_COUNT (Limbs -> wearSlotT, 2026-07-26)
         * -- excludes the mob-only, always-inactive EX_* placeholder
         * slots from ever being randomly hit. */
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = victim->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, victim, dmg, limb);
        if (!defeated && victim->desc) {
            char msg[128];
            snprintf(msg, sizeof(msg), "The %s catches you %s!\r\n",
                     sk->name, describe_dam(dmg, limb_hp_before, NULL));
            descriptor_notify(victim->desc, msg);
        }
        t = next;
    }
    char msg[160];
    if (hit_count > 0)
        snprintf(msg, sizeof(msg), "You unleash %s, catching everyone nearby!\r\n", sk->name);
    else
        snprintf(msg, sizeof(msg), "You unleash %s, but there's no one else here to catch in it.\r\n", sk->name);
    descriptor_send(d, msg);
    if (hit_count > 0) {
        snprintf(msg, sizeof(msg), "%s unleashes %s, catching everyone nearby!\r\n",
                 being_display_name(ch), sk->name);
        descriptor_room_echo(r, ch, msg);
    }
}

/* Applies a heal-type prayer's effect to `target` (may be `ch` itself)
 * and reports it to both. cmd_continue.c duplicates this same formula/
 * messaging rather than depending on this file, matching this
 * codebase's established "small static helpers get duplicated per
 * command file" convention (see ci_contains/find_keyword_item above,
 * also duplicated in cmd_cast.c). */
static void pray_apply_heal(descriptor_t *d, being_t *ch, being_t *target, const char *spell_name) {
    int amount = 8 + ch->progress.level / 2;
    being_heal(target, amount);
    char msg[192];
    if (target == ch) {
        snprintf(msg, sizeof(msg), "You pray for %s and feel restored! (+%d HP)\r\n", spell_name, amount);
        descriptor_send(d, msg);
    } else {
        char tcapbuf[128];
        snprintf(msg, sizeof(msg), "You pray for %s, and %s is restored! (+%d HP)\r\n",
                 spell_name, being_display_name(target), amount);
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "%s prays for %s, restoring you! (+%d HP)\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), spell_name, amount);
            descriptor_notify(target->desc, msg);
        }
    }
}

/* `penance` (Cleric, level 1, roster gap/flavor-text pass 2026-07-27):
 * same correction as cmd_cast.c's "meditate" branch -- the roster's own
 * "background discipline governing how fast you gain divine favor"
 * framing had no real Tobin mechanic to hook (no separate divine-favor
 * resource), so it shipped "Not yet wired to a real effect". Realigned
 * with meditate/yoginsa's own shape instead: a single-action Vitality
 * restore, since the user identified all three as "the same" discipline
 * across classes. Same target-defaults-to-self, ally-restores-them-
 * instead shape as pray_apply_heal() just above. */
static void pray_apply_penance(descriptor_t *d, being_t *ch, being_t *target, const char *spell_name) {
    int amount = 8 + ch->progress.level / 2;
    being_heal_vit(target, amount);
    char msg[192];
    if (target == ch) {
        snprintf(msg, sizeof(msg), "You pray for %s and feel your vitality return! (+%d Vit)\r\n", spell_name, amount);
        descriptor_send(d, msg);
    } else {
        char tcapbuf[128];
        snprintf(msg, sizeof(msg), "You pray for %s, and %s looks refreshed! (+%d Vit)\r\n",
                 spell_name, being_display_name(target), amount);
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "%s prays for %s, refreshing your vitality! (+%d Vit)\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), spell_name, amount);
            descriptor_notify(target->desc, msg);
        }
    }
}

/* Works out what a successful prayer actually does, expanded 2026-07-18
 * (user: "implement spell/skill affects... make each work from sneezy
 * code") beyond the original heal/sanctuary/damage-only v1 -- see
 * cmd_cast.c's matching header comment for the full rationale (cure
 * poison/disease reuse this session's own affect work, closing the loop
 * with `drink`'s puddle roll and the hospital's cure; the offensive
 * mirrors -- Cleric's own "poison"/"disease"/"infect" prayers -- now
 * actually inflict them on the caster's current opponent, same targeting
 * convention the damage branch below already used). Remembers heal-type
 * prayers so `continue` (cmd_continue.c) can keep repeating them. */
static void task_pray(descriptor_t *d, being_t *ch, being_t *target, const skill_def_t *sk) {
    char msg[192];
    /* Resolved target for the OFFENSIVE branches only (poison/disease/
     * damage/area, below): an explicit target (target != ch) is used as-
     * is; with none given, falls back to whoever ch is already fighting,
     * same as before this breadth pass -- "pray harm light" with no
     * target still hits your current opponent. NULL (no explicit target
     * AND not fighting anyone) is handled per-branch with a "who?"
     * message rather than silently doing nothing. Heal/buff branches
     * keep using `target` directly (self by default), unaffected. */
    being_t *atk_target = (target != ch) ? target : ch->fighting;
    if (strcasecmp(sk->name, "cure poison") == 0) {
        ch->last_heal_target = NULL;
        bool had = being_has_affect(target, AFFECT_POISON);
        if (had)
            being_remove_affect(target, AFFECT_POISON);
        if (target == ch) {
            snprintf(msg, sizeof(msg), had
                     ? "You pray for %s -- the poison in your veins fades away!\r\n"
                     : "You pray for %s, but you weren't poisoned to begin with.\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), had
                     ? "You pray for %s over %s -- their poison fades away!\r\n"
                     : "You pray for %s over %s, but they weren't poisoned.\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc && had)
                descriptor_notify(target->desc, "The poison in your veins fades away!\r\n");
        }
    } else if (strcasecmp(sk->name, "cure disease") == 0) {
        ch->last_heal_target = NULL;
        bool cured = false;
        for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
            if (affect_is_disease(target->affects[i].type)) {
                being_remove_affect(target, target->affects[i].type);
                cured = true;
            }
        }
        if (target == ch) {
            snprintf(msg, sizeof(msg), cured
                     ? "You pray for %s -- your sickness lifts!\r\n"
                     : "You pray for %s, but you weren't sick to begin with.\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), cured
                     ? "You pray for %s over %s -- their sickness lifts!\r\n"
                     : "You pray for %s over %s, but they weren't sick.\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc && cured)
                descriptor_notify(target->desc, "Your sickness lifts!\r\n");
        }
    } else if (strcasecmp(sk->name, "poison") == 0) {
        /* No longer gated on ch->fighting (offensive spell breadth,
         * Sneezy -> Tobin feature audit) -- opens combat against
         * atk_target the same way an offensive damage prayer now can,
         * see the damage branch below. */
        ch->last_heal_target = NULL;
        if (!atk_target) {
            descriptor_send(d, "Pray for that over whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        being_apply_affect(atk_target, AFFECT_POISON, 20);
        snprintf(msg, sizeof(msg), "You pray for %s, poisoning %s!\r\n", sk->name, being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s prays for %s, poisoning you!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "curse") == 0) {
        /* Full spell/skill/prayer roster import continued, level-5+ list
         * (2026-07-27): real upstream (misc/magicutils.cc's
         * genericCurse()) is a hitroll penalty plus a worsened
         * paralysis-immunity penalty -- Tobin has neither a separate
         * hitroll stat nor a paralysis affect yet, so this lands as a
         * level-scaled DEXTERITY penalty (AFFECT_CURSE, affect.h),
         * standing in for the hitroll debuff since combat_strike()'s
         * own to-hit roll is driven directly off DEXTERITY. "Curses a
         * target or object" -- the object variant (an item that can't
         * be removed once worn) has no Tobin equivalent, dropped. */
        ch->last_heal_target = NULL;
        if (!atk_target) {
            descriptor_send(d, "Pray for that over whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int penalty = sk->min_level / 3;
        if (penalty < 1)
            penalty = 1;
        if (penalty > 5)
            penalty = 5;
        being_apply_stat_affect(atk_target, AFFECT_CURSE, 100, -penalty);
        snprintf(msg, sizeof(msg), "You pray for %s over %s -- a dark aura settles over them!\r\n",
                 sk->name, being_display_name(atk_target));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s prays for %s over you -- a dark aura settles over you!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (ci_contains(sk->name, "disease") || ci_contains(sk->name, "infect")) {
        ch->last_heal_target = NULL;
        if (!atk_target) {
            descriptor_send(d, "Pray for that over whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        affect_type_t dis = affect_random_disease();
        being_apply_affect(atk_target, dis, 40);
        snprintf(msg, sizeof(msg), "You pray for %s, afflicting %s with %s!\r\n",
                 sk->name, being_display_name(atk_target), affect_name(dis));
        descriptor_send(d, msg);
        if (atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s prays for %s, afflicting you with %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name, affect_name(dis));
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "penance") == 0) {
        pray_apply_penance(d, ch, target, sk->name);
    } else if (ci_contains(sk->desc, "heal") || ci_contains(sk->desc, "cure")) {
        pray_apply_heal(d, ch, target, sk->name);
        ch->last_heal_target = target;
        snprintf(ch->last_heal_spell, sizeof(ch->last_heal_spell), "%s", sk->name);
    } else if (ci_contains(sk->desc, "reduces incoming damage") || ci_contains(sk->desc, "improves armor class")
               || ci_contains(sk->desc, "improves hit and damage") || ci_contains(sk->desc, "reflective shield")
               || ci_contains(sk->name, "plasma mirror")) {
        /* Affects system (user 2026-07-11's "buffs/debuffs/status"
         * backlog item) -- flagship example: "sanctuary"'s own
         * description ("A strong aura that reduces incoming damage.")
         * now actually does that (combat.c's combat_strike() halves
         * damage against anyone with AFFECT_SANCTUARY active). Expanded
         * 2026-07-18 to cover "armor"/"bless"/"plasma mirror" too --
         * same shared mechanic, an honest scope-down from three bespoke
         * ones (see cmd_cast.c's matching comment). */
        ch->last_heal_target = NULL;
        being_apply_affect(target, AFFECT_SANCTUARY, 12);
        if (target == ch) {
            snprintf(msg, sizeof(msg), "You pray for %s -- a shimmering aura surrounds you!\r\n", sk->name);
            descriptor_send(d, msg);
        } else {
            snprintf(msg, sizeof(msg), "You pray for %s over %s -- a shimmering aura surrounds them!\r\n",
                     sk->name, being_display_name(target));
            descriptor_send(d, msg);
            if (target->desc)
                descriptor_notify(target->desc, "A shimmering aura surrounds you!\r\n");
        }
    } else if (ci_contains(sk->desc, "area-effect")) {
        /* Real room-wide effect (offensive spell breadth) -- previously
         * fell into the single-target branch below like everything
         * else. Cleric's roster doesn't currently have any spell
         * matching both "area-effect" and "damage"/"bolt"/"strike", but
         * checked first for the same reason cmd_cast.c's mirror does:
         * so one never accidentally silently degrades to single-target. */
        ch->last_heal_target = NULL;
        pray_area_damage(d, ch, sk);
    } else if (ci_contains(sk->desc, "damage") || ci_contains(sk->desc, "bolt")
               || ci_contains(sk->desc, "strike")) {
        /* No longer gated on ch->fighting (offensive spell breadth) --
         * can now open combat against atk_target, same as `attack`/
         * `kill`. If ch is already fighting someone else, this is just
         * a one-off supplemental hit -- the existing fight isn't
         * disturbed (Tobin's `fighting` is strictly 1v1). */
        ch->last_heal_target = NULL;
        if (!atk_target) {
            descriptor_send(d, "Pray for that over whom?\r\n");
            return;
        }
        if (!ch->fighting) {
            ch->fighting = atk_target;
            atk_target->fighting = ch;
            being_set_wait(ch, COMBAT_ROUND_PULSES);
        }
        int dmg = spell_damage_for_level(sk->min_level);
        /* LIMB_REAL_COUNT, not LIMB_COUNT (Limbs -> wearSlotT, 2026-07-26)
         * -- excludes the mob-only, always-inactive EX_* placeholder
         * slots from ever being randomly hit. */
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int limb_hp_before = atk_target->limbs[limb].hp;
        bool defeated = combat_apply_skill_damage(ch, atk_target, dmg, limb);
        /* Damage numbers (user 2026-07-12, follow-up "take out the
         * damage number and use it to describe how hard the hit was"):
         * same describe_dam() treatment as combat.c's melee messages,
         * shown to every viewer now, not just immortals. */
        const char *intensity = describe_dam(dmg, limb_hp_before, NULL);
        snprintf(msg, sizeof(msg), "You pray for %s, striking %s %s!\r\n",
                 sk->name, being_display_name(atk_target), intensity);
        descriptor_send(d, msg);
        if (!defeated && atk_target->desc) {
            char tcapbuf[128];
            snprintf(msg, sizeof(msg), "%s prays for %s, striking you %s!\r\n",
                     being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), sk->name, intensity);
            descriptor_notify(atk_target->desc, msg);
        }
    } else if (strcasecmp(sk->name, "summon swarm") == 0) {
        /* Pet/charm (Sneezy → Tobin feature audit). Reuses the real
         * seeded "swarm locusts cloud" mob (vnum 7852) via
         * being_create_mob(), same non-new-row precedent cmd_cast.c's
         * elemental/animal-companion branch uses. */
        ch->last_heal_target = NULL;
        if (being_find_charmed_pet(ch)) {
            descriptor_send(d, "You already have a charmed creature under your control.\r\n");
            return;
        }
        being_t *pet = being_summon_charmed_pet(ch, 7852, PET_CHARM_DURATION_ROUNDS);
        if (!pet) {
            descriptor_send(d, "Your prayer goes unanswered -- nothing answers your call.\r\n");
            return;
        }
        descriptor_send(d, "A droning cloud of locusts descends and settles at your command!\r\n");
        char capbuf[128], roommsg[256];
        snprintf(roommsg, sizeof(roommsg), "%s prays for %s, and %s descends, obedient to their will!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), sk->name, pet->base.short_descr);
        descriptor_room_echo(ch->base.roomp, ch, roommsg);
    } else {
        ch->last_heal_target = NULL;
        snprintf(msg, sizeof(msg),
                 "You pray for %s, but nothing happens yet -- its real effect isn't implemented.\r\n",
                 sk->name);
        descriptor_send(d, msg);
    }
}

/* Runs the `pray` command: checks the caster is allowed to pray the
 * named spell (class, level, discipline practice), makes sure they
 * have a holy symbol on hand, then applies the spell's effect. See
 * this file's header comment for the full rules. */
bool cmd_pray(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && ch->char_class != CLASS_CLERIC) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "Pray for what? Try 'skills' to see your prayers.\r\n");
        return true;
    }

    char target_buf[64];
    const char *target_name;
    const skill_def_t *sk = find_spell_and_target(CLASS_CLERIC, args, imm, target_buf, sizeof(target_buf), &target_name);
    if (!sk) {
        descriptor_send(d, "You don't know a prayer by that name.\r\n");
        return true;
    }
    if (!imm && ch->progress.level < sk->min_level) {
        char msg[96];
        snprintf(msg, sizeof(msg), "You aren't experienced enough to pray for %s yet (level %d).\r\n",
                 sk->name, sk->min_level);
        descriptor_send(d, msg);
        return true;
    }
    /* Discipline-percentage gate (user 2026-07-12: "players have to
     * visit a guildmaster to gain skills based upon percentage of
     * discipline learned") -- see cmd_practice.c. Bypassed for
     * immortals, same "no restrictions" spirit as the level/class
     * gates above. */
    if (!imm) {
        if (sk->tier == SKILL_TIER_CLASS && ch->progress.basic_disc_pct <= 0) {
            descriptor_send(d, "You haven't practiced your Basic discipline yet -- visit a guildmaster.\r\n");
            return true;
        }
        if (sk->tier == SKILL_TIER_COMBAT && ch->progress.combat_disc_pct <= 0) {
            descriptor_send(d, "You haven't practiced your Combat discipline yet -- visit a combat guildmaster.\r\n");
            return true;
        }
        if (sk->tier == SKILL_TIER_ADVANCED &&
            (ch->progress.basic_disc_pct < 100 || ch->progress.combat_disc_pct < 100
             || ch->progress.advanced_disc_pct <= 0)) {
            descriptor_send(d, "Master your Basic and Combat disciplines, and begin Advanced practice, before this.\r\n");
            return true;
        }
    }

    /* `summon` (Cleric, level 19, audit continued): unlike every other
     * prayer, its target isn't in the caster's own room -- the whole
     * point is pulling someone from ELSEWHERE to you, so it's
     * intercepted here, before the generic room-scoped target
     * resolution below would wrongly refuse with "You don't see them
     * here." Reuses `transfer`'s (cmd_transfer.c) own world-wide,
     * online-only name search and relocation call, plus
     * `combat_pk_allowed()` for mortal-vs-mortal consent -- the same
     * convention every other hostile-capable prayer in this audit
     * (poison/disease/curse/fear/slumber) already follows via
     * `combat_find_room_target()`'s built-in gate, ported by hand here
     * since that helper doesn't apply world-wide. Checked the real
     * upstream first (disc/disc_cleric_hand_of_god.cc's `summon()`/
     * `rawSummon()`): refuses an immortal target outright (no caster-
     * immortal exception at that check either, ported faithfully), plus
     * a `isNotPowerful()` discipline-tier power-gap gate this pass does
     * NOT port -- no clean Tobin equivalent (Tobin has no per-discipline
     * power-tier system), same "Tobin-scale slice" cut headbutt/bodyslam
     * already made for their own real-only mechanics. Also not ported:
     * the real `ROOM_ARENA`/`ROOM_HAVE_TO_WALK`/fall-sector location
     * refusals and the critical-failure branch that flings the CASTER
     * randomly instead (Tobin has no arena/have-to-walk room content
     * yet to gate on). */
    if (strcasecmp(sk->name, "summon") == 0) {
        if (!target_name) {
            descriptor_send(d, "Summon whom?\r\n");
            return true;
        }
        being_t *summ_target = NULL;
        size_t len = strlen(target_name);
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (it->character && it->character->base.roomp
                && strncasecmp(it->character->base.name, target_name, len) == 0) {
                summ_target = it->character;
                break;
            }
        }
        if (!summ_target) {
            char msg[128];
            snprintf(msg, sizeof(msg), "No one named '%s' is in the game.\r\n", target_name);
            descriptor_send(d, msg);
            return true;
        }
        if (summ_target == ch) {
            descriptor_send(d, "You can't summon yourself.\r\n");
            return true;
        }
        if (being_is_immortal(summ_target)) {
            descriptor_send(d, "Summoning the gods can be hazardous to your health...\r\n");
            return true;
        }
        if (!imm && !combat_pk_allowed(ch, summ_target)) {
            descriptor_send(d, "They haven't consented to that kind of magic (toggle pk).\r\n");
            return true;
        }
        if (summ_target->base.roomp == ch->base.roomp) {
            char msg[128];
            snprintf(msg, sizeof(msg), "%s is already here.\r\n", being_display_name(summ_target));
            descriptor_send(d, msg);
            return true;
        }

        obj_t *summ_symbol = find_keyword_item(ch, "symbol");
        if (!summ_symbol) {
            descriptor_send(d, "You need a holy symbol to pray successfully.\r\n");
            return true;
        }

        if (imm || skill_roll_success(skill_learn_from_doing(ch, sk))) {
            room_t *old_room = summ_target->base.roomp;
            char depart_msg[128];
            snprintf(depart_msg, sizeof(depart_msg), "%s vanishes through the cosmic ether!\r\n",
                     summ_target->base.name);
            descriptor_room_echo(old_room, summ_target, depart_msg);

            thing_set_room(&summ_target->base, ch->base.roomp);

            char arrive_msg[128];
            snprintf(arrive_msg, sizeof(arrive_msg), "%s arrives through the cosmic ether!\r\n",
                     summ_target->base.name);
            descriptor_room_echo(ch->base.roomp, summ_target, arrive_msg);

            if (summ_target->desc) {
                descriptor_send(summ_target->desc,
                                 "You feel a tug and are transferred through the cosmic ether!\r\n");
                cmd_dispatch(summ_target->desc, "look");
            }

            char msg[128];
            snprintf(msg, sizeof(msg), "You pray for %s -- %s appears before you!\r\n",
                     sk->name, being_display_name(summ_target));
            descriptor_send(d, msg);
        } else {
            char msg[128];
            snprintf(msg, sizeof(msg), "You fumble the prayer for %s -- nothing happens.\r\n", sk->name);
            descriptor_send(d, msg);
        }
        consume_symbol(d, summ_symbol);
        return true;
    }

    /* Defaults to self, same as always (heal/buff prayers with no
     * target mean self) -- the offensive branches below separately fall
     * back to ch->fighting when target is still `ch` at that point, so
     * "pray harm light" with no target keeps hitting whoever you're
     * already fighting, unchanged from before the breadth pass. */
    being_t *target = ch;
    if (target_name) {
        target = combat_find_room_target(ch, target_name);
        if (!target) {
            descriptor_send(d, "You don't see them here.\r\n");
            return true;
        }
    }

    obj_t *symbol = find_keyword_item(ch, "symbol");
    if (!symbol) {
        descriptor_send(d, "You need a holy symbol to pray successfully.\r\n");
        return true;
    }

    /* Per-skill proficiency (Sneezy-style learn-by-doing, user 2026-07-17)
     * -- separate from the discipline-percentage ACCESS gate above, this
     * is the caster's own success chance at THIS specific prayer, and it
     * climbs with every attempt. Immortals always succeed, same "no
     * restrictions" spirit as their other gate bypasses. */
    if (imm || skill_roll_success(skill_learn_from_doing(ch, sk))) {
        task_pray(d, ch, target, sk);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "You fumble the prayer for %s -- nothing happens.\r\n", sk->name);
        descriptor_send(d, msg);
    }
    consume_symbol(d, symbol);
    return true;
}
