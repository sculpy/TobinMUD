/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "being.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "balance.h"
#include "body.h"
#include "descriptor.h"
#include "gametime.h"
#include "log.h"
#include "mob_repo.h"
#include "obj.h"
#include "room.h"

/* Maps a stat token (full name or 3-letter abbreviation, either case) to
 * the matching field inside `a`, so `set`/`stat`-style commands can look
 * up "str"/"strength" etc. by user-typed text instead of a switch at
 * every call site. Returns NULL for an unrecognized token. */
int *attrs_field(attrs_t *a, const char *tok) {
    if (strcasecmp(tok, "str") == 0 || strcasecmp(tok, "strength") == 0) return &a->strength;
    if (strcasecmp(tok, "dex") == 0 || strcasecmp(tok, "dexterity") == 0) return &a->dexterity;
    if (strcasecmp(tok, "con") == 0 || strcasecmp(tok, "constitution") == 0) return &a->constitution;
    if (strcasecmp(tok, "int") == 0 || strcasecmp(tok, "intelligence") == 0) return &a->intelligence;
    if (strcasecmp(tok, "wis") == 0 || strcasecmp(tok, "wisdom") == 0) return &a->wisdom;
    if (strcasecmp(tok, "cha") == 0 || strcasecmp(tok, "charisma") == 0) return &a->charisma;
    return NULL;
}

/* Allocates and initializes a brand-new player character -- base attrs,
 * starting HP/vit/hunger/thirst, and the humanoid body every PC uses.
 * Used at character creation; an existing PC is loaded from the DB
 * elsewhere (player_repo.c), not built through here. Caller owns the
 * returned being_t and must eventually being_destroy() it. */
being_t *being_create_pc(const char *name, long account_id, long player_id) {
    being_t *b = calloc(1, sizeof(*b));
    if (!b)
        return NULL;

    b->base.kind = THING_PC;
    b->base.id = (int)player_id;
    snprintf(b->base.name, sizeof(b->base.name), "%s", name ? name : "");
    snprintf(b->base.short_descr, sizeof(b->base.short_descr), "%s", name ? name : "");
    b->account_id = account_id;
    b->player_id = player_id;
    b->attrs.strength = b->attrs.dexterity = b->attrs.constitution =
        b->attrs.intelligence = b->attrs.wisdom = b->attrs.charisma = ATTR_BASE;

    b->handed_right = 1; /* right-handed default */
    b->body_type = BODY_HUMANOID; /* every PC -- Body types, 2026-07-26 */
    b->position = POSITION_STANDING;
    b->progress.level = MORTAL_LEVEL_MIN;
    b->progress.experience = 0;
    b->progress.max_hp = being_calc_max_hp(b);
    b->progress.hp = b->progress.max_hp;
    being_limbs_full_heal(b);
    b->progress.max_vit = being_calc_max_vit(b);
    b->progress.vit = b->progress.max_vit;
    b->progress.hunger = 100;
    b->progress.thirst = 100;
    /* A starting stipend (user 2026-07-28: "let all players start with
     * 7 pracs to spend") -- separate from practice_points_for_level()'s
     * own per-level-up award, so a brand-new character isn't stuck at
     * 0% Basic discipline until they actually gain a level. */
    b->progress.practice_points = 7;
    b->progress.birth_time = (long)time(NULL);
    b->severity = LOG_SEVERITY_DEFAULT;

    /* fighting, last_combat_pulse, wait_pulses are already zeroed by calloc */

    return b;
}

/* Maps the upstream `mob.class` bitmask to a Tobin player_class_t --
 * only the single-class bits that have a real Tobin equivalent (user
 * 2026-07-12's practice/guildmaster request). Shaman(16)/deikhan(32)/
 * other(256) have no Tobin class and are left unmapped; ranger(128)
 * maps to Druid, matching the Druid roster's own Ranger-skill lineage
 * (see skill.c's Druid section). */
bool mob_class_mask_to_tobin(int mask, player_class_t *out) {
    switch (mask) {
        case 1:   *out = CLASS_MAGE;    return true;
        case 2:   *out = CLASS_CLERIC;  return true;
        case 4:   *out = CLASS_WARRIOR; return true;
        case 8:   *out = CLASS_THIEF;   return true;
        case 64:  *out = CLASS_MONK;    return true;
        case 128: *out = CLASS_DRUID;   return true;
        default:  return false;
    }
}

/* Allocates and initializes a mob instance from its vnum prototype
 * (mob_proto_load()) -- name/description/gender/class/race copied
 * straight across, but attrs/HP are derived from level rather than
 * copied from the original's incompatible 12-stat scale (see the
 * comment on mob_attr below). Used by anything that spawns a mob:
 * zone resets, being_summon_charmed_pet(), being_start_polymorph(), etc.
 * Caller owns the returned being_t. */
being_t *being_create_mob(int vnum) {
    mob_proto_t proto;
    if (!mob_proto_load(vnum, &proto))
        return NULL;

    being_t *b = calloc(1, sizeof(*b));
    if (!b)
        return NULL;

    b->base.kind = THING_MOB;
    b->base.id = vnum;
    snprintf(b->base.name, sizeof(b->base.name), "%s", proto.name);
    snprintf(b->base.short_descr, sizeof(b->base.short_descr), "%s", proto.short_descr);
    snprintf(b->appearance, sizeof(b->appearance), "%s", proto.description);

    b->gender = (gender_t)proto.sex;
    b->progress.level = proto.level;
    b->progress.experience = 0;
    b->mob_actions = proto.actions;
    b->mob_align = proto.align;
    b->mob_spec_proc = proto.spec_proc;
    b->mob_race = proto.race;
    b->body_type = proto.body_type; /* Body types, 2026-07-26 -- must be set
                                        BEFORE being_limbs_full_heal() below,
                                        which reads it to decide which limbs
                                        this mob actually has */
    b->mob_class_known = mob_class_mask_to_tobin(proto.class_mask, &b->char_class);

    /* Placeholder attrs/HP formulas (see STATUS.md's Mobiles decision row):
     * the original's 12-stat mob columns are a completely different, wider
     * scale than Tobin's ATTR_BASE-centered 6-stat set (real seed values
     * range well outside 90-150), so copying them verbatim would make
     * combat_strike() wildly unbalanced. Deriving attrs from level instead
     * keeps mobs on the same scale PCs already use. `hpbonus` (really the
     * original's primary HP-scaling parameter, not a "+bonus") drives HP. */
    int mob_attr = ATTR_BASE + proto.level;
    if (mob_attr > ATTR_MAX)
        mob_attr = ATTR_MAX;
    b->attrs.strength = b->attrs.dexterity = b->attrs.constitution =
        b->attrs.intelligence = b->attrs.wisdom = b->attrs.charisma = mob_attr;

    b->handed_right = 1;
    b->position = POSITION_STANDING;
    b->progress.max_hp = 20 + proto.level * 5 + (int)(proto.hpbonus * 10);
    if (b->progress.max_hp < 1)
        b->progress.max_hp = 1;
    b->progress.hp = b->progress.max_hp;
    being_limbs_full_heal(b);

    /* account_id, player_id, desc all stay 0/NULL -- never a real DB row. */

    return b;
}

