/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "obj.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"
#include "material.h"
#include "obj_magic_repo.h"
#include "obj_repo.h"
#include "room.h"
#include "thing.h"
#include "world.h"

static const char *const OBJ_CATEGORY_NAMES[OBJ_CAT_COUNT] = {
    "light", "weapon", "ammo", "armor", "container", "drink", "food",
    "money", "key", "magic device", "tool", "furniture", "treasure",
    "written", "trash", "other",
};

/* Display name for a Tobin obj_category_t (OBJ_CATEGORY_NAMES[] above)
 * -- falls back to "other" for an out-of-range value. */
const char *obj_category_name(obj_category_t cat) {
    if (cat < 0 || cat >= OBJ_CAT_COUNT)
        return "other";
    return OBJ_CATEGORY_NAMES[cat];
}

/* Collapses the original's 60 itemTypeT values (misc/obj.h, verbatim
 * upstream 0-based numbering -- confirmed by reading the enum directly, not
 * reassigned anywhere) into Tobin's 16 obj_category_t buckets. Indexed
 * directly by the DB `obj.type` column; a handful of types that don't fit
 * neatly anywhere gameplay-relevant yet (corpses, quest components, raw
 * materials, ...) land in TRASH/OTHER as a placeholder, same precedent as
 * sector_color()'s keyword bucketing. */
static const obj_category_t ITEM_TYPE_CATEGORY[] = {
    OBJ_CAT_OTHER,        /* 0  ITEM_UNDEFINED */
    OBJ_CAT_LIGHT,        /* 1  ITEM_LIGHT */
    OBJ_CAT_MAGIC_DEVICE, /* 2  ITEM_SCROLL */
    OBJ_CAT_MAGIC_DEVICE, /* 3  ITEM_WAND */
    OBJ_CAT_MAGIC_DEVICE, /* 4  ITEM_STAFF */
    OBJ_CAT_WEAPON,       /* 5  ITEM_WEAPON */
    OBJ_CAT_OTHER,        /* 6  ITEM_FUEL */
    OBJ_CAT_TREASURE,     /* 7  ITEM_OPAL */
    OBJ_CAT_TREASURE,     /* 8  ITEM_TREASURE */
    OBJ_CAT_ARMOR,        /* 9  ITEM_ARMOR */
    OBJ_CAT_MAGIC_DEVICE, /* 10 ITEM_POTION */
    OBJ_CAT_ARMOR,        /* 11 ITEM_WORN */
    OBJ_CAT_OTHER,        /* 12 ITEM_OTHER */
    OBJ_CAT_TRASH,        /* 13 ITEM_TRASH */
    OBJ_CAT_OTHER,        /* 14 ITEM_TRAP */
    OBJ_CAT_CONTAINER,    /* 15 ITEM_CHEST */
    OBJ_CAT_WRITTEN,      /* 16 ITEM_NOTE */
    OBJ_CAT_DRINK,        /* 17 ITEM_DRINKCON */
    OBJ_CAT_KEY,          /* 18 ITEM_KEY */
    OBJ_CAT_FOOD,         /* 19 ITEM_FOOD */
    OBJ_CAT_MONEY,        /* 20 ITEM_MONEY */
    OBJ_CAT_TOOL,         /* 21 ITEM_PEN */
    OBJ_CAT_FURNITURE,    /* 22 ITEM_BOAT */
    OBJ_CAT_WRITTEN,      /* 23 ITEM_AUDIO */
    OBJ_CAT_WRITTEN,      /* 24 ITEM_BOARD */
    OBJ_CAT_WEAPON,       /* 25 ITEM_BOW */
    OBJ_CAT_AMMO,         /* 26 ITEM_ARROW */
    OBJ_CAT_CONTAINER,    /* 27 ITEM_BAG */
    OBJ_CAT_TRASH,        /* 28 ITEM_CORPSE */
    OBJ_CAT_CONTAINER,    /* 29 ITEM_SPELLBAG */
    OBJ_CAT_OTHER,        /* 30 ITEM_COMPONENT */
    OBJ_CAT_WRITTEN,      /* 31 ITEM_BOOK */
    OBJ_CAT_OTHER,        /* 32 ITEM_PORTAL */
    OBJ_CAT_FURNITURE,    /* 33 ITEM_WINDOW */
    OBJ_CAT_FURNITURE,    /* 34 ITEM_TREE */
    OBJ_CAT_TOOL,         /* 35 ITEM_TOOL */
    OBJ_CAT_TOOL,         /* 36 ITEM_HOLY_SYM */
    OBJ_CAT_CONTAINER,    /* 37 ITEM_QUIVER */
    OBJ_CAT_TOOL,         /* 38 ITEM_BANDAGE */
    OBJ_CAT_FURNITURE,    /* 39 ITEM_STATUE */
    OBJ_CAT_FURNITURE,    /* 40 ITEM_BED */
    OBJ_CAT_FURNITURE,    /* 41 ITEM_TABLE */
    OBJ_CAT_OTHER,        /* 42 ITEM_RAW_MATERIAL */
    OBJ_CAT_TREASURE,     /* 43 ITEM_GEMSTONE */
    OBJ_CAT_WEAPON,       /* 44 ITEM_MARTIAL_WEAPON */
    OBJ_CAT_TREASURE,     /* 45 ITEM_JEWELRY */
    OBJ_CAT_DRINK,        /* 46 ITEM_VIAL */
    OBJ_CAT_TRASH,        /* 47 ITEM_PCORPSE */
    OBJ_CAT_DRINK,        /* 48 ITEM_POOL */
    OBJ_CAT_CONTAINER,    /* 49 ITEM_KEYRING */
    OBJ_CAT_OTHER,        /* 50 ITEM_RAW_ORGANIC */
    OBJ_CAT_LIGHT,        /* 51 ITEM_FLAME */
    OBJ_CAT_OTHER,        /* 52 ITEM_APPLIED_SUB */
    OBJ_CAT_OTHER,        /* 53 ITEM_GAS */
    OBJ_CAT_MAGIC_DEVICE, /* 54 ITEM_ARMOR_WAND */
    OBJ_CAT_CONTAINER,    /* 55 ITEM_DRUG_CONTAINER */
    OBJ_CAT_MAGIC_DEVICE, /* 56 ITEM_DRUG */
    OBJ_CAT_WEAPON,       /* 57 ITEM_GUN */
    OBJ_CAT_AMMO,         /* 58 ITEM_AMMO */
    OBJ_CAT_OTHER,        /* 59 ITEM_PLANT */
    OBJ_CAT_TOOL,         /* 60 ITEM_COOKWARE */
    OBJ_CAT_FURNITURE,    /* 61 ITEM_VEHICLE */
    OBJ_CAT_TREASURE,     /* 62 ITEM_CASINO_CHIP */
    OBJ_CAT_MAGIC_DEVICE, /* 63 ITEM_POISON */
    OBJ_CAT_WEAPON,       /* 64 ITEM_HANDGONNE */
    OBJ_CAT_OTHER,        /* 65 ITEM_EGG */
    OBJ_CAT_WEAPON,       /* 66 ITEM_CANNON */
    OBJ_CAT_TREASURE,     /* 67 ITEM_TOOTH_NECKLACE */
    OBJ_CAT_TRASH,        /* 68 ITEM_TRASH_PILE */
    OBJ_CAT_WRITTEN,      /* 69 ITEM_CARD_DECK */
    OBJ_CAT_CONTAINER,    /* 70 ITEM_SUITCASE */
    OBJ_CAT_FURNITURE,    /* 71 ITEM_SADDLE */
    OBJ_CAT_FURNITURE,    /* 72 ITEM_HARNESS */
    OBJ_CAT_CONTAINER,    /* 73 ITEM_SADDLEBAG */
    OBJ_CAT_FURNITURE,    /* 74 ITEM_WAGON */
    OBJ_CAT_CONTAINER,    /* 75 ITEM_MONEYPOUCH */
    OBJ_CAT_FOOD,         /* 76 ITEM_FRUIT */
};
#define NUM_ITEM_TYPES (sizeof(ITEM_TYPE_CATEGORY) / sizeof(ITEM_TYPE_CATEGORY[0]))

