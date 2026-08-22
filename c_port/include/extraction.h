/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_EXTRACTION_H
#define TOBIN_EXTRACTION_H

/* Crafting & extraction (Sneezy → Tobin feature audit) -- corpse-based
 * material gathering (`skin`/`butcher`) plus wild foraging (`forage`),
 * the Tobin-scale slice of a much larger real system (see
 * docs/systems/informational/crafting-extraction.md in the bundled
 * reference clone: skinning, butchering, dissection, brewing, scribing,
 * and material-category repair, spanning Ranger/Shaman/Mage/Warrior).
 * Scoped down to skin+butcher+forage only, all landing on Druid (Tobin's
 * established Ranger-flavor analog, see skill.c's roster-import doc
 * comment) -- brewing/scribing need a Shaman-style component/spell-charge
 * system Tobin doesn't have, dissection needs per-race quest-item data
 * this bundled source doesn't include, and repair-by-material-category
 * would duplicate the simpler repair system Object maintenance already
 * shipped. Also dropped: the original's multi-pulse task framing (skin/
 * butcher/forage resolve instantly here, not over several task pulses --
 * no dedicated task engine exists in Tobin outside Planting's one-off),
 * per-race hide/meat item tables (one generic yield item per operation
 * instead), and weapon-dulling-on-failure (no weapon "sharpness" stat
 * exists in Tobin's obj_t to dull in the first place). All disclosed
 * simplifications, not silent gaps. */

/* obj_t.raw_type on an ephemeral corpse (combat.c) -- which kind of
 * being it came from, since skin/butcher only make sense on a mob. */
#define CORPSE_KIND_MOB 1
#define CORPSE_KIND_PC  2

/* obj_t.val[3] bits on a corpse -- once-only extraction (no half-yield
 * partial-extraction tier, unlike the original's CORPSE_HALF_SKIN/
 * CORPSE_HALF_BUTCHERED -- a disclosed simplification). */
#define CORPSE_SKINNED   (1 << 0)
#define CORPSE_BUTCHERED (1 << 1)
#define CORPSE_DISSECTED (1 << 2)

#endif