/* Tears down a being completely: scrubs every dangling reference to it
 * from other live beings (fighting/last_heal_target/mount/rider, and a
 * connected PC's own charmed pet independently fighting it), frees its
 * carried/worn/held objects, leaves its group, unlinks it from its room,
 * and frees the struct itself. Callers must not touch `b` again after
 * this returns -- see the callers in affect.c (dissolve_charmed_pet(),
 * revert_polymorph()) for the "safe to delete mid-iteration" pattern
 * this is designed to support. */
void being_destroy(being_t *b) {
    if (!b)
        return;

    /* Clear any dangling reference from an opponent still fighting us --
     * otherwise the next combat round would dereference freed memory. */
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        if (d->character && d->character->fighting == b)
            d->character->fighting = NULL;
        if (d->character && d->character->last_heal_target == b)
            d->character->last_heal_target = NULL;
        /* b was a mount some connected rider was on (b is being destroyed
         * out from under them, e.g. a ridden horse dying in combat) --
         * dismount them so the game loop never dereferences b again. */
        if (d->character && d->character->mount == b) {
            d->character->mount = NULL;
            if (d->character->position == POSITION_MOUNTED)
                d->character->position = POSITION_STANDING;
        }
        /* Pet/charm: a connected PC's own charmed pet may ALSO have been
         * independently fighting b (combat.c's pet-assist pass gives a
         * pet its own one-sided ->fighting pointer, separate from its
         * master's) -- b has no descriptor of its own for a symmetric
         * check to catch this, so it's done here instead, from the
         * master's side. Without this, a second pet piling onto the same
         * target the first pet just killed (or a pet whose target the
         * PC's own strike killed first) would dereference freed memory
         * on its next combat tick. */
        if (d->character) {
            being_t *pet = being_find_charmed_pet(d->character);
            if (pet && pet->fighting == b)
                pet->fighting = NULL;
        }
    }
    /* b itself was riding something when it was destroyed (a PC quitting
     * or dying while mounted) -- the mount survives, so clear ITS side of
     * the relationship directly (it's not a connected descriptor's
     * character, so the loop above never reaches it) and let it resume
     * normal standing/AI. */
    if (b->mount) {
        if (b->mount->rider == b)
            b->mount->rider = NULL;
        if (b->mount->position == POSITION_MOUNTED)
            b->mount->position = POSITION_STANDING;
    }

    /* Free every object this being has (carried, worn, or held -- all live
     * in the one stuff_head chain, see being.h's equipment[]/held[]
     * comment). Safe: nothing else can still reference these once b goes
     * away -- player_load_admin()'s snapshot-copy-then-destroy path
     * (edplayer/set) never populates a being's inventory in the first
     * place (see player_repo.c), so this is never called on a being whose
     * objects are also referenced by a live copy elsewhere. */
    thing_t *carried = b->base.stuff_head;
    while (carried) {
        thing_t *next = carried->stuff_next;
        if (carried->kind == THING_OBJ)
            obj_destroy((obj_t *)carried);
        carried = next;
    }
    for (int i = 0; i < LIMB_COUNT; i++)
        b->equipment[i] = NULL;
    for (int i = 0; i < 2; i++)
        b->held[i] = NULL;

    being_leave_group(b);

    thing_remove_from_parent(&b->base);
    free(b);
}

/* True if `a` and `b` are in the same group -- same being, one leads the
 * other, or both share the same leader. Groups here are a flat
 * leader+followers[] structure (see being.h), not arbitrary graphs, so
 * this is just three cheap pointer comparisons. */
bool being_in_group(const being_t *a, const being_t *b) {
    if (!a || !b)
        return false;
    if (a == b)
        return true;
    if (!a->grouped || !b->grouped)
        return false;
    if (a->master == b || b->master == a)
        return true;
    return a->master && a->master == b->master;
}

/* Fills `out` with every grouped member of self's group (leader first,
 * then followers), up to `max` entries, and returns the count actually
 * written -- the one place that flattens leader+followers[] into a
 * plain list for callers like group-wide commands (xp splits, `who`
 * group display, etc.) that just want to iterate members. */
int being_group_members(const being_t *self, being_t **out, int max) {
    if (!self || !self->grouped || max <= 0)
        return 0;

    being_t *leader = self->master ? self->master : (being_t *)self;
    int n = 0;
    if (leader->grouped && n < max)
        out[n++] = leader;
    for (int i = 0; i < GROUP_MAX_FOLLOWERS && n < max; i++) {
        being_t *f = leader->followers[i];
        if (f && f->grouped)
            out[n++] = f;
    }
    return n;
}

/* Removes `b` from whatever group it's in, in either role: as a
 * follower (unlinked from its master's followers[]) or as a leader
 * (the group dissolves outright -- no succession, followers just
 * become ungrouped individuals; see being.h's field comment for why).
 * Called from being_destroy() so a dying/quitting member always leaves
 * its group cleanly. */
void being_leave_group(being_t *b) {
    if (!b)
        return;

    /* Remove b from its master's followers[] (if any). */
    if (b->master) {
        for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
            if (b->master->followers[i] == b) {
                b->master->followers[i] = NULL;
                break;
            }
        }
        b->master = NULL;
    }
    b->grouped = false;

    /* b was itself a leader -- the group dissolves (no succession, see
     * being.h's field comment). */
    for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
        being_t *f = b->followers[i];
        if (!f)
            continue;
        f->master = NULL;
        f->grouped = false;
        b->followers[i] = NULL;
    }
}

/* Finds `master`'s charmed pet, if it has one -- scans followers[] for
 * the one carrying AFFECT_CHARMED (a charmed pet is stored as an
 * ordinary follower, distinguished only by that affect). Returns NULL
 * if there is none. */
being_t *being_find_charmed_pet(const being_t *master) {
    if (!master)
        return NULL;
    for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
        being_t *f = master->followers[i];
        if (f && being_has_affect(f, AFFECT_CHARMED))
            return f;
    }
    return NULL;
}