/* Collapses a raw upstream `obj.type` value into Tobin's obj_category_t
 * via ITEM_TYPE_CATEGORY[] above -- the one place a newly loaded/created
 * object's gameplay category gets decided (see obj_create_from_proto()
 * below). Falls back to OBJ_CAT_OTHER for an out-of-range value. */
obj_category_t category_for_item_type(int orig_item_type) {
    if (orig_item_type < 0 || (size_t)orig_item_type >= NUM_ITEM_TYPES)
        return OBJ_CAT_OTHER;
    return ITEM_TYPE_CATEGORY[orig_item_type];
}

/* The original's ITEM_WEAR_* bit layout (misc/obj.h), kept verbatim so
 * every already-seeded object's wear_flag column "just works" -- see
 * obj_t's field comment. */
#define WEAR_TAKE    1
#define WEAR_FINGERS 2
#define WEAR_NECK    4
#define WEAR_BODY    8
#define WEAR_HEAD    16
#define WEAR_LEGS    32
#define WEAR_FEET    64
#define WEAR_HANDS   128
#define WEAR_ARMS    256
#define WEAR_BACK    1024
#define WEAR_WAIST   2048
#define WEAR_WRISTS  4096
#define WEAR_HOLD    16384
#define WEAR_THROW   32768

/* Whether an object with `wear_flag` can be picked up at all (the
 * WEAR_TAKE bit) -- most objects have it, but a handful of fixed-in-
 * place items (furniture, scenery) don't. */
bool obj_takeable(int wear_flag) {
    return (wear_flag & WEAR_TAKE) != 0;
}

/* Bit-position-indexed names for the WEAR_* layout above -- "UNUSED" for
 * the original's own two never-assigned bits (9 and 13), kept so a
 * historic wear_flag value that happens to set one still reports
 * something rather than silently vanishing from the readable form. */
static const char *const WEAR_FLAG_NAMES[16] = {
    "TAKE", "FINGERS", "NECK", "BODY", "HEAD", "LEGS", "FEET", "HANDS",
    "ARMS", "UNUSED", "BACK", "WAIST", "WRISTS", "UNUSED", "HOLD", "THROW",
};

/* Formats every set WEAR_* bit in `flags` as a "[ NAME ]"-joined string
 * into `buf` (WEAR_FLAG_NAMES[] above), or "none" if nothing's set --
 * same convention as mob_action_names() (mob_ai.c), used by `stat`/
 * debug output to show an object's raw wear_flag in readable form. */
const char *obj_wear_flag_names(int flags, char *buf, size_t size) {
    size_t n = 0;
    buf[0] = '\0';
    for (int bit = 0; bit < 16; bit++) {
        if (!(flags & (1 << bit)))
            continue;
        n += (size_t)snprintf(buf + n, size > n ? size - n : 0, "%s[ %s ]",
                              n > 0 ? " " : "", WEAR_FLAG_NAMES[bit]);
        if (n >= size)
            break;
    }
    if (buf[0] == '\0')
        snprintf(buf, size, "none");
    return buf;
}

/* The original's itemTypeT enum (misc/obj.h, 77 entries, ITEM_UNDEFINED=0
 * .. ITEM_FRUIT=76), verbatim upstream order -- same table
 * category_for_item_type() above collapses into Tobin's 16 obj_category_t
 * buckets, but `stat` (user 2026-07-12: "stat obj, get names for type,
 * action_flag") wants the real seeded value decoded honestly, not the
 * collapsed category. "ITEM_" prefix stripped, same convention as
 * mob_race_name()'s "RACE_"-stripped table (being.c). */
