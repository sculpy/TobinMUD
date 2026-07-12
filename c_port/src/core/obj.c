/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "obj.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj_repo.h"
#include "room.h"
#include "thing.h"
#include "world.h"

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

/* Bit-position-indexed names for the WEAR_* layout above -- "UNUSED" for
 * the original's own two never-assigned bits (9 and 13), kept so a
 * historic wear_flag value that happens to set one still reports
 * something rather than silently vanishing from the readable form. */
static const char *const WEAR_FLAG_NAMES[16] = {
    "TAKE", "FINGERS", "NECK", "BODY", "HEAD", "LEGS", "FEET", "HANDS",
    "ARMS", "UNUSED", "BACK", "WAIST", "WRISTS", "UNUSED", "HOLD", "THROW",
};

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
    if (!o || o->category != OBJ_CAT_ARMOR)
        return 0;
    int ac = (int)(o->weight * ARMOR_AC_PER_WEIGHT);
    if (ac > ARMOR_AC_MAX)
        ac = ARMOR_AC_MAX;
    return ac;
}

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