/* Charm spell entry point: spawns the mob at `vnum` into master's room,
 * attaches it as a follower, and slaps AFFECT_CHARMED on it for
 * `duration_rounds` (its expiry in affect.c's dissolve_charmed_pet()
 * dissolves the pet). Refuses if master already has a charmed pet
 * (being_find_charmed_pet()) or has no free follower slot -- only one
 * pet at a time, same as the followers[] cap. */
being_t *being_summon_charmed_pet(being_t *master, int vnum, int duration_rounds) {
    if (!master || !master->base.roomp)
        return NULL;
    if (being_find_charmed_pet(master))
        return NULL;

    int slot = -1;
    for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
        if (!master->followers[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return NULL;

    being_t *pet = being_create_mob(vnum);
    if (!pet)
        return NULL;

    thing_set_room(&pet->base, master->base.roomp);
    pet->master = master;
    master->followers[slot] = pet;
    being_apply_affect(pet, AFFECT_CHARMED, duration_rounds);
    return pet;
}

/* Polymorph spell entry point: swaps descriptor `d` onto a freshly
 * created mob body at `vnum`, stashing the player's real being in
 * d->possess_original so it can be restored later, and starts
 * AFFECT_POLYMORPH ticking down (its expiry in affect.c's
 * revert_polymorph() swaps back). Refuses if `d` is already possessing/
 * polymorphed into something (possess_original already set) or has no
 * character/room to polymorph from. */
bool being_start_polymorph(descriptor_t *d, int vnum, int duration_rounds) {
    if (!d || !d->character || d->possess_original)
        return false;
    being_t *ch = d->character;
    if (!ch->base.roomp)
        return false;

    being_t *form = being_create_mob(vnum);
    if (!form)
        return false;

    thing_set_room(&form->base, ch->base.roomp);
    d->possess_original = ch;
    ch->desc = NULL;
    d->character = form;
    form->desc = d;
    being_apply_affect(form, AFFECT_POLYMORPH, duration_rounds);
    return true;
}

/* True once `b`'s level reaches IMMORTAL_LEVEL_MIN -- the one check
 * gating a long list of immortal-only behaviors (disease/poison
 * immunity, no wait-state, always-lit vision, etc.) across the
 * codebase, so it lives here rather than being reimplemented per call
 * site. */
bool being_is_immortal(const being_t *b) {
    return b && b->progress.level >= IMMORTAL_LEVEL_MIN;
}

/* Whether `ch` currently can't see in `r` for lack of light -- false for
 * an immortal, an always-lit/indoor room, or daytime, and otherwise
 * depends on whether `ch` is carrying an active light source
 * (being_has_active_light() below). Drives look/move description
 * suppression and any other "you can't see" gate. */
bool room_is_dark_for(const struct room *r, const being_t *ch) {
    const room_t *room = (const room_t *)r;
    if (!room || !ch)
        return false;
    if (being_is_immortal(ch))
        return false;
    if (room->room_flag & (ROOM_FLAG_ALWAYS_LIT | ROOM_FLAG_INDOORS))
        return false;
    if (gametime_is_daytime())
        return false;
    return !being_has_active_light(ch);
}

/* True if `b` is carrying/wielding any object that's a lit light source
 * (OBJ_CAT_LIGHT with fuel remaining in val[3]) -- scans the being's
 * whole stuff_head chain since a light doesn't have to be wielded to
 * count, just carried lit. Used by room_is_dark_for() above. */
bool being_has_active_light(const being_t *b) {
    if (!b)
        return false;
    for (thing_t *t = b->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        const obj_t *o = (const obj_t *)t;
        if (o->category == OBJ_CAT_LIGHT && o->val[3])
            return true;
    }
    return false;
}

/* Forces `name` into Capitalized-first-letter, lowercase-rest form
 * in place -- used to tidy up player-typed names (character creation,
 * immortal `set` commands) so display and DB lookups stay consistent
 * regardless of how the player capitalized it when typing. */
void being_normalize_name(char *name) {
    if (!name || !name[0])
        return;
    name[0] = (char)toupper((unsigned char)name[0]);
    for (int i = 1; name[i]; i++)
        name[i] = (char)tolower((unsigned char)name[i]);
}

/* Immortal rank title for `level` ("Immortal"/"God"/.../"Implementor"),
 * or NULL for a mortal level -- used wherever an immortal's rank needs
 * spelling out (e.g. `who`, `stat`), paired with being_rank_color()
 * below for the matching display color. */
const char *being_level_title(int level) {
    if (level < IMMORTAL_LEVEL_MIN)
        return NULL;
    if (level <= 53)
        return "Immortal";
    if (level <= 57)
        return "God";
    if (level == 58)
        return "Greater God";
    if (level == 59)
        return "Administrator";
    return "Implementor"; /* 60+ */
}

/* Color code tier matching being_level_title()'s rank bands (higher
 * immortal ranks get brighter/rarer colors), or "" for a mortal --
 * pairs with being_level_title() wherever a rank needs both a label and
 * a color, e.g. `who`. */
const char *being_rank_color(int level) {
    if (level >= 59)
        return "<P>"; /* 59+ */
    if (level >= 57)
        return "<p>"; /* 57-58 */
    if (level >= 54)
        return "<C>"; /* 54-56 */
    if (level >= IMMORTAL_LEVEL_MIN)
        return "<c>"; /* 51-53 */
    return ""; /* mortal */
}

/* Reads `b`'s current wait-state (pulses left before their next action
 * is allowed), always 0 for an immortal regardless of what
 * wait_pulses actually holds -- immortals are never rate-limited by
 * combat/skill lag. */
int being_get_wait(const being_t *b) {
    if (!b)
        return 0;
    return being_is_immortal(b) ? 0 : b->wait_pulses;
}

/* Sets `b`'s wait-state, but is a no-op for an immortal -- the write
 * side of being_get_wait()'s immunity, so combat/skill code can call
 * this unconditionally without special-casing immortals itself. */
void being_set_wait(being_t *b, int pulses) {
    if (!b || being_is_immortal(b))
        return;
    b->wait_pulses = pulses;
}

/* Relative HP-per-level scaling by class (PCs only), loosely mirroring the
 * original's hpGainForClass()'s ranking (warrior/monk tankiest, mage
 * squishiest) without porting its per-level-roll mechanic -- Tobin has no
 * level-up event to hook a per-level roll into yet, so this just scales
 * the flat per-level term class_stat_bonus() doesn't otherwise touch. */
/* Real per-level HP gain, by class (misc/limits.cc's hpGainForClass()):
 * a random roll each level-up (Warrior 6-11, Cleric 5-7, Ranger 6-8,
 * Thief 4-8, Monk 4-7, Mage 3-7) -- averaged here since Tobin recomputes
 * max_hp on demand from level rather than accumulating random per-level
 * rolls. Ranger has no Tobin class of its own; Druid reuses its number,
 * matching the existing CLASS_DRUID<-mob.class(ranger) mapping
 * (being_map_mob_class(), Session ~2026-07-12) elsewhere in this file.
 * Replaces the old placeholder ratios (Warrior 1.3x/Mage 0.8x etc, an
 * unsourced guess) applied to a flat "5 per level" -- this is now the
 * real average gain itself, not a multiplier on an arbitrary base.
 * User (2026-07-28): "hp assigned to new characters and hp gain per
 * level need balancing" -- grounded in source; the `balance` command's
 * own class_balance_get()->hp_mult (applied on top, unchanged below)
 * stays the live-adjustable knob for anything these real averages still
 * don't quite hit (e.g. a from-scratch Warrior computes to ~28 HP at
 * level 1 and ~445 at level 50 against real upstream's own documented
 * anchors of 25 and 500 -- the gap is real upstream's PER-LEVEL
 * multiplicative CON scaling, getConHpModifier(), which this pass does
 * NOT port; Tobin's con_bonus stays a flat one-time addition). */
static double class_hp_per_level(player_class_t c) {
    switch (c) {
        case CLASS_WARRIOR: return 8.5;
        case CLASS_CLERIC:  return 6.0;
        case CLASS_DRUID:   return 7.0;
        case CLASS_THIEF:   return 6.0;
        case CLASS_MONK:    return 5.5;
        case CLASS_MAGE:    return 5.0;
        default:            return 5.0;
    }
}

/* Recomputes `b`'s max HP from current constitution, level, and
 * class/race (PC) or known class (mob) balance multipliers -- called
 * whenever any of those inputs change (level up, stat change,
 * `balance` command edits) so max_hp always reflects the current
 * formula rather than being cached stale. See class_hp_per_level()
 * above and balance.c's class_balance_get()/race_balance_get() for the
 * two multiplier sources this combines. */
int being_calc_max_hp(const being_t *b) {
    if (!b)
        return 20;
    int con_bonus = b->attrs.constitution - ATTR_BASE;
    double per_level = 5.0;
    double scale = 1.0;
    /* Gamewide HP multiplier (user 2026-07-12's `balance` command) --
     * a PC's own class+race, or a guildmaster mob's known class (no
     * race applies to mobs). Neutral (1.0) until an immortal actually
     * balances that class/race, so this is a no-op by default. */
    if (b->base.kind == THING_PC) {
        per_level = class_hp_per_level(b->char_class);
        scale = class_balance_get(b->char_class)->hp_mult;
        scale *= race_balance_get(b->race)->hp_mult;
    } else if (b->mob_class_known) {
        per_level = class_hp_per_level(b->char_class);
        scale *= class_balance_get(b->char_class)->hp_mult;
    }
    return 20 + con_bonus + (int)(b->progress.level * per_level * scale);
}

/* Recomputes `b`'s max vitality (the stamina-like pool skills/spells
 * spend from) from constitution and level, same "recompute on demand"
 * role as being_calc_max_hp() above but with its own flatter, class-
 * independent formula. */
int being_calc_max_vit(const being_t *b) {
    if (!b)
        return 50;
    int con_bonus = b->attrs.constitution - ATTR_BASE;
    return 50 + con_bonus + b->progress.level * 2;
}

static const char *LIMB_NAMES[LIMB_COUNT] = {
    "head", "neck", "back", "left arm", "right arm", "left wrist", "right wrist",
    "left hand", "right hand", "left finger", "right finger",
    "body", "waist", "genitalia", "right leg", "left leg", "left foot", "right foot",
    "extra right leg", "extra left leg", "extra right foot", "extra left foot",
};

/* position_types[] (misc/being.cc), indexed by position_t. */
static const char *const POSITION_NAMES[] = {
    "Dead", "Mortally wounded", "Incapacitated", "Stunned", "Sleeping",
    "Resting", "Sitting", "Engaged", "Fighting", "Crawling", "Standing",
    "Mounted", "Flying",
};

/* Display text for a position_t (POSITION_NAMES[] above, ported from
 * the original's position_types[]) -- falls back to "Standing" for an
 * out-of-range value rather than reading past the array. */
const char *position_name(position_t p) {
    if (p < 0 || (size_t)p >= sizeof(POSITION_NAMES) / sizeof(POSITION_NAMES[0]))
        return "Standing";
    return POSITION_NAMES[p];
}

/* Reverse of position_name() above: matches a (possibly abbreviated)
 * typed name against POSITION_NAMES[] by prefix, for commands that let
 * an immortal set a position by typed word (e.g. `set position`).
 * Refuses an ambiguous prefix (matches more than one name) rather than
 * silently picking the first match. */
bool position_from_name(const char *name, position_t *out) {
    size_t len = strlen(name);
    if (len == 0)
        return false;
    int match = -1;
    size_t count = sizeof(POSITION_NAMES) / sizeof(POSITION_NAMES[0]);
    for (size_t i = 0; i < count; i++) {
        if (strncasecmp(POSITION_NAMES[i], name, len) == 0) {
            if (match >= 0)
                return false; /* ambiguous prefix */
            match = (int)i;
        }
    }
    if (match < 0)
        return false;
    *out = (position_t)match;
    return true;
}

/* gender_name()/subject()/object()/possess()/reflexive() below are the
 * pronoun family for a gender_t: display name, and the he/she/it,
 * him/her/it, his/her/its, himself/herself/itself forms used when
 * building a message about a being in the third person. All default to
 * the neuter form for GENDER_NEUTER or anything unrecognized. */
const char *gender_name(gender_t g) {
    switch (g) {
        case GENDER_MALE:   return "male";
        case GENDER_FEMALE: return "female";
        case GENDER_NEUTER:
        default:            return "neuter";
    }
}

const char *gender_subject(gender_t g) {
    switch (g) {
        case GENDER_MALE:   return "he";
        case GENDER_FEMALE: return "she";
        case GENDER_NEUTER:
        default:            return "it";
    }
}

const char *gender_object(gender_t g) {
    switch (g) {
        case GENDER_MALE:   return "him";
        case GENDER_FEMALE: return "her";
        case GENDER_NEUTER:
        default:            return "it";
    }
}

const char *gender_possess(gender_t g) {
    switch (g) {
        case GENDER_MALE:   return "his";
        case GENDER_FEMALE: return "her";
        case GENDER_NEUTER:
        default:            return "its";
    }
}

const char *gender_reflexive(gender_t g) {
    switch (g) {
        case GENDER_MALE:   return "himself";
        case GENDER_FEMALE: return "herself";
        case GENDER_NEUTER:
        default:            return "itself";
    }
}

static const char *const CLASS_NAMES[CLASS_COUNT] = {
    "Mage", "Cleric", "Warrior", "Thief", "Druid", "Monk",
};

/* Display name for a PC class (CLASS_NAMES[] above) -- falls back to
 * "Mage" for an out-of-range value. */
const char *class_name(player_class_t c) {
    if (c < 0 || c >= CLASS_COUNT)
        return "Mage";
    return CLASS_NAMES[c];
}

/* Display label for a mob's raw class bitmask -- "none" for no class,
 * the mapped Tobin class name via mob_class_mask_to_tobin() where one
 * exists, or "unmapped (mask N)" so an immortal looking at `stat` can
 * still see the raw value for a class Tobin has no equivalent for. */
const char *mob_class_label(int mask, char *buf, size_t bufsz) {
    if (mask == 0) {
        snprintf(buf, bufsz, "none");
        return buf;
    }
    player_class_t cls;
    if (mob_class_mask_to_tobin(mask, &cls))
        snprintf(buf, bufsz, "%s", class_name(cls));
    else
        snprintf(buf, bufsz, "unmapped (mask %d)", mask);
    return buf;
}

/* Every class's bonus/penalty sums to zero -- see the declaration comment
 * (being.h) for the stat-affinity rationale. */
void class_stat_bonus(player_class_t c, attrs_t *a) {
    if (!a)
        return;
    switch (c) {
        case CLASS_MAGE:
            a->intelligence += 4;
            a->strength -= 4;
            break;
        case CLASS_CLERIC:
            a->wisdom += 4;
            a->strength -= 2;
            a->dexterity -= 2;
            break;
        case CLASS_WARRIOR:
            a->constitution += 3;
            a->strength += 3;
            a->charisma -= 3;
            a->wisdom -= 3;
            break;
        case CLASS_THIEF:
            a->dexterity += 4;
            a->strength -= 4;
            break;
        case CLASS_DRUID:
            a->wisdom += 2;
            a->constitution += 2;
            a->intelligence -= 4;
            break;
        case CLASS_MONK:
            a->strength += 2;
            a->constitution += 2;
            a->charisma -= 4;
            break;
        default:
            break;
    }
}

static const char *const RACE_NAMES[RACE_COUNT] = {
    "Human", "Elf", "Ogre", "Dwarf", "Hobbit", "Gnome",
};

/* Display name for a PC race (RACE_NAMES[] above) -- falls back to
 * "Human" for an out-of-range value. */
const char *race_name(player_race_t r) {
    if (r < 0 || r >= RACE_COUNT)
        return "Human";
    return RACE_NAMES[r];
}

/* The original's full monster-race table (misc/race.cc's RaceNames[],
 * "RACE_" prefix stripped) -- completely separate from RACE_NAMES/
 * race_name() above (Tobin's own 6-entry PLAYER race set). Used only by
 * mob_race_name() below to decode a MOB's raw `mob.race` column for
 * `stat` (user 2026-07-12) -- "no race applies to mobs" mechanically
 * (balance.c), this is display-only, not tied to any Tobin mechanic. */
static const char *const MOB_RACE_NAMES[] = {
    "NORACE", "HUMAN", "ELVEN", "DWARF", "HOBBIT", "GNOME", "OGRE",
    "PEGASUS", "LYCANTH", "DRAGON", "UNDEAD", "ORC", "INSECT", "ARACHNID",
    "DINOSAUR", "FISH", "BIRD", "GIANT", "BIRDMAN", "PARASITE", "SLIME",
    "DEMON", "SNAKE", "HIPPOPOTAMUS", "TREE", "VEGGIE", "ELEMENT", "ANT",
    "DEVIL", "FROGMAN", "GOBLIN", "TROLL", "ANGEL", "MFLAYER", "PRIMATE",
    "FAERIE", "DROW", "GOLEM", "BANSHEE", "PANTATH", "MERMAID", "RODENT",
    "FISHMAN", "TYTAN", "WOODELF", "FELINE", "CANINE", "HORSE", "AMPHIB",
    "VAMPIRE", "REPTILE", "UNCERT", "VAMPIREBAT", "OCTOPUS", "CRUSTACEAN",
    "MOSS", "BOVINE", "GOAT", "SHEEP", "DEER", "BEAR", "WEASEL", "SQUIRREL",
    "RABBIT", "BADGER", "OTTER", "BEAVER", "PIG", "BOAR", "TURTLE",
    "GIRAFFE", "CENTIPEDE", "MOUND", "PIERCER", "ORB", "MANTICORE",
    "GRIFFON", "SPHINX", "SHEDU", "LAMMASU", "DJINN", "PHOENIX",
    "DRAGONNE", "HIPPOGRIFF", "RUST_MON", "LION", "TIGER", "LEOPARD",
    "COUGAR", "FROG", "ELEPHANT", "RHINO", "NAGA", "OTYUGH", "OX",
    "GREMLIN", "OWLBEAR", "CHIMERA", "SATYR", "DRYAD", "BUGBEAR",
    "MINOTAUR", "GORGON", "KOBOLD", "BASILISK", "LIZARD_MAN", "CENTAUR",
    "GOPHER", "LAMIA", "SAHUAGIN", "BAT", "PYGMY", "WYVERN", "KUOTOA",
    "BAANTA", "GNOLL", "HOBGOBLIN", "MIMIC", "MEDUSA", "PENGUIN",
    "OSTRICH", "TROG", "COATL", "SIMAL", "WYVELIN", "FLYINSECT", "RATMAN",
};
#define MOB_RACE_COUNT (sizeof(MOB_RACE_NAMES) / sizeof(MOB_RACE_NAMES[0]))

const char *mob_race_name(int idx) {
    if (idx < 0 || (size_t)idx >= MOB_RACE_COUNT)
        return "unknown";
    return MOB_RACE_NAMES[idx];
}

/* Mundane real-world creature races (user 2026-07-19: "animal races
 * should not have wealth, that doesnt make sense") -- deliberately
 * excludes anything fantastical/sapient (DRAGON, ORC, GOBLIN, UNDEAD,
 * DEMON, ...), even where an argument could be made either way
 * (VAMPIREBAT stays with its VAMPIRE cousin, not here; GIANT/PRIMATE-
 * adjacent humanoids stay out too), and excludes plants/oozes/
 * elementals (TREE, VEGGIE, MOSS, SLIME, ELEMENT) since the user asked
 * about ANIMAL races specifically, not "things too dumb to carry a coin
 * purse" generally. AskUserQuestion-confirmed scope with the user
 * before implementing. */
bool mob_race_is_rideable(int idx) {
    return idx == 47; /* HORSE */
}

/* True for a MOB_RACE_NAMES[] index that's a mundane real-world animal
 * (see the "Mundane real-world creature races" comment on
 * mob_race_is_rideable() just above for the exact inclusion/exclusion
 * rationale) -- used to decide things like "animals don't carry
 * wealth" (user 2026-07-19). */
bool mob_race_is_animal(int idx) {
    switch (idx) {
        case 12:  /* INSECT */
        case 13:  /* ARACHNID */
        case 14:  /* DINOSAUR */
        case 15:  /* FISH */
        case 16:  /* BIRD */
        case 22:  /* SNAKE */
        case 23:  /* HIPPOPOTAMUS */
        case 27:  /* ANT */
        case 34:  /* PRIMATE */
        case 41:  /* RODENT */
        case 45:  /* FELINE */
        case 46:  /* CANINE */
        case 47:  /* HORSE */
        case 48:  /* AMPHIB */
        case 50:  /* REPTILE */
        case 53:  /* OCTOPUS */
        case 54:  /* CRUSTACEAN */
        case 56:  /* BOVINE */
        case 57:  /* GOAT */
        case 58:  /* SHEEP */
        case 59:  /* DEER */
        case 60:  /* BEAR */
        case 61:  /* WEASEL */
        case 62:  /* SQUIRREL */
        case 63:  /* RABBIT */
        case 64:  /* BADGER */
        case 65:  /* OTTER */
        case 66:  /* BEAVER */
        case 67:  /* PIG */
        case 68:  /* BOAR */
        case 69:  /* TURTLE */
        case 70:  /* GIRAFFE */
        case 71:  /* CENTIPEDE */
        case 85:  /* LION */
        case 86:  /* TIGER */
        case 87:  /* LEOPARD */
        case 88:  /* COUGAR */
        case 89:  /* FROG */
        case 90:  /* ELEPHANT */
        case 91:  /* RHINO */
        case 94:  /* OX */
        case 107: /* GOPHER */
        case 110: /* BAT */
        case 119: /* PENGUIN */
        case 120: /* OSTRICH */
        case 125: /* FLYINSECT */
            return true;
        default:
            return false;
    }
}

/* Human is the deliberate baseline (no modifier) -- every other race's
 * bonus/penalty sums to zero, same convention as class_stat_bonus(). */
void race_stat_bonus(player_race_t r, attrs_t *a) {
    if (!a)
        return;
    switch (r) {
        case RACE_HUMAN:
            break; /* versatile baseline -- no modifier */
        case RACE_ELF:
            a->dexterity += 2;
            a->intelligence += 2;
            a->constitution -= 4;
            break;
        case RACE_OGRE:
            a->strength += 4;
            a->intelligence -= 2;
            a->charisma -= 2;
            break;
        case RACE_DWARF:
            a->constitution += 4;
            a->dexterity -= 2;
            a->charisma -= 2;
            break;
        case RACE_HOBBIT:
            a->dexterity += 4;
            a->strength -= 2;
            a->constitution -= 2;
            break;
        case RACE_GNOME:
            a->intelligence += 4;
            a->strength -= 2;
            a->constitution -= 2;
            break;
        default:
            break;
    }
}

/* Bucketed HP% description for `b` ("perfect", "hurt", "near death",
 * etc.) -- the classic MUD `diagnose`/`look` health line, done as
 * discrete tiers rather than a raw percentage so the game reads as
 * flavor text instead of a spreadsheet. */
const char *being_health_word(const being_t *b) {
    if (!b || b->progress.max_hp <= 0)
        return "unknown";
    int pct = (int)(((long)b->progress.hp * 100) / b->progress.max_hp);
    if (pct > 100)   return "heroic";   /* above max (future: buffs) */
    if (pct >= 100)  return "perfect";
    if (pct >= 90)   return "excellent";
    if (pct >= 75)   return "good";
    if (pct >= 60)   return "injured";
    if (pct >= 50)   return "hurt";
    if (pct >= 40)   return "wounded";
    if (pct >= 30)   return "bad";
    if (pct >= 20)   return "awful";
    if (pct >= 10)   return "horrid";
    return "near death";
}

/* Words for progress_t.hunger/thirst (0-100, -1 = immortal-immune), same
 * bucketing style as being_health_word() above. */
const char *being_hunger_word(int hunger) {
    if (hunger < 0)   return "immune";
    if (hunger >= 80) return "well fed";
    if (hunger >= 50) return "satisfied";
    if (hunger >= 25) return "getting hungry";
    if (hunger >= 1)  return "very hungry";
    return "starving";
}

const char *being_thirst_word(int thirst) {
    if (thirst < 0)   return "immune";
    if (thirst >= 80) return "quenched";
    if (thirst >= 50) return "not thirsty";
    if (thirst >= 25) return "getting thirsty";
    if (thirst >= 1)  return "very thirsty";
    return "parched";
}

/* Good/evil axis word for `score` (Session 43 continued, Mobile_Attitude
 * prerequisite) -- progress_t.alignment ranges -1000 (evil) to +1000
 * (good), 0 (neutral, the default for every character until something
 * sets it). Symmetric tiers around 0, same bucketing style as
 * being_health_word() above. */
const char *alignment_word(int alignment) {
    if (alignment >= 700)  return "saintly";
    if (alignment >= 350)  return "good";
    if (alignment > -350)  return "neutral";
    if (alignment > -700)  return "evil";
    return "demonic";
}

/* Display name for a limb_t (LIMB_NAMES[] above) -- falls back to
 * "body" for an out-of-range value. */
const char *limb_name(limb_t limb) {
    if (limb < 0 || limb >= LIMB_COUNT)
        return "body";
    return LIMB_NAMES[limb];
}

/* Floor on a limb's max HP (Session 42) -- without this, a fresh level-1
 * character's ~25 max HP split 13 ways rounds to 1 HP per limb, so any
 * landed hit (minimum damage 1) would instantly destroy whatever limb it
 * hit. With the floor, destroying a limb -- and decapitation in particular,
 * see combat.c's combat_sever_limb() -- takes a real run of hits even at
 * level 1, rather than a first-swing coin flip. */
#define LIMB_MIN_MAX_HP 15

/* (Re)initializes every limb's hp/max_hp for `b`, splitting max_hp
 * evenly across whichever limbs this body_type actually has (see the
 * body-types comment inside for how "has" is decided) and applying the
 * LIMB_MIN_MAX_HP floor above. Called on being creation and full heals
 * so limb HP always tracks a fresh source of truth rather than being
 * left stale after a max_hp change. */
void being_limbs_full_heal(being_t *b) {
    if (!b)
        return;
    /* Body types (2026-07-26): which limbs are actually PRESENT now
     * depends on b->body_type, not a fixed "everything before the EX_*
     * slots" boundary -- a BODY_SPIDER has real EX_* legs/feet and no
     * arms/wrists/hands at all, the exact reverse of a humanoid. A slot
     * with weight 0 for this body shape (body_limb_weight(), body.c) gets
     * 0/0 ("this being doesn't have this limb", being_has_limb()); only
     * present slots share overall HP evenly. */
    int present = 0;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (body_limb_weight(b->body_type, (limb_t)i) > 0)
            present++;
    if (present < 1)
        present = 1; /* never divide by zero for a body row that's somehow all-0 */
    int share = b->progress.max_hp / present;
    if (share < LIMB_MIN_MAX_HP)
        share = LIMB_MIN_MAX_HP;
    for (int i = 0; i < LIMB_COUNT; i++) {
        bool active = body_limb_weight(b->body_type, (limb_t)i) > 0;
        b->limbs[i].max_hp = active ? share : 0;
        b->limbs[i].hp = active ? share : 0;
    }
}

/* Whether `b`'s body actually includes `limb` -- driven by max_hp being
 * >0 (being_limbs_full_heal() zeroes both hp and max_hp for a slot the
 * current body_type doesn't have), so this doubles as the "is this a
 * real limb for this body shape" check used throughout combat/equip
 * code. */
bool being_has_limb(const being_t *b, limb_t limb) {
    if (!b || limb < 0 || limb >= LIMB_COUNT)
        return false;
    return b->limbs[limb].max_hp > 0;
}

/* Applies `dmg` to both `b`'s overall HP and the specific limb's HP
 * (clamped at 0) -- the shared damage-application point combat code
 * uses whenever a hit lands on a particular limb, keeping the two
 * pools in sync. */
void being_hurt_limb(being_t *b, limb_t limb, int dmg) {
    if (!b || dmg <= 0 || limb < 0 || limb >= LIMB_COUNT)
        return;
    b->progress.hp -= dmg;
    b->limbs[limb].hp -= dmg;
    if (b->limbs[limb].hp < 0)
        b->limbs[limb].hp = 0;
}

/* `limb`'s HP as a 0-100 percentage of its max, clamped -- feeds
 * limb_status_text() below and any limb-condition display. */
int being_limb_pct(const being_t *b, limb_t limb) {
    if (!b || limb < 0 || limb >= LIMB_COUNT || b->limbs[limb].max_hp <= 0)
        return 0;
    int pct = (b->limbs[limb].hp * 100) / b->limbs[limb].max_hp;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return pct;
}

/* Warning text for a limb at `pct` health, or NULL if it's healthy
 * enough not to mention -- used to append a "your arm needs medical
 * attention"-style note wherever a limb's condition is shown. */
const char *limb_status_text(int pct) {
    if (pct <= 0)
        return "is destroyed and needs medical attention";
    if (pct < 10)
        return "needs medical attention";
    if (pct < 20)
        return "is hurt rather badly";
    return NULL;
}

/* True if `b` has any present limb (being_has_limb()) currently at 0
 * HP -- deliberately excludes the always-0/0 EX_* slots a body_type
 * doesn't have, which are "absent," not "destroyed." Used to gate
 * decapitation/dismemberment-driven effects and status displays. */
bool being_has_destroyed_limb(const being_t *b) {
    if (!b)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++) {
        /* An always-inactive EX_* slot (max_hp<=0, being_has_limb()) sits
         * at hp=0 permanently -- that's "this being doesn't have this
         * limb", not a real destroyed one, and must never count here. */
        if (being_has_limb(b, (limb_t)i) && b->limbs[i].hp <= 0)
            return true;
    }
    return false;
}

/* Sums `b`'s effective armor class: every worn item's AC contribution
 * (obj_armor_ac() per equipment slot), plus the gamewide class/race AC
 * balance modifiers (same convention as being_calc_max_hp()), plus a
 * flat mounted bonus. Lower is better, matching the rest of the AC
 * scale. */
int being_total_ac(const being_t *b) {
    if (!b)
        return 0;
    int total = 0;
    for (int i = 0; i < LIMB_COUNT; i++)
        total += obj_armor_ac(b->equipment[i]);
    /* Gamewide AC modifier (user 2026-07-12's `balance` command) -- see
     * being_calc_max_hp()'s matching comment. */
    if (b->base.kind == THING_PC) {
        total += class_balance_get(b->char_class)->ac_mod;
        total += race_balance_get(b->race)->ac_mod;
    } else if (b->mob_class_known) {
        total += class_balance_get(b->char_class)->ac_mod;
    }
    /* Mounted AC bonus (Mount / riding system, Sneezy → Tobin feature
     * audit) -- lower is better, same scale as MOUNTED_ATTACK_BONUS
     * (combat.c). A mobility/positioning edge, not armor -- doesn't
     * stack with anything, just a flat improvement while in the saddle. */
    if (b->position == POSITION_MOUNTED)
        total -= 5;
    return total;
}

/* Right-aligned "<label>: <value>" column, one limb/hand per line (moved
 * here from cmd_object.c's cmd_equipment() 2026-07-12 so `look <person>`
 * can share it -- see being.h's doc comment). EQUIP_LABEL_WIDTH matches
 * the longest label ("secondary hold"). */
#define EQUIP_LABEL_WIDTH 14

static void equip_line(char *out, size_t out_sz, size_t *n, const char *label,
                        const struct obj *o) {
    const char *value = o
        ? (o->base.short_descr[0] ? o->base.short_descr : o->base.name)
        : "nothing";
    /* Condition (Object maintenance, Sneezy → Tobin feature audit) --
     * appended right after the short_descr, same convention cmd_object.c's
     * cmd_inventory() uses. */
    const char *cond = o ? obj_condition_word(o) : NULL;
    *n += (size_t)snprintf(out + *n, out_sz - *n, "  %*s: %s%s%s%s\r\n",
                           EQUIP_LABEL_WIDTH, label, value,
                           cond ? " (" : "", cond ? cond : "", cond ? ")" : "");
}

/* Renders `b`'s full equipment listing (one line per present limb slot
 * via equip_line() above, skipping genitalia and any limb this body
 * doesn't have, then both hold slots in handedness order) into `out` --
 * the shared implementation behind both `equipment` and `look <person>`
 * (see the comment above equip_line() for why it moved here). */
void being_render_equipment(const being_t *b, char *out, size_t out_sz, size_t *n) {
    if (!b)
        return;
    for (int i = 0; i < LIMB_COUNT && *n < out_sz; i++) {
        if (i == LIMB_GENITALIA || !being_has_limb(b, (limb_t)i))
            continue;
        equip_line(out, out_sz, n, limb_name((limb_t)i), b->equipment[i]);
    }
    int primary = b->handed_right ? 0 : 1;
    int secondary = b->handed_right ? 1 : 0;
    if (*n < out_sz)
        equip_line(out, out_sz, n, "primary hold", b->held[primary]);
    if (*n < out_sz)
        equip_line(out, out_sz, n, "secondary hold", b->held[secondary]);
}

/* The name to show for `b` in ordinary text: a mob's short_descr (its
 * "a large rat"-style article-including description) or a PC's plain
 * name. Not capitalized -- see being_display_name_cap() below for the
 * capitalized, buffer-returning variant used at the start of a
 * sentence. */
const char *being_display_name(const being_t *b) {
    if (!b)
        return "";
    return (b->base.kind == THING_MOB) ? b->base.short_descr : b->base.name;
}

/* Same idea as being_display_name() above, but capitalizes the first
 * real letter into `buf` and returns it -- used at the start of a
 * sentence (e.g. "Grendel hits you"). Skips over any leading `<x>`
 * color codes in a mob's short_descr so it capitalizes the actual
 * first letter of the name, not a color tag character. */
const char *being_display_name_cap(const being_t *b, char *buf, size_t bufsz) {
    if (!b || bufsz == 0)
        return "";
    if (b->base.kind != THING_MOB) {
        snprintf(buf, bufsz, "%s", b->base.name);
        return buf;
    }
    snprintf(buf, bufsz, "%s", b->base.short_descr);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

/* Heals `b` by `amount`, applying it to both overall HP and every
 * limb's HP (each clamped at its own max) -- the counterpart to
 * being_hurt_limb() for restoring HP across the board rather than to
 * one specific limb, e.g. a full heal spell or resting. */
void being_heal(being_t *b, int amount) {
    if (!b || amount <= 0)
        return;
    b->progress.hp += amount;
    if (b->progress.hp > b->progress.max_hp)
        b->progress.hp = b->progress.max_hp;
    for (int i = 0; i < LIMB_COUNT; i++) {
        b->limbs[i].hp += amount;
        if (b->limbs[i].hp > b->limbs[i].max_hp)
            b->limbs[i].hp = b->limbs[i].max_hp;
    }
}

/* Restores `amount` of vitality to `b`, clamped at max_vit -- the vit
 * counterpart to being_heal(), used by rest/regen and any effect that
 * restores stamina rather than HP. */
void being_heal_vit(being_t *b, int amount) {
    if (!b || amount <= 0)
        return;
    b->progress.vit += amount;
    if (b->progress.vit > b->progress.max_vit)
        b->progress.vit = b->progress.max_vit;
}

/* Deducts `amount` of vitality from `b`, clamped at 0 -- the spend
 * side of being_heal_vit(), used wherever a skill/spell costs vit to
 * perform. */
void being_spend_vit(being_t *b, int amount) {
    if (!b || amount <= 0)
        return;
    b->progress.vit -= amount;
    if (b->progress.vit < 0)
        b->progress.vit = 0;
}

/* Real upstream's XP-to-level curve (misc/gaining.cc's
 * getExpClassLevel()), precomputed for levels 1-50 despite the
 * misleading name -- its own code comment says it deliberately
 * replaced an OLDER system that had "huge arrays for each [class]"
 * with one shared formula, and `getExpClassLevel()` itself takes only
 * a level, no class parameter. User confirmed (2026-07-28, AskUserQuestion):
 * one shared curve for every class, matching real upstream, not a
 * deliberate per-class deviation. Index 0 unused (levels are 1-based);
 * index 50 is the real function's own explicit `MAX_MORT` special
 * case (a hardcoded 1,000,000,000, not a continuation of the curve).
 * Replaces the old level^2*100 placeholder. */
static const long XP_FOR_LEVEL[MORTAL_LEVEL_MAX + 1] = {
    0,                                          /* unused */
    0, 37, 343, 1259, 3326, 7133, 13524, 23616, 39454, 62573,
    95456, 141253, 205965, 293178, 409267, 562149, 767142, 1032166, 1371540, 1804231,
    2366004, 3073778, 3961145, 5068722, 6475372, 8216430, 10363864, 13003924, 16303179, 20334005,
    25245613, 31215707, 38584642, 47498296, 58258111, 71221052, 87064864, 106078633, 128858951, 156108600,
    189148553, 228544934, 275457160, 331245615, 398546218, 478368861, 572934506, 684840683, 818869564, 1000000000,
};

/* Works out how much total experience is needed to REACH a given
 * level, so progress_add_xp() can tell when someone has earned enough
 * to level up. */
long progress_xp_for_level(int level) {
    if (level <= 1)
        return 0;
    if (level >= MORTAL_LEVEL_MAX)
        return XP_FOR_LEVEL[MORTAL_LEVEL_MAX];
    return XP_FOR_LEVEL[level];
}

/* Adds `xp_gain` experience to `p` and levels it up as many times as
 * the new total allows (capped at the mortal max level). Returns how
 * many levels were gained, so the caller knows whether to celebrate.
 * Only touches level/experience -- it works on a bare progress_t with
 * no access to the being's attributes, so it deliberately does NOT
 * recompute max_hp or heal limbs on its own; the caller (combat.c's
 * combat_defeat()) does that with the full being_t once it sees
 * levels_gained > 0. */
int progress_add_xp(progress_t *p, long xp_gain) {
    if (!p || xp_gain <= 0)
        return 0;

    p->experience += xp_gain;

    int levels_gained = 0;
    while (p->level < MORTAL_LEVEL_MAX && p->experience >= progress_xp_for_level(p->level + 1)) {
        p->level++;
        levels_gained++;
    }
    return levels_gained;
}