static const char *const ITEM_TYPE_NAMES[NUM_ITEM_TYPES] = {
    "UNDEFINED", "LIGHT", "SCROLL", "WAND", "STAFF", "WEAPON", "FUEL",
    "OPAL", "TREASURE", "ARMOR", "POTION", "WORN", "OTHER", "TRASH",
    "TRAP", "CHEST", "NOTE", "DRINKCON", "KEY", "FOOD", "MONEY", "PEN",
    "BOAT", "AUDIO", "BOARD", "BOW", "ARROW", "BAG", "CORPSE", "SPELLBAG",
    "COMPONENT", "BOOK", "PORTAL", "WINDOW", "TREE", "TOOL", "HOLY_SYM",
    "QUIVER", "BANDAGE", "STATUE", "BED", "TABLE", "RAW_MATERIAL",
    "GEMSTONE", "MARTIAL_WEAPON", "JEWELRY", "VIAL", "PCORPSE", "POOL",
    "KEYRING", "RAW_ORGANIC", "FLAME", "APPLIED_SUB", "GAS", "ARMOR_WAND",
    "DRUG_CONTAINER", "DRUG", "GUN", "AMMO", "PLANT", "COOKWARE",
    "VEHICLE", "CASINO_CHIP", "POISON", "HANDGONNE", "EGG", "CANNON",
    "TOOTH_NECKLACE", "TRASH_PILE", "CARD_DECK", "SUITCASE", "SADDLE",
    "HARNESS", "SADDLEBAG", "WAGON", "MONEYPOUCH", "FRUIT",
};

/* Display name for the raw upstream `obj.type` value (ITEM_TYPE_NAMES[]
 * above, the honest un-collapsed 77-entry enum) -- for `stat`, which
 * wants to show the real seeded type, not the collapsed
 * obj_category_t category_for_item_type() derives from it. Returns "?"
 * for an out-of-range value. */
const char *obj_type_name(int raw_type) {
    if (raw_type < 0 || (size_t)raw_type >= NUM_ITEM_TYPES)
        return "?";
    return ITEM_TYPE_NAMES[raw_type];
}

int obj_item_type_count(void) {
    return (int)NUM_ITEM_TYPES;
}

/* The original's extraFlags bitmask (misc/obj.h, 32 bits, ITEM_GLOW=bit0
 * .. ITEM_NOLOCATE=bit31) -- Tobin's DB column is `action_flag`. A
 * handful of bits were never assigned upstream (27, "NOT_USED3") or
 * later repurposed as generic scratch space (25, "NOJUNK_PLAYER"); kept
 * verbatim rather than renamed/dropped so a historic seeded value still
 * reports something recognizable. `flags` is cast to unsigned so bit 31
 * (ITEM_NOLOCATE) decodes correctly even though the DB column itself is
 * a signed int. */
static const char *const OBJ_ACTION_FLAG_NAMES[32] = {
    "GLOW", "HUM", "STRUNG", "SHADOWY", "PROTOTYPE", "INVISIBLE", "MAGIC",
    "NODROP", "BLESS", "SPIKED", "HOVER", "RUSTY", "ANTI_CLERIC",
    "ANTI_MAGE", "ANTI_THIEF", "ANTI_WARRIOR", "ANTI_SHAMAN",
    "ANTI_DEIKHAN", "ANTI_RANGER", "ANTI_MONK", "PAIRED", "NORENT",
    "FLOAT", "NOPURGE", "NEWBIE", "NOJUNK_PLAYER", "SILVERED",
    "NOT_USED3", "ATTACHED", "BURNING", "CHARRED", "NOLOCATE",
};

/* Formats every set action_flag bit in `flags` as a "[ NAME ]"-joined
 * string into `buf` (OBJ_ACTION_FLAG_NAMES[] above), or "none" if
 * nothing's set -- same convention as obj_wear_flag_names() above, for
 * `stat`/debug display of an object's raw extraFlags bitmask. */
const char *obj_action_flag_names(int flags, char *buf, size_t size) {
    size_t n = 0;
    buf[0] = '\0';
    unsigned int uflags = (unsigned int)flags;
    for (int bit = 0; bit < 32; bit++) {
        if (!(uflags & (1u << bit)))
            continue;
        n += (size_t)snprintf(buf + n, size > n ? size - n : 0, "%s[ %s ]",
                              n > 0 ? " " : "", OBJ_ACTION_FLAG_NAMES[bit]);
        if (n >= size)
            break;
    }
    if (buf[0] == '\0')
        snprintf(buf, size, "none");
    return buf;
}

/* obj_wear_flag_count()/name() and obj_action_flag_count()/name() below
 * are the per-bit accessors behind obj_wear_flag_names()/
 * obj_action_flag_names() above -- for a caller (e.g. an immortal edit
 * menu toggling one flag at a time) that wants to iterate bits
 * individually rather than get the whole joined string at once. */
int obj_wear_flag_count(void) {
    return 16;
}

const char *obj_wear_flag_name(int bit) {
    if (bit < 0 || bit >= 16)
        return "?";
    return WEAR_FLAG_NAMES[bit];
}

int obj_action_flag_count(void) {
    return 32;
}

const char *obj_action_flag_name(int bit) {
    if (bit < 0 || bit >= 32)
        return "?";
    return OBJ_ACTION_FLAG_NAMES[bit];
}

/* Object anti-race flags (TODO.md priority item, 2026-08-02) -- a
 * Tobin-only bitmask, no upstream equivalent (see obj.h's own doc
 * comment on why this is a separate field from action_flag). Bit order
 * matches player_race_t (being.h) 1:1. */
static const char *const OBJ_ANTI_RACE_FLAG_NAMES[6] = {
    "HUMAN", "ELF", "OGRE", "DWARF", "HOBBIT", "GNOME",
};

const char *obj_anti_race_flag_names(int flags, char *buf, size_t size) {
    size_t n = 0;
    buf[0] = '\0';
    unsigned int uflags = (unsigned int)flags;
    for (int bit = 0; bit < 6; bit++) {
        if (!(uflags & (1u << bit)))
            continue;
        n += (size_t)snprintf(buf + n, size > n ? size - n : 0, "%s[ %s ]",
                              n > 0 ? " " : "", OBJ_ANTI_RACE_FLAG_NAMES[bit]);
        if (n >= size)
            break;
    }
    if (buf[0] == '\0')
        snprintf(buf, size, "none");
    return buf;
}

