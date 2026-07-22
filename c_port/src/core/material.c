/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "material.h"

/* Real MAT_* numeric IDs from the upstream import (misc/materials.h),
 * bucketed into Tobin's 5 tiers. Grouped by the original's own General
 * (0-19) / Nature (50-77) / Mineral (100-126) / Metal (150-177) ranges
 * for readability, not because the ranges themselves mean anything to
 * Tobin. Anything not listed here falls through to COMMON (see
 * material.h's doc comment on why that's the safe default). */
material_tier_t material_tier_for_id(int material) {
    switch (material) {
        /* --- FINE --- */
        case 13:  /* MAT_TOUGH_CLOTH */
        case 52:  /* MAT_TOUGH_LEATHER */
        case 125: /* MAT_JET */
        case 156: /* MAT_BRONZE */
        case 157: /* MAT_BRASS */
        case 158: /* MAT_IRON */
            return MATERIAL_TIER_FINE;

        /* --- SUPERIOR --- */
        case 73:  /* MAT_DWARF_LEATHER */
        case 76:  /* MAT_OGRE_HIDE */
        case 107: /* MAT_IVORY */
        case 108: /* MAT_OBSIDIAN */
        case 109: /* MAT_ONYX */
        case 116: /* MAT_JADE */
        case 117: /* MAT_AMBER */
        case 118: /* MAT_TURQUOISE */
        case 122: /* MAT_MALACHITE */
        case 124: /* MAT_QUARTZ */
        case 126: /* MAT_CORUNDUM */
        case 159: /* MAT_STEEL */
        case 166: /* MAT_ALUMINUM */
            return MATERIAL_TIER_SUPERIOR;

        /* --- RARE --- */
        case 53:  /* MAT_DRAGON_SCALE */
        case 67:  /* MAT_PEARL */
        case 102: /* MAT_RUNESTONE */
        case 103: /* MAT_CRYSTAL */
        case 106: /* MAT_EMERALD */
        case 110: /* MAT_OPAL */
        case 111: /* MAT_RUBY */
        case 112: /* MAT_SAPPHIRE */
        case 119: /* MAT_AMETHYST */
        case 121: /* MAT_DRAGONBONE */
        case 162: /* MAT_SILVER */
        case 163: /* MAT_GOLD */
        case 164: /* MAT_PLATINUM */
        case 165: /* MAT_TITANIUM */
        case 169: /* MAT_ELECTRUM */
        case 172: /* MAT_TUNGSTEN */
            return MATERIAL_TIER_RARE;

        /* --- LEGENDARY --- */
        case 57:  /* MAT_WATER */
        case 58:  /* MAT_FIRE */
        case 59:  /* MAT_EARTH */
        case 60:  /* MAT_ELEMENTAL */
        case 61:  /* MAT_ICE */
        case 62:  /* MAT_LIGHTNING */
        case 63:  /* MAT_CHAOS */
        case 72:  /* MAT_GHOSTLY */
        case 104: /* MAT_DIAMOND */
        case 160: /* MAT_MITHRIL */
        case 161: /* MAT_ADAMANTITE */
        case 170: /* MAT_ATHANOR */
        case 173: /* MAT_STARMETAL */
        case 174: /* MAT_TERBIUM */
        case 177: /* MAT_ETERNIUM */
            return MATERIAL_TIER_LEGENDARY;

        /* Everything else -- MAT_UNDEFINED, PAPER, CLOTH, WAX, GLASS,
         * WOOD, SILK, FOODSTUFF, LEATHER, WOOL, FUR, STONE, BONE,
         * COPPER, TIN, and the rest of the mundane/unmapped IDs --
         * stays COMMON. */
        default:
            return MATERIAL_TIER_COMMON;
    }
}

const char *material_tier_name(material_tier_t tier) {
    switch (tier) {
        case MATERIAL_TIER_FINE:       return "Fine";
        case MATERIAL_TIER_SUPERIOR:   return "Superior";
        case MATERIAL_TIER_RARE:       return "Rare";
        case MATERIAL_TIER_LEGENDARY:  return "Legendary";
        case MATERIAL_TIER_COMMON:
        default:                       return "Common";
    }
}

double material_tier_damage_mult(material_tier_t tier) {
    switch (tier) {
        case MATERIAL_TIER_FINE:       return 1.10;
        case MATERIAL_TIER_SUPERIOR:   return 1.25;
        case MATERIAL_TIER_RARE:       return 1.50;
        case MATERIAL_TIER_LEGENDARY:  return 2.00;
        case MATERIAL_TIER_COMMON:
        default:                       return 1.00;
    }
}

double material_tier_ac_mult(material_tier_t tier) {
    /* Same curve as damage -- a single "how good is this material"
     * multiplier applied to whichever stat the item actually carries. */
    return material_tier_damage_mult(tier);
}

int material_tier_struct_bonus(material_tier_t tier) {
    switch (tier) {
        case MATERIAL_TIER_FINE:       return 2;
        case MATERIAL_TIER_SUPERIOR:   return 5;
        case MATERIAL_TIER_RARE:       return 10;
        case MATERIAL_TIER_LEGENDARY:  return 20;
        case MATERIAL_TIER_COMMON:
        default:                       return 0;
    }
}

double material_tier_value_mult(material_tier_t tier) {
    switch (tier) {
        case MATERIAL_TIER_FINE:       return 1.5;
        case MATERIAL_TIER_SUPERIOR:   return 3.0;
        case MATERIAL_TIER_RARE:       return 6.0;
        case MATERIAL_TIER_LEGENDARY:  return 15.0;
        case MATERIAL_TIER_COMMON:
        default:                       return 1.0;
    }
}
