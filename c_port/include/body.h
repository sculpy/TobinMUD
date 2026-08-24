/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BODY_H
#define TOBIN_BODY_H

#include "being.h"

/* Body types (Sneezy -> Tobin feature audit, user 2026-07-26: "creatures
 * have different limb sets" -- pairs with the Limbs -> wearSlotT reshape).
 * Full parity with the original's real `body_t` enum (body.h) and its
 * per-type `slot_chance[MAX_BODY_TYPES][MAX_WEAR]` hit-weight table
 * (body.cc) -- 60 real body shapes, verbatim ordering and numbers, not a
 * Tobin invention or a scoped-down subset (user explicitly chose full
 * parity when asked, given the mob table had zero body-type data to
 * build a smaller mechanism against anyway).
 *
 * Disclosed gap: the real seeded `mob` table never carried body-type
 * data (confirmed live, no such column existed before this). A new
 * `mob.body_type` column defaults every mob to BODY_HUMANOID -- a
 * meaningful real sample (obviously four-legged/serpentine/insectoid
 * named mobs) is classified via name-matching heuristics
 * (tobin_migrations.sql), not an exhaustive hand-audit of all real
 * seeded mobs -- that remains a separate, later task. `edit mob` (medit)
 * doesn't expose this field yet either (mob_proto_t round-trips it
 * correctly via SQL/mob_proto_load()/mob_proto_save(), just no UI). */
typedef enum {
    BODY_NONE, BODY_HUMANOID, BODY_INSECTOID, BODY_PIERCER, BODY_MOSS,
    BODY_ELEMENTAL, BODY_KUOTOA, BODY_CRUSTACEAN, BODY_DJINN, BODY_MERMAID,
    BODY_FROGMAN, BODY_MANTICORE, BODY_GRIFFON, BODY_SHEDU, BODY_SPHINX,
    BODY_CENTAUR, BODY_LAMIA, BODY_LAMMASU, BODY_WYVERN, BODY_DRAGONNE,
    BODY_HIPPOGRIFF, BODY_CHIMERA, BODY_DRAGON, BODY_FISH, BODY_SNAKE,
    BODY_NAGA, BODY_SPIDER, BODY_CENTIPEDE, BODY_OCTOPUS, BODY_BIRD,
    BODY_BAT, BODY_TREE, BODY_PARASITE, BODY_SLIME, BODY_ORB,
    BODY_VEGGIE, BODY_DEMON, BODY_LION, BODY_FOUR_LEG, BODY_PIG,
    BODY_TURTLE, BODY_FOUR_HOOF, BODY_BAANTA, BODY_AMPHIBEAN, BODY_FROG,
    BODY_MIMIC, BODY_MEDUSA, BODY_FELINE, BODY_DINOSAUR, BODY_REPTILE,
    BODY_ELEPHANT, BODY_OTYUGH, BODY_OWLBEAR, BODY_MINOTAUR, BODY_GOLEM,
    BODY_COATL, BODY_SIMAL, BODY_PEGASUS, BODY_ANT, BODY_WYVELIN, BODY_FISHMAN,
    BODY_TYPE_COUNT
} body_type_t;

/* "HUMANOID", "INSECTOID", ... (bodyNames[], body.cc) -- used by `stat`. */
const char *body_type_name(body_type_t bt);

/* Per-limb hit-chance weight for `bt`, straight from the original's real
 * BODY_HUMANOID/BODY_SPIDER/etc. slot_chance[] row -- 0 for any Tobin
 * limb this body shape doesn't have at all (e.g. BODY_SNAKE has no arms).
 * `bt` out of range clamps to BODY_HUMANOID. Replaces combat.c's old
 * flat, humanoid-only LIMB_HIT_WEIGHT table. */
int body_limb_weight(body_type_t bt, limb_t limb);

/* Body-appropriate display name for `limb` on `bt`, overriding limb_name()
 * (being.c) where the generic name would be flatly wrong -- user
 * 2026-07-26: "spiders dont have feet". An arthropod's real/EX_ "foot"
 * slots are anatomically just more legs (their own slot_chance weights
 * even add up to a real 8-legs-total count for BODY_SPIDER once counted
 * this way: 2 leg + 2 EX_leg + 2 foot + 2 EX_foot), so those columns are
 * relabeled "leg" for the arthropod body types (INSECTOID/SPIDER/
 * CENTIPEDE/ANT) instead of showing "foot"/"extra foot". Returns NULL for
 * every other limb/body-type combination -- caller falls back to
 * limb_name() as normal. */
const char *body_limb_name_override(body_type_t bt, limb_t limb);

#endif