int obj_anti_race_flag_count(void) {
    return 6;
}

const char *obj_anti_race_flag_name(int bit) {
    if (bit < 0 || bit >= 6)
        return "?";
    return OBJ_ANTI_RACE_FLAG_NAMES[bit];
}

/* True iff `race` (a player_race_t) is barred from wearing/wielding/
 * holding `o` -- checked by cmd_wear.c's wear_one_item()/
 * do_hold_or_wield(). Bit order matches player_race_t exactly, so this
 * is a direct shift-and-test, same convention as mob_race_is_animal(). */
bool obj_race_forbidden(const obj_t *o, int race) {
    if (race < 0 || race >= 6)
        return false;
    return (o->anti_race_flag & (1 << race)) != 0;
}

/* Picks which limb slot on `fitter` an object with `wear_flag` should
 * go into when worn -- tries each wearable bit the object has in a
 * fixed priority order, returning the first still-empty matching slot.
 * Returns WEAR_SLOT_HELD for a holdable item, WEAR_SLOT_NO_ROOM if
 * every matching slot is already full, or WEAR_SLOT_NOT_WEARABLE if
 * the flag has no wearable bits (or `fitter` is NULL) at all. */
int wear_slot_for_flag(int wear_flag, const struct being *fitter) {
    if (!fitter)
        return WEAR_SLOT_NOT_WEARABLE;

    if (wear_flag & WEAR_HOLD)
        return WEAR_SLOT_HELD;

    bool matched = false;

    if (wear_flag & WEAR_HEAD) {
        matched = true;
        if (!fitter->equipment[LIMB_HEAD]) return LIMB_HEAD;
    }
    if (wear_flag & WEAR_NECK) {
        matched = true;
        if (!fitter->equipment[LIMB_NECK]) return LIMB_NECK;
    }
    if (wear_flag & WEAR_BODY) {
        matched = true;
        if (!fitter->equipment[LIMB_BODY]) return LIMB_BODY;
    }
    if (wear_flag & WEAR_WAIST) {
        matched = true;
        if (!fitter->equipment[LIMB_WAIST]) return LIMB_WAIST;
    }
    if (wear_flag & WEAR_ARMS) {
        matched = true;
        if (!fitter->equipment[LIMB_RIGHT_ARM]) return LIMB_RIGHT_ARM;
        if (!fitter->equipment[LIMB_LEFT_ARM]) return LIMB_LEFT_ARM;
    }
    /* BACK/WRISTS/HANDS (Limbs -> wearSlotT, 2026-07-26): these bits
     * already existed verbatim on real seeded objects (obj_t's own field
     * comment) but had no Tobin limb to map to until now. */
    if (wear_flag & WEAR_BACK) {
        matched = true;
        if (!fitter->equipment[LIMB_BACK]) return LIMB_BACK;
    }
    if (wear_flag & WEAR_WRISTS) {
        matched = true;
        if (!fitter->equipment[LIMB_RIGHT_WRIST]) return LIMB_RIGHT_WRIST;
        if (!fitter->equipment[LIMB_LEFT_WRIST]) return LIMB_LEFT_WRIST;
    }
    if (wear_flag & WEAR_HANDS) {
        matched = true;
        if (!fitter->equipment[LIMB_RIGHT_HAND]) return LIMB_RIGHT_HAND;
        if (!fitter->equipment[LIMB_LEFT_HAND]) return LIMB_LEFT_HAND;
    }
    if (wear_flag & WEAR_FINGERS) {
        matched = true;
        if (!fitter->equipment[LIMB_RIGHT_FINGER]) return LIMB_RIGHT_FINGER;
        if (!fitter->equipment[LIMB_LEFT_FINGER]) return LIMB_LEFT_FINGER;
    }
    if (wear_flag & WEAR_LEGS) {
        matched = true;
        if (!fitter->equipment[LIMB_RIGHT_LEG]) return LIMB_RIGHT_LEG;
        if (!fitter->equipment[LIMB_LEFT_LEG]) return LIMB_LEFT_LEG;
    }
    if (wear_flag & WEAR_FEET) {
        matched = true;
        if (!fitter->equipment[LIMB_RIGHT_FOOT]) return LIMB_RIGHT_FOOT;
        if (!fitter->equipment[LIMB_LEFT_FOOT]) return LIMB_LEFT_FOOT;
    }

    if (matched)
        return WEAR_SLOT_NO_ROOM;

    /* THROW/reserved bits only, as of the Limbs -> wearSlotT reshape
     * (2026-07-26) -- HANDS/WRISTS/BACK are now real Tobin limbs, mapped
     * above. THROW has no Tobin throwing-weapon mechanic; carriable, not
     * wearable. */
    return WEAR_SLOT_NOT_WEARABLE;
}

/* Allocates and initializes a new object instance from its vnum
 * prototype (obj_proto_load()) -- copies name/description/wear_flag/
 * vals/weight/etc. straight across, derives its obj_category_t via
 * category_for_item_type(), and applies a material-tier structural
 * bonus so a higher-tier material instance starts tougher (and fully
 * undamaged) rather than copying the prototype's raw struct values
 * verbatim. Caller owns the returned obj_t. */
