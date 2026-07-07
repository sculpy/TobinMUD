/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "obj.h"

#include <stdio.h>
#include <stdlib.h>

#include "being.h"
#include "obj_repo.h"

static const char *const OBJ_CATEGORY_NAMES[OBJ_CAT_COUNT] = {
    "light", "weapon", "ammo", "armor", "container", "drink", "food",
    "money", "key", "magic device", "tool", "furniture", "treasure",
    "written", "trash", "other",
};

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

bool obj_takeable(int wear_flag) {
    return (wear_flag & WEAR_TAKE) != 0;
}

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

    /* HANDS/WRISTS/BACK/THROW/reserved bits: no Tobin limb equivalent (the
     * 13-limb set was already deliberately trimmed vs. the original's real
     * slot list, see STATUS.md's Limbs decision row) -- carriable, not
     * wearable in this port. */
    return WEAR_SLOT_NOT_WEARABLE;
}

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
    o->wear_flag = proto.wear_flag;
    for (int i = 0; i < 4; i++)
        o->val[i] = proto.val[i];
    o->weight = proto.weight;
    o->volume = proto.volume;
    o->price = proto.price;
    o->max_struct = proto.max_struct;
    o->cur_struct = proto.cur_struct;
    o->material = proto.material;
    o->can_be_seen = proto.can_be_seen;

    return o;
}

void obj_destroy(obj_t *o) {
    if (!o)
        return;
    thing_remove_from_parent(&o->base);
    free(o);
}
