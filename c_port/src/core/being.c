/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "being.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#include "balance.h"
#include "descriptor.h"
#include "gametime.h"
#include "log.h"
#include "mob_repo.h"
#include "obj.h"
#include "room.h"

int *attrs_field(attrs_t *a, const char *tok) {
    if (strcasecmp(tok, "str") == 0 || strcasecmp(tok, "strength") == 0) return &a->strength;
    if (strcasecmp(tok, "dex") == 0 || strcasecmp(tok, "dexterity") == 0) return &a->dexterity;
    if (strcasecmp(tok, "con") == 0 || strcasecmp(tok, "constitution") == 0) return &a->constitution;
    if (strcasecmp(tok, "int") == 0 || strcasecmp(tok, "intelligence") == 0) return &a->intelligence;
    if (strcasecmp(tok, "wis") == 0 || strcasecmp(tok, "wisdom") == 0) return &a->wisdom;
    if (strcasecmp(tok, "cha") == 0 || strcasecmp(tok, "charisma") == 0) return &a->charisma;
    return NULL;
}

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
    b->position = POSITION_STANDING;
    b->progress.level = MORTAL_LEVEL_MIN;
    b->progress.experience = 0;
    b->progress.max_hp = being_calc_max_hp(b);
    b->progress.hp = b->progress.max_hp;
    being_limbs_full_heal(b);
    b->progress.hunger = 100;
    b->progress.thirst = 100;
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
static bool mob_class_mask_to_tobin(int mask, player_class_t *out) {
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

bool being_is_immortal(const being_t *b) {
    return b && b->progress.level >= IMMORTAL_LEVEL_MIN;
}

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

void being_normalize_name(char *name) {
    if (!name || !name[0])
        return;
    name[0] = (char)toupper((unsigned char)name[0]);
    for (int i = 1; name[i]; i++)
        name[i] = (char)tolower((unsigned char)name[i]);
}

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

int being_get_wait(const being_t *b) {
    if (!b)
        return 0;
    return being_is_immortal(b) ? 0 : b->wait_pulses;
}

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
static double class_hp_scale(player_class_t c) {
    switch (c) {
        case CLASS_WARRIOR: return 1.3;
        case CLASS_MONK:    return 1.15;
        case CLASS_DRUID:   return 1.0;
        case CLASS_CLERIC:  return 1.0;
        case CLASS_THIEF:   return 0.9;
        case CLASS_MAGE:    return 0.8;
        default:            return 1.0;
    }
}

int being_calc_max_hp(const being_t *b) {
    if (!b)
        return 20;
    int con_bonus = b->attrs.constitution - ATTR_BASE;
    double scale = 1.0;
    /* Gamewide HP multiplier (user 2026-07-12's `balance` command) --
     * a PC's own class+race, or a guildmaster mob's known class (no
     * race applies to mobs). Neutral (1.0) until an immortal actually
     * balances that class/race, so this is a no-op by default. */
    if (b->base.kind == THING_PC) {
        scale = class_hp_scale(b->char_class);
        scale *= class_balance_get(b->char_class)->hp_mult;
        scale *= race_balance_get(b->race)->hp_mult;
    } else if (b->mob_class_known) {
        scale *= class_balance_get(b->char_class)->hp_mult;
    }
    return 20 + con_bonus + (int)(b->progress.level * 5 * scale);
}

static const char *LIMB_NAMES[LIMB_COUNT] = {
    "head", "neck", "left arm", "right arm", "left finger", "right finger",
    "body", "waist", "genitalia", "right leg", "left leg", "left foot", "right foot",
};

/* position_types[] (misc/being.cc), indexed by position_t. */
static const char *const POSITION_NAMES[] = {
    "Dead", "Mortally wounded", "Incapacitated", "Stunned", "Sleeping",
    "Resting", "Sitting", "Engaged", "Fighting", "Crawling", "Standing",
    "Mounted", "Flying",
};

const char *position_name(position_t p) {
    if (p < 0 || (size_t)p >= sizeof(POSITION_NAMES) / sizeof(POSITION_NAMES[0]))
        return "Standing";
    return POSITION_NAMES[p];
}

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

const char *class_name(player_class_t c) {
    if (c < 0 || c >= CLASS_COUNT)
        return "Mage";
    return CLASS_NAMES[c];
}

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

void being_limbs_full_heal(being_t *b) {
    if (!b)
        return;
    int share = b->progress.max_hp / LIMB_COUNT;
    if (share < LIMB_MIN_MAX_HP)
        share = LIMB_MIN_MAX_HP;
    for (int i = 0; i < LIMB_COUNT; i++) {
        b->limbs[i].max_hp = share;
        b->limbs[i].hp = share;
    }
}

void being_hurt_limb(being_t *b, limb_t limb, int dmg) {
    if (!b || dmg <= 0 || limb < 0 || limb >= LIMB_COUNT)
        return;
    b->progress.hp -= dmg;
    b->limbs[limb].hp -= dmg;
    if (b->limbs[limb].hp < 0)
        b->limbs[limb].hp = 0;
}

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

const char *limb_status_text(int pct) {
    if (pct <= 0)
        return "is destroyed and needs medical attention";
    if (pct < 10)
        return "needs medical attention";
    if (pct < 20)
        return "is hurt rather badly";
    return NULL;
}

bool being_has_destroyed_limb(const being_t *b) {
    if (!b)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++) {
        if (b->limbs[i].hp <= 0)
            return true;
    }
    return false;
}

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
    *n += (size_t)snprintf(out + *n, out_sz - *n, "  %*s: %s\r\n",
                           EQUIP_LABEL_WIDTH, label, value);
}

void being_render_equipment(const being_t *b, char *out, size_t out_sz, size_t *n) {
    if (!b)
        return;
    for (int i = 0; i < LIMB_COUNT && *n < out_sz; i++) {
        if (i == LIMB_GENITALIA)
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

const char *being_display_name(const being_t *b) {
    if (!b)
        return "";
    return (b->base.kind == THING_MOB) ? b->base.short_descr : b->base.name;
}

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

/* Works out how much total experience is needed to REACH a given
 * level, so progress_add_xp() can tell when someone has earned enough
 * to level up. */
long progress_xp_for_level(int level) {
    return (long)level * (long)level * 100;
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