obj_t *obj_create_from_proto(int vnum) {
    obj_proto_t proto;
    if (!obj_proto_load(vnum, &proto))
        return NULL;

    obj_t *o = calloc(1, sizeof(*o));
    if (!o)
        return NULL;

    o->base.kind = THING_OBJ;
    o->base.id = vnum;
    snprintf(o->base.name, sizeof(o->base.name), "%s", proto.name);
    snprintf(o->base.short_descr, sizeof(o->base.short_descr), "%s", proto.short_descr);
    snprintf(o->long_descr, sizeof(o->long_descr), "%s", proto.long_descr);

    o->vnum = vnum;
    o->category = category_for_item_type(proto.type);
    o->raw_type = proto.type;
    o->wear_flag = proto.wear_flag;
    o->action_flag = proto.action_flag;
    o->anti_race_flag = proto.anti_race_flag;
    for (int i = 0; i < 4; i++)
        o->val[i] = proto.val[i];
    o->weight = proto.weight;
    o->volume = proto.volume;
    o->price = proto.price;
    o->max_struct = proto.max_struct;
    o->cur_struct = proto.cur_struct;
    o->material = proto.material;
    o->can_be_seen = proto.can_be_seen;
    /* Weapon sharpness -- not persisted (see obj.h's own doc comment on
     * obj_t.sharpness), so every freshly-spawned instance (including a
     * reload after reconnect) simply starts back at the default. */
    o->sharpness = SHARPNESS_DEFAULT;
    o->decay_time = proto.decay_time;

    /* Material property system (Sneezy → Tobin feature audit) -- a
     * higher-tier material makes a freshly-created instance tougher from
     * the start (material.h's material_tier_struct_bonus()), added to
     * both halves so it starts undamaged, not already "hurt" relative to
     * its new ceiling. */
    if (o->max_struct > 0) {
        int bonus = material_tier_struct_bonus(material_tier_for_id(o->material));
        o->max_struct += bonus;
        o->cur_struct += bonus;
    }

    /* Magic items (Sneezy -> Tobin feature audit): cache this instance's
     * real objaffect-sourced stat/AC/HP/Vitality bonuses once, here, at
     * creation -- see obj_repo.h's obj_load_stat_affects() and obj_t's
     * own doc comment for why this is cached rather than re-queried. */
    obj_load_stat_affects(vnum, &o->aff_str, &o->aff_dex, &o->aff_con,
                          &o->aff_intel, &o->aff_wis, &o->aff_cha,
                          &o->aff_hit, &o->aff_move, &o->aff_ac);

    /* A scroll/wand/staff's charges (obj_magic.sql) start fresh every
     * time an instance is created -- same "resets on every relog/reload,
     * no per-instance persistence yet" limitation spell components/holy
     * symbols already have (player_inventory's flat vnum+slot schema has
     * no per-instance val[] column). val[0]=current, val[1]=max, same
     * convention those items use. Overrides whatever the raw obj.val0/1
     * columns held (0 for the seeded examples; unreliable import noise
     * on any other magic-device row -- see obj_magic.sql's own comment). */
    char spell_name[OBJ_MAGIC_SPELL_NAME_LEN];
    int max_charges;
    if (obj_magic_repo_get(vnum, spell_name, sizeof(spell_name), &max_charges)) {
        o->val[1] = max_charges;
        o->val[0] = max_charges;
    }

    return o;
}

/* Allocates a temporary, no-prototype object (vnum 0, e.g. a corpse or
 * a blood/pee puddle) built directly from the given text and category
 * rather than loaded from a DB prototype -- takeable and non-decaying
 * by default, with the caller expected to override wear_flag/
 * decay_time right after creation for anything that shouldn't behave
 * like an ordinary item (see the field comments below). Caller owns
 * the returned obj_t. */
obj_t *obj_create_ephemeral(const char *name, const char *short_descr,
                            const char *long_descr, obj_category_t category) {
    obj_t *o = calloc(1, sizeof(*o));
    if (!o)
        return NULL;

    o->base.kind = THING_OBJ;
    o->base.id = 0;
    snprintf(o->base.name, sizeof(o->base.name), "%s", name);
    snprintf(o->base.short_descr, sizeof(o->base.short_descr), "%s", short_descr);
    snprintf(o->long_descr, sizeof(o->long_descr), "%s", long_descr);

    o->vnum = 0;
    o->category = category;
    o->wear_flag = WEAR_TAKE;
    o->weight = 2.0;
    o->can_be_seen = true;
    o->decay_time = -1; /* never decays by default -- callers that want a
                          * temporary object (corpses, severed limbs --
                          * combat.c) set a real countdown explicitly right
                          * after creation, same post-creation-field-set
                          * precedent as wear_flag/val[] above. */

    return o;
}

/* Whole-keyword case-insensitive match (not a prefix match like
 * cmd_object.c's obj_name_matches() -- `type_tag` is always an exact,
 * known keyword here, e.g. "pee"/"blood", not player-typed input). */
static bool pool_keyword_matches(const char *keywords, const char *tag) {
    size_t tag_len = strlen(tag);
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen == tag_len && strncasecmp(start, tag, tag_len) == 0)
            return true;
    }
    return false;
}

/* "a puddle of blood" -> "a pool of blood" -> "a large pool of blood" as
 * val[0] (the growth counter) climbs -- same word-tier-bucketing style as
 * alignment_word()/limb_status_text(). */
static const char *pool_size_phrase(int size) {
    if (size >= 4)
        return "a large pool of";
    if (size >= 2)
        return "a pool of";
    return "a puddle of";
}

/* Colorizes the substance noun (user, 2026-07-11: "pee blood x4 should
 * create A large pool of <R>blood<z> is here."): dim for a puddle/pool,
 * bright once it's grown into a "large pool" -- the color escalates with
 * the size tier the same way the wording does. Falls back to plain white
 * for any noun besides the two that exist today. */
static char pool_noun_color(const char *noun, int size) {
    char dim, bright;
    if (strcasecmp(noun, "blood") == 0) {
        dim = 'r'; bright = 'R';
    } else if (strcasecmp(noun, "pee") == 0) {
        dim = 'y'; bright = 'Y';
    } else {
        dim = 'w'; bright = 'W';
    }
    return size >= 4 ? bright : dim;
}

/* Rewrites pool `o`'s short_descr/long_descr from its current val[0]
 * size tier and substance noun -- the shared render step both
 * obj_grow_pool() (growing) and pool_decay_visit() (shrinking) call
 * after changing val[0], so the two size/color helpers above only need
 * one place that actually formats the text. */
