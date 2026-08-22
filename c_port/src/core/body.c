/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "body.h"

/* Verbatim copy of the original's real slot_chance[MAX_BODY_TYPES][MAX_WEAR]
 * (body.cc) -- do not hand-edit a row's numbers, they're real upstream
 * data. Column order, from that file's own comment:
 *   0=unused 1=finger 2=finger 3=neck 4=body 5=head 6=leg 7=leg 8=foot
 *   9=foot 10=hand 11=hand 12=arm 13=arm 14=back 15=waist 16=wrist
 *   17=wrist 18=hold 19=hold 20=EX_leg 21=EX_leg 22=EX_foot 23=EX_foot
 * Row order matches body_type_t exactly, INCLUDING a real upstream quirk:
 * the original's own `bodyNames[]` array is missing an entry for
 * BODY_WYVELIN (60 strings for 61 real body_t values) -- body_type_name()
 * below adds the missing name rather than reproducing that off-by-one. */
static const unsigned char SLOT_CHANCE[BODY_TYPE_COUNT][24] = {
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 10, 5, 3, 3, 7, 7, 0, 0, 0, 0},   /* BODY_NONE */
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 10, 5, 3, 3, 7, 7, 0, 0, 0, 0},   /* BODY_HUMANOID */
    {0, 0, 0, 0, 15, 25, 5, 5, 0, 0, 0, 0, 0, 0, 5, 25, 0, 0, 0, 0, 5, 5, 5, 5},  /* BODY_INSECTOID */
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_PIERCER */
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_MOSS */
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_ELEMENTAL */
    {0, 0, 0, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 10, 5, 0, 0, 7, 7, 0, 0, 0, 0},   /* BODY_KUOTOA */
    {0, 0, 0, 0, 40, 0, 5, 5, 0, 0, 6, 6, 6, 6, 0, 0, 0, 0, 6, 6, 5, 5, 0, 0},    /* BODY_CRUSTACEAN */
    {0, 1, 1, 4, 31, 7, 3, 0, 2, 0, 3, 3, 5, 5, 10, 5, 3, 3, 7, 7, 0, 0, 0, 0},   /* BODY_DJINN */
    {0, 1, 1, 4, 31, 7, 3, 0, 2, 0, 3, 3, 5, 5, 10, 5, 3, 3, 7, 7, 0, 0, 0, 0},   /* BODY_MERMAID */
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 10, 5, 3, 3, 7, 7, 0, 0, 0, 0},   /* BODY_FROGMAN */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_MANTICORE */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_GRIFFON */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_SHEDU */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_SPHINX */
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 12, 4, 3, 3, 5, 5, 3, 3, 2, 2},   /* BODY_CENTAUR */
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 12, 4, 3, 3, 5, 5, 3, 3, 2, 2},   /* BODY_LAMIA */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_LAMMASU */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 3, 3, 5, 5, 7, 5, 0, 0, 2, 2, 0, 0, 0, 0},    /* BODY_WYVERN */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_DRAGONNE */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_HIPPOGRIFF */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_CHIMERA */
    {0, 0, 0, 5, 30, 7, 5, 5, 3, 3, 0, 0, 5, 5, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_DRAGON */
    {0, 0, 0, 0, 75, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_FISH */
    {0, 0, 0, 0, 80, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_SNAKE */
    {0, 0, 0, 0, 80, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_NAGA */
    {0, 0, 0, 0, 47, 8, 4, 4, 4, 4, 0, 0, 0, 0, 5, 0, 0, 0, 4, 4, 4, 4, 4, 4},    /* BODY_SPIDER */
    {0, 0, 0, 0, 47, 8, 4, 4, 4, 4, 0, 0, 0, 0, 5, 0, 0, 0, 4, 4, 4, 4, 4, 4},    /* BODY_CENTIPEDE */
    {0, 0, 0, 0, 40, 10, 5, 5, 5, 5, 0, 0, 5, 5, 0, 0, 0, 0, 5, 5, 5, 5, 0, 0},   /* BODY_OCTOPUS */
    {0, 0, 0, 8, 42, 9, 5, 5, 0, 0, 0, 0, 7, 7, 0, 7, 0, 0, 5, 5, 0, 0, 0, 0},    /* BODY_BIRD */
    {0, 0, 0, 8, 42, 9, 5, 5, 0, 0, 0, 0, 7, 7, 0, 7, 0, 0, 5, 5, 0, 0, 0, 0},    /* BODY_BAT */
    {0, 0, 0, 0, 80, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* BODY_TREE */
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_PARASITE */
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_SLIME */
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_ORB */
    {0, 0, 0, 0, 80, 0, 0, 0, 0, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},  /* BODY_VEGGIE */
    {0, 0, 0, 2, 50, 3, 5, 5, 3, 3, 5, 5, 5, 5, 5, 0, 2, 2, 0, 0, 5, 5, 0, 0},    /* BODY_DEMON */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_LION */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_FOUR_LEG */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_PIG */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 0, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_TURTLE */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_FOUR_HOOF */
    {0, 0, 0, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 10, 5, 0, 0, 7, 7, 0, 0, 0, 0},   /* BODY_BAANTA */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_AMPHIBEAN */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 0, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_FROG */
    {0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_MIMIC */
    {0, 1, 1, 2, 50, 3, 5, 5, 3, 3, 5, 5, 5, 5, 5, 5, 2, 2, 7, 7, 0, 0, 0, 0},    /* BODY_MEDUSA */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_FELINE */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_DINOSAUR */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_REPTILE */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 0, 0, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_ELEPHANT */
    {0, 0, 0, 0, 37, 9, 7, 7, 4, 4, 0, 0, 7, 7, 7, 0, 0, 0, 0, 0, 7, 0, 4, 0},    /* BODY_OTYUGH */
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 10, 5, 3, 3, 7, 7, 0, 0, 0, 0},   /* BODY_OWLBEAR */
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 10, 5, 3, 3, 7, 7, 0, 0, 0, 0},   /* BODY_MINOTAUR */
    {0, 0, 0, 2, 50, 3, 5, 5, 3, 3, 5, 5, 5, 5, 5, 0, 2, 2, 0, 0, 0, 0, 0, 0},    /* BODY_GOLEM */
    {0, 0, 0, 0, 65, 19, 0, 0, 0, 0, 0, 0, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},   /* BODY_COATL */
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 12, 4, 3, 3, 5, 5, 3, 3, 2, 2},   /* BODY_SIMAL */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 7, 7, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_PEGASUS */
    {0, 0, 0, 0, 15, 25, 5, 5, 0, 0, 0, 0, 0, 0, 5, 25, 0, 0, 0, 0, 5, 5, 5, 5},  /* BODY_ANT */
    {0, 0, 0, 6, 38, 8, 5, 5, 3, 3, 0, 0, 7, 7, 7, 5, 0, 0, 2, 2, 5, 5, 3, 3},    /* BODY_WYVELIN */
    {0, 1, 1, 4, 26, 7, 3, 3, 2, 2, 3, 3, 5, 5, 10, 5, 3, 3, 7, 7, 0, 0, 0, 0},   /* BODY_FISHMAN */
};

static const char *const BODY_NAMES[BODY_TYPE_COUNT] = {
    "NONE", "HUMANOID", "INSECTOID", "PIERCER", "MOSS", "ELEMENTAL", "KUOTOA",
    "CRUSTACEAN", "DJINN", "MERMAID", "FROGMAN", "MANTICORE", "GRIFFON",
    "SHEDU", "SPHINX", "CENTAUR", "LAMIA", "LAMMASU", "WYVERN", "DRAGONNE",
    "HIPPOGRIFF", "CHIMERA", "DRAGON", "FISH", "SNAKE", "NAGA", "SPIDER",
    "CENTIPEDE", "OCTOPUS", "BIRD", "BAT", "TREE", "PARASITE", "SLIME", "ORB",
    "VEGGIE", "DEMON", "LION", "FOUR_LEG", "PIG", "TURTLE", "FOUR_HOOF",
    "BAANTA", "AMPHIBEAN", "FROG", "MIMIC", "MEDUSA", "FELINE", "DINOSAUR",
    "REPTILE", "ELEPHANT", "OTYUGH", "OWLBEAR", "MINOTAUR", "GOLEM", "COATL",
    "SIMAL", "PEGASUS", "ANT", "WYVELIN", "FISHMAN",
};

/* Maps each Tobin limb_t to its SLOT_CHANCE column (see the table's own
 * comment for the column layout). -1 = no original column at all
 * (LIMB_GENITALIA, a Tobin-only slot). */
static const int LIMB_TO_COL[LIMB_COUNT] = {
    [LIMB_HEAD] = 5, [LIMB_NECK] = 3, [LIMB_BACK] = 14,
    [LIMB_LEFT_ARM] = 12, [LIMB_RIGHT_ARM] = 13,
    [LIMB_LEFT_WRIST] = 16, [LIMB_RIGHT_WRIST] = 17,
    [LIMB_LEFT_HAND] = 10, [LIMB_RIGHT_HAND] = 11,
    [LIMB_LEFT_FINGER] = 1, [LIMB_RIGHT_FINGER] = 2,
    [LIMB_BODY] = 4, [LIMB_WAIST] = 15,
    [LIMB_GENITALIA] = -1,
    [LIMB_RIGHT_LEG] = 6, [LIMB_LEFT_LEG] = 7,
    [LIMB_LEFT_FOOT] = 8, [LIMB_RIGHT_FOOT] = 9,
    [LIMB_EX_RIGHT_LEG] = 20, [LIMB_EX_LEFT_LEG] = 21,
    [LIMB_EX_RIGHT_FOOT] = 22, [LIMB_EX_LEFT_FOOT] = 23,
};

/* Display name for a body_type_t (BODY_NAMES[] above, upstream
 * bodyNames[] with the missing BODY_WYVELIN entry filled in -- see the
 * SLOT_CHANCE comment) -- falls back to BODY_HUMANOID for an
 * out-of-range value rather than reading past the array. */
const char *body_type_name(body_type_t bt) {
    if (bt < 0 || bt >= BODY_TYPE_COUNT)
        bt = BODY_HUMANOID;
    return BODY_NAMES[bt];
}

/* How much of body type `bt`'s overall equip-slot weight `limb` gets --
 * 0 means this body shape doesn't have that limb at all
 * (being_limbs_full_heal() uses exactly that to decide which limbs are
 * "present"). Looks up the upstream SLOT_CHANCE table via LIMB_TO_COL,
 * except LIMB_GENITALIA which has no upstream column and is synthesized
 * from whether this body shape has a waist (see the inline comment). */
int body_limb_weight(body_type_t bt, limb_t limb) {
    if (bt < 0 || bt >= BODY_TYPE_COUNT)
        bt = BODY_HUMANOID;
    if (limb < 0 || limb >= LIMB_COUNT)
        return 0;
    if (limb == LIMB_GENITALIA) {
        /* No original column -- Tobin-only slot. Present (weight 1, same
         * as the humanoid row's own finger/genitalia-adjacent rarity)
         * only on body shapes with a real waist (column 15), a reasonable
         * proxy for "humanoid enough to have this anatomy" -- everything
         * from an insect to a tree to an elemental has waist=0. */
        return SLOT_CHANCE[bt][15] > 0 ? 1 : 0;
    }
    int col = LIMB_TO_COL[limb];
    if (col < 0)
        return 0;
    return SLOT_CHANCE[bt][col];
}

/* For an arthropod-shaped body (insectoid/spider/centipede/ant), returns
 * "leg" phrasing instead of the default "foot" limb_name() for the
 * foot slots -- an insect's rearmost limb segments read as legs, not
 * feet. Returns NULL (use the default name) for every other body type
 * or limb. */
const char *body_limb_name_override(body_type_t bt, limb_t limb) {
    bool arthropod = bt == BODY_INSECTOID || bt == BODY_SPIDER
        || bt == BODY_CENTIPEDE || bt == BODY_ANT;
    if (!arthropod)
        return NULL;
    switch (limb) {
        case LIMB_LEFT_FOOT:  return "left leg";
        case LIMB_RIGHT_FOOT: return "right leg";
        case LIMB_EX_LEFT_FOOT:  return "extra left leg";
        case LIMB_EX_RIGHT_FOOT: return "extra right leg";
        default: return NULL;
    }
}
