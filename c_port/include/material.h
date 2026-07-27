/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MATERIAL_H
#define TOBIN_MATERIAL_H

/* Material property system (Sneezy → Tobin feature audit, "Material
 * property system"). Checked the real upstream first
 * (misc/materials.h/.cc, docs/systems/informational/material-system.md):
 * 83 `MAT_*` constants, a per-object `material` field (already present
 * in Tobin -- obj.h/obj_repo.h's `material` column, loaded from the real
 * seeded import data since Session ~50s but never mechanically used
 * until now), and a `material_type_numbers[200]` property table. The
 * original's OWN doc claims direct hardness→damage and hardness→AC
 * multiplier formulas -- verified against the actual .cc source and
 * those formulas do NOT exist there; what's real is durability (mutual
 * wear during combat) and value (a flat weight × material-price × 10
 * addition). Per user 2026-07-21's AskUserQuestion, Tobin's version
 * deliberately goes FURTHER than the real upstream: a genuine
 * damage/AC multiplier per tier, not just a port of what Sneezy
 * actually does -- a disclosed invention, not a faithful port.
 *
 * Rather than inventing a new per-item field, this reuses Tobin's
 * EXISTING real seeded `obj.material`/`obj_t.material` column (already
 * populated with real MAT_* values across thousands of seeded items,
 * confirmed live: e.g. 969 items at MAT_WOOD=5, 658 at MAT_STEEL=159,
 * 194 at MAT_SILVER=162) and buckets the 83 real material IDs down into
 * 5 Tobin-scale tiers via material_tier_for_id() -- matching the
 * feature audit's own "3-5 tiers, not 83" sizing recommendation. */

typedef enum {
    MATERIAL_TIER_COMMON = 0,   /* wood, cloth, leather, iron, bone, ... (the default/baseline) */
    MATERIAL_TIER_FINE,         /* tough cloth/leather, bronze, brass, jet */
    MATERIAL_TIER_SUPERIOR,     /* steel, ivory, obsidian, dwarf leather, jade, ... */
    MATERIAL_TIER_RARE,         /* silver, gold, platinum, crystal, dragon scale, gemstones, ... */
    MATERIAL_TIER_LEGENDARY,    /* mithril, adamantite, diamond, starmetal, the elemental materials */
} material_tier_t;

/* Buckets a real seeded `obj.material` value (Sneezy's own MAT_* range,
 * 0-199) into one of the 5 tiers above. Unknown/unmapped IDs (including
 * the default MAT_UNDEFINED=0) fall through to MATERIAL_TIER_COMMON --
 * a safe, neutral default (1.0x everything, +0 structure) so every
 * already-seeded object that happens to carry a material ID this
 * mapping doesn't recognize behaves exactly as it did before this
 * feature shipped. */
material_tier_t material_tier_for_id(int material);

const char *material_tier_name(material_tier_t tier);

/* Multiplies a weapon's final computed damage (combat.c's combat_strike(),
 * folded into the same gamewide dmg_mult the `balance` command already
 * applies). */
double material_tier_damage_mult(material_tier_t tier);

/* Multiplies a worn item's computed AC (obj_armor_ac(), obj.c) before the
 * existing ARMOR_AC_MAX clamp -- material can push an item closer to the
 * cap, never past it. */
double material_tier_ac_mult(material_tier_t tier);

/* Added to both max_struct and cur_struct at creation time
 * (obj_create_from_proto(), obj.c) -- a higher-tier item is tougher from
 * the moment it's made, and (since the repair-shop economy's
 * effective_max_struct() reads straight from max_struct) repairs to a
 * higher ceiling too. */
int material_tier_struct_bonus(material_tier_t tier);

/* Multiplies an item's shop price, both buying (cmd_buy()) and selling
 * (cmd_sell()) -- the one dimension the real upstream genuinely does
 * this way too (obj_base_weapon.cc's price += weight * material.price). */
double material_tier_value_mult(material_tier_t tier);

#endif