static void pool_set_descr(obj_t *o, const char *noun) {
    const char *phrase = pool_size_phrase(o->val[0]);
    char color = pool_noun_color(noun, o->val[0]);
    snprintf(o->base.short_descr, sizeof(o->base.short_descr), "%s <%c>%s<z>", phrase, color, noun);
    /* Capitalize only the sentence-starting long_descr copy -- short_descr
     * stays lowercase-first (inventory/ground-listing convention, obj.h). */
    char capped[64];
    snprintf(capped, sizeof(capped), "%s", phrase);
    capped[0] = (char)toupper((unsigned char)capped[0]);
    snprintf(o->long_descr, sizeof(o->long_descr), "%s <%c>%s<z> is here.", capped, color, noun);
}

void obj_grow_pool(struct room *room, const char *type_tag, const char *keywords,
                    const char *noun) {
    if (!room)
        return;

    for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_TRASH || !pool_keyword_matches(t->name, type_tag))
            continue;
        o->val[0]++;
        pool_set_descr(o, noun);
        return;
    }

    obj_t *o = obj_create_ephemeral(keywords, "", "", OBJ_CAT_TRASH);
    if (!o)
        return;
    o->wear_flag = 0; /* not takeable -- ground scenery until a scavenger cleans it up */
    o->val[0] = 1;
    pool_set_descr(o, noun);
    thing_move_to(&o->base, &room->base);
}

/* Recovers the substance noun ("pee"/"blood") from a pool's current
 * short_descr ("a pool of <r>blood<z>" -> "blood") so decay can regenerate
 * the text at a smaller tier -- pool_set_descr() always writes "<phrase>
 * <color>noun<z>" with the phrase ending in "of ", so the noun is
 * whatever comes after the last " of ", with its color-tag wrapper
 * stripped back off. No separate noun field exists on obj_t to store it
 * directly (the generic int val[4] payload has no string slot). */
static void pool_noun_from_descr(const char *short_descr, char *out, size_t outsz) {
    const char *p = strstr(short_descr, " of ");
    const char *start = p ? p + 4 : short_descr;
    if (start[0] == '<' && start[2] == '>')
        start += 3; /* skip the "<X>" color tag */

    char tmp[64];
    snprintf(tmp, sizeof(tmp), "%s", start);
    char *end_tag = strstr(tmp, "<z>");
    if (end_tag)
        *end_tag = '\0';
    snprintf(out, outsz, "%s", tmp);
}

/* Per-object visit for obj_decay_tick()'s puddle-aging pass below --
 * shrinks one ground puddle/pool by one val[0] tier, destroying it
 * outright once it bottoms out, or re-rendering its text at the
 * smaller size otherwise (see pool_noun_from_descr() just above for how
 * the substance noun is recovered without a dedicated field). */
static void pool_decay_visit(obj_t *o) {
    if (o->category != OBJ_CAT_TRASH || !pool_keyword_matches(o->base.name, "puddle"))
        return;

    o->val[0]--;
    if (o->val[0] <= 0) {
        obj_destroy(o); /* fully absorbed into the ground */
        return;
    }

    char noun[32];
    pool_noun_from_descr(o->base.short_descr, noun, sizeof(noun));
    pool_set_descr(o, noun);
}

/* Ages every ground puddle in the world down one size tier -- "puddle"
 * disappears entirely, "pool"/"large pool" shrink toward it -- reversing
 * obj_grow_pool()'s growth a little at a time (user, 2026-07-11: "pools
 * should absorb into the ground little by little upon ticks"). Pulse-
 * registered in main.c at the same ~60s cadence as mob_ai_tick(); also
 * forced synchronously by `aitick` (cmd_aitick.c) for deterministic
 * testing, same precedent as the mob wander/scavenge chances. */
void obj_pool_decay_tick(long pulse_num) {
    (void)pulse_num;
    world_for_each_obj(pool_decay_visit);
}

/* Per-object visit for obj_light_burn_tick() below -- burns one unit of
 * fuel (val[2]) off a lit light source (val[3] nonzero), extinguishing
 * it (val[3]=0) once fuel hits 0. No-ops for anything that isn't a lit
 * OBJ_CAT_LIGHT. */
static void light_burn_visit(obj_t *o) {
    if (o->category != OBJ_CAT_LIGHT || !o->val[3])
        return;
    o->val[2]--;
    if (o->val[2] <= 0) {
        o->val[2] = 0;
        o->val[3] = 0;
    }
}

/* Runs light_burn_visit() over a being's own carried/worn/held chain --
 * lamps and torches are usually CARRIED, not sitting on a room floor, so
 * world_for_each_obj() alone (room-floor objects only, see its own doc
 * comment) would never burn one down. */
static void light_burn_being(const being_t *b) {
    for (thing_t *t = b->base.stuff_head; t; t = t->stuff_next)
        if (t->kind == THING_OBJ)
            light_burn_visit((obj_t *)t);
}

/* world_for_each_mob() adapter for obj_light_burn_tick() below -- burns
 * fuel on a mob's own carried/worn/held lights via light_burn_being(). */
static void light_burn_mob_visit(being_t *m) {
    light_burn_being(m);
}

/* Runs on a timer (see main.c): burns fuel off every lit light source
 * in the world -- room-floor lights, every connected player's carried
 * lights, and every mob's carried lights -- via the three visit
 * helpers above, since a light can be sitting in any of those three
 * places, not just on the ground. */
void obj_light_burn_tick(long pulse_num) {
    (void)pulse_num;
    world_for_each_obj(light_burn_visit); /* room-floor lights (lampposts, ...) */
    for (descriptor_t *d = g_descriptors; d; d = d->next)
        if (d->character)
            light_burn_being(d->character); /* connected players' carried lights */
    world_for_each_mob(light_burn_mob_visit); /* mob-carried lights */
}

/* `world_for_each_obj()` only ever visits objects sitting DIRECTLY in a
 * room's own stuff_head (see its own doc comment), so `o->base.parent`
 * here is guaranteed to be that room's `&r->base` -- same first-member
 * pointer-cast idiom this codebase already uses throughout (e.g. `(being_t
 * *)t`), not a new assumption. */
static void decay_visit(obj_t *o) {
    if (o->decay_time < 0)
        return;
    if (o->decay_time > 0)
        o->decay_time--;
    if (o->decay_time != 0)
        return;

    room_t *r = (room_t *)o->base.parent;

    /* Relocate contents to the room FIRST -- obj_destroy() doesn't touch
     * children at all (see its own doc comment), so a decaying
     * container's contents would otherwise dangle off a freed parent.
     * Matches the original's "relocate container contents before
     * deletion" rule verbatim. */
    while (o->base.stuff_head)
        thing_move_to(o->base.stuff_head, &r->base);

    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char cap[128];
    snprintf(cap, sizeof(cap), "%s", label);
    cap[0] = (char)toupper((unsigned char)cap[0]);
    char msg[192];
    snprintf(msg, sizeof(msg), "%s decays into nothing.\r\n", cap);
    descriptor_room_echo(r, NULL, msg);

    obj_destroy(o);
}

/* Runs on a timer (see main.c): counts down decay_time on every
 * room-floor object via decay_visit() above, destroying (with a room
 * message) anything that reaches 0 -- objects carried by a being don't
 * decay this way (see decay_visit()'s own comment on why only room
 * objects are reachable here). */
void obj_decay_tick(long pulse_num) {
    (void)pulse_num;
    world_for_each_obj(decay_visit);
}

/* Unlinks `o` from whatever it's currently sitting in (room, being's
 * inventory, or another container) and frees it. Does NOT relocate its
 * own contents if `o` is itself a container -- callers that need that
 * (e.g. decay_visit() above) must move the contents out first, since
 * this function has no opinion on where they should go. */
void obj_destroy(obj_t *o) {
    if (!o)
        return;
    thing_remove_from_parent(&o->base);
    free(o);
}

/* Total weight of the objects currently inside `container` (its THING_OBJ
 * children in the shared thing_t chain). Non-recursive: a container nested
 * inside another counts only by its own weight here, matching how capacity
 * is checked one level at a time. */
const char *obj_apply_ground_token(const char *text, const struct room *room,
                                    char *buf, size_t bufsz) {
    if (!text || !buf || bufsz == 0)
        return buf;

    const char *ground = room_ground_type(room);
    size_t bi = 0;
    for (size_t i = 0; text[i] && bi + 1 < bufsz; ) {
        if (text[i] == '$' && text[i + 1] == '$' && text[i + 2] == 'g') {
            bi += (size_t)snprintf(buf + bi, bufsz - bi, "%s", ground);
            i += 3;
        } else if (text[i] == '$' && text[i + 1] == 'g') {
            bi += (size_t)snprintf(buf + bi, bufsz - bi, "%s", ground);
            i += 2;
        } else {
            buf[bi++] = text[i++];
        }
    }
    buf[bi < bufsz ? bi : bufsz - 1] = '\0';
    return buf;
}

#define ARMOR_AC_PER_WEIGHT 2   /* see obj.h's obj_armor_ac() doc comment */
#define ARMOR_AC_MAX 30         /* caps one absurdly heavy piece from dominating */

int obj_armor_ac(const obj_t *o) {
    if (!o)
        return 0;
    /* Real, hand-authored objaffect data applies no matter what category
     * this item collapsed into -- rings/shields/other worn jewelry carry
     * real AC rows too, not just OBJ_CAT_ARMOR (Magic items, Sneezy ->
     * Tobin feature audit; see obj_t's own doc comment on aff_ac for the
     * sign-flip, and this function's header comment for how a real vnum
     * caught the category-gated version of this dropping the bonus). The
     * guessed weight formula stays armor-only -- guessing an AC for a
     * ring with no real data would be nonsense. */
    int ac;
    if (o->aff_ac != 0) {
        ac = o->aff_ac;
    } else {
        if (o->category != OBJ_CAT_ARMOR)
            return 0;
        ac = (int)(o->weight * ARMOR_AC_PER_WEIGHT);
    }
    /* Material property system: a higher-tier material scales whatever
     * AC the item already carries -- applied before the cap, so material
     * can push a piece closer to ARMOR_AC_MAX but never past it. */
    ac = (int)(ac * material_tier_ac_mult(material_tier_for_id(o->material)));
    if (ac > ARMOR_AC_MAX)
        ac = ARMOR_AC_MAX;
    return ac;
}

/* Pre-colored condition word ("brand new" down to "destroyed") for
 * `o`'s current cur_struct/max_struct ratio -- returns NULL for an
 * object with no structure to track at all (max_struct<=0). Appended
 * next to an item's name wherever its condition is shown (inventory,
 * equipment listing). See the inline comment for why each threshold is
 * a cross-multiplied fraction rather than a rounded percentage. */
const char *obj_condition_word(const obj_t *o) {
    if (!o || o->max_struct <= 0)
        return NULL;
    int cur = o->cur_struct;
    if (cur > o->max_struct) cur = o->max_struct;
    if (cur < 0) cur = 0;
    int max = o->max_struct;
    /* Real thresholds AND real colors, both from TObj::equip_condition()
     * (misc/info.cc) -- e.g. `sstring a("<C>like new<1>");` -- ported
     * verbatim rather than inventing Tobin's own scheme. <letter> tags
     * match Tobin's own colorstring.c one-for-one (bright/dim pairs),
     * stripped or ANSI-translated downstream depending on the viewer's
     * color toggle, same as every other pre-colored string in the
     * codebase (socials, help text, ...). Each a strict "p > 0.N"
     * against the exact fraction cur/max, compared via
     * cross-multiplication (cur*10 > max*N) rather than an integer
     * percentage, which would floor/truncate and misclassify values
     * right at a tenth boundary (e.g. cur/max == 0.90 exactly must fall
     * through to "excellent", not get rounded up into "like new"). */
    if (cur == max)          return "<C>brand new<1>";
    if (cur * 10 > max * 9)  return "<c>like new<1>";
    if (cur * 10 > max * 8)  return "<B>excellent<1>";
    if (cur * 10 > max * 7)  return "<b>very good<1>";
    if (cur * 10 > max * 6)  return "<P>good<1>";
    if (cur * 10 > max * 5)  return "<p>fine<1>";
    if (cur * 10 > max * 4)  return "<G>fair<1>";
    if (cur * 10 > max * 3)  return "<g>poor<1>";
    if (cur * 10 > max * 2)  return "<y>very poor<1>";
    if (cur * 10 > max * 1)  return "<o>bad<1>";
    if (cur > 0)             return "<R>very bad<1>";
    return "<r>destroyed<1>";
}

/* Applies (sign=+1) or reverses (sign=-1) `o`'s cached aff_str..aff_cha
 * stat bonuses to `ch`, clamped into [1, ATTR_MAX] -- the stat half of
 * obj_apply_equip_affects() below, factored out separately since
 * obj_apply_equip_load_affects() needs just this half on reconnect
 * (see its own doc comment for why HIT/MOVE must NOT be replayed
 * there). No-ops entirely if `o` has no stat affects cached at all. */
static void apply_equip_stat_affects(being_t *ch, const obj_t *o, int sign) {
    if (!o->aff_str && !o->aff_dex && !o->aff_con && !o->aff_intel
        && !o->aff_wis && !o->aff_cha)
        return;

    ch->attrs.strength     += sign * o->aff_str;
    ch->attrs.dexterity    += sign * o->aff_dex;
    ch->attrs.constitution += sign * o->aff_con;
    ch->attrs.intelligence += sign * o->aff_intel;
    ch->attrs.wisdom       += sign * o->aff_wis;
    ch->attrs.charisma     += sign * o->aff_cha;

    int *attrs[] = {&ch->attrs.strength, &ch->attrs.dexterity, &ch->attrs.constitution,
                    &ch->attrs.intelligence, &ch->attrs.wisdom, &ch->attrs.charisma};
    for (size_t i = 0; i < sizeof(attrs) / sizeof(attrs[0]); i++) {
        if (*attrs[i] < 1) *attrs[i] = 1;
        if (*attrs[i] > ATTR_MAX) *attrs[i] = ATTR_MAX;
    }
}

/* Applies (sign=+1) or reverses (sign=-1) all of `o`'s equip-time
 * bonuses to `ch`: stats via apply_equip_stat_affects() above, plus
 * max_hp/hp and max_vit/vit from aff_hit/aff_move, each clamped so
 * current never exceeds the (possibly just-changed) max and never
 * drops below 0/1. The normal wear/remove path -- see
 * obj_apply_equip_load_affects() below for the narrower reconnect-only
 * variant. */
void obj_apply_equip_affects(being_t *ch, const obj_t *o, int sign) {
    apply_equip_stat_affects(ch, o, sign);

    if (o->aff_hit) {
        ch->progress.max_hp += sign * o->aff_hit;
        if (ch->progress.max_hp < 1) ch->progress.max_hp = 1;
        ch->progress.hp += sign * o->aff_hit;
        if (ch->progress.hp > ch->progress.max_hp) ch->progress.hp = ch->progress.max_hp;
        if (ch->progress.hp < 0) ch->progress.hp = 0;
    }
    if (o->aff_move) {
        ch->progress.max_vit += sign * o->aff_move;
        if (ch->progress.max_vit < 1) ch->progress.max_vit = 1;
        ch->progress.vit += sign * o->aff_move;
        if (ch->progress.vit > ch->progress.max_vit) ch->progress.vit = ch->progress.max_vit;
        if (ch->progress.vit < 0) ch->progress.vit = 0;
    }
}

/* On RECONNECT only (player_inventory_load(), obj_repo.c) -- re-lands an
 * already-equipped item's STAT bonus, but deliberately NOT its HIT/MOVE
 * one. `attrs_t` (STR/DEX/CON/INT/WIS/CHA) is only ever persisted at
 * character creation or via the immortal edplayer editor -- normal play
 * never saves it -- so a stat bonus applied by cmd_wear() lives ONLY in
 * memory and would silently vanish across a reconnect if load didn't
 * reapply it here (found live: a combat smoke test's raw-SQL DEX bump
 * wasn't surviving a reconnect during Magic items testing). `progress`
 * (max_hp/max_vit and current hp/vit) is the opposite story: vitals_tick_
 * run() (and several other commands) save it every ~60s for any
 * connected MORTAL regardless of what changed, so by the time a real
 * session disconnects, a HIT/MOVE bonus from a still-worn item is
 * already baked into the saved value -- reapplying it here on top would
 * double it, compounding further on every subsequent relog. Immortals
 * are the one narrow gap this leaves: vitals_tick_run() skips them
 * entirely, so an immortal who wears a HIT/MOVE item and disconnects
 * before any OTHER action happens to save progress loses that bonus on
 * reconnect until they re-wear it -- accepted as a minor, immortal-only,
 * testing-adjacent edge case rather than reintroducing the compounding
 * bug for real mortal players. */
void obj_apply_equip_load_affects(being_t *ch, const obj_t *o) {
    apply_equip_stat_affects(ch, o, 1);
}

/* Total weight of the objects currently inside `container` (its
 * THING_OBJ children in the shared thing_t chain) -- see the misplaced
 * copy of this comment above obj_apply_ground_token() for the original
 * wording. Non-recursive: a container nested inside another counts
 * only by its own weight here, matching how capacity is checked one
 * level at a time. */
double obj_contained_weight(const obj_t *container) {
    if (!container)
        return 0.0;
    double total = 0.0;
    for (const thing_t *t = container->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ)
            total += ((const obj_t *)t)->weight;
    }
    return total;
}
