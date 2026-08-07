/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "skill.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "descriptor.h"
#include "skill_repo.h"

/* Ported from SneezyMUD's real discArray[] (misc/spell_info.cc), trimmed
 * to Tobin's simplified 3-tier scheme -- see skill.h's doc comment.
 * Descriptions are one-line summaries of the original's effect, not the
 * original's exact wording. Weapon-proficiency ("Combat" tier for the
 * caster classes) and several class-specific physical basics are
 * genuinely level 1 in the source; others carry the source's real
 * `START_n` threshold. */
static const skill_def_t SKILLS[] = {
    /* ---------------- WARRIOR ---------------- */
    { "riding",                  CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `riding` for help." },
    /* Weapon specializations (missing-skill audit, "Generic / cross-class"
     * list; user, 2026-08-04: "all level 1 ... all of those should be
     * automatic" -- real upstream, spell_info.cc's discArray, ports each
     * as SKILL_WARRIOR/DISC_SLASH etc, START_1 -- same level 1 here.
     * "Automatic" means known from character creation with no
     * guildmaster visit required (being_knows_skill()'s own special
     * case, skill.c), climbing in proficiency purely through landed
     * hits with a matching weapon category (combat_strike(), combat.c),
     * same "learn by doing, no activation command" shape kubo/cintai
     * (Monk) already use for their own passive combat bonuses. User,
     * 2026-08-04, on what 100% proficiency should do: "bigger passive
     * bonus" -- no separate "advanced" skill unlock, just a stronger
     * bonus curve once mastered (see combat_strike()'s own comment). */
    { "slash specialization",    CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `slash specialization` for help." },
    { "blunt specialization",    CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `blunt specialization` for help." },
    { "pierce specialization",   CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `pierce specialization` for help." },
    { "ranged specialization",   CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `ranged specialization` for help." },
    { "barehand specialization", CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `barehand specialization` for help." },
    { "sign",                    CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `sign` for help." },
    { "bash",                    CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `bash` for help." },
    { "berserk",                 CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `berserk` for help." },
    { "rally",                   CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `rally` for help." },
    { "retreat",                 CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `retreat` for help." },
    { "parry",                   CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `parry` for help." },
    { "kick",                   CLASS_WARRIOR, SKILL_TIER_CLASS,  1, "See help `kick` for help." },
    { "grapple",                 CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `grapple` for help." },
    { "trip",                    CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `trip` for help." },
    { "doorbash",                CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `doorbash` for help." },
    { "dual wield",              CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `dual wield` for help." },
    { "power move",              CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `power move` for help." },
    { "two-handed specialization", CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `two-handed specialization` for help." },
    { "fortify",                 CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `fortify` for help." },
    { "rescue",                  CLASS_WARRIOR, SKILL_TIER_CLASS, 1, "See help `rescue` for help." },
    { "repair",                  CLASS_WARRIOR, SKILL_TIER_CLASS, 5, "See help `repair` for help." },
    /* Missing-skill audit (TODO.md, 2026-08-05): real upstream has a
     * separate SKILL_REPAIR_CLERIC/SKILL_REPAIR_MONK/SKILL_REPAIR_THIEF
     * per class, all funneling into the same repairMeHammer()-style
     * mechanic Tobin's own `repair` (Warrior, above) already ports.
     * Rather than duplicate the command under three more names, these
     * classes get the SAME "repair" skill (cmd_repair.c gates on
     * being_knows_skill(ch, "repair") + skill_find(ch->char_class,
     * "repair", ...), so a same-named roster row for another class
     * just works, no command changes needed). "Blacksmithing" itself
     * (Warrior) is this exact skill under its real-upstream name --
     * already fully covered, not a separate missing entry. */
    { "repair",                  CLASS_CLERIC, SKILL_TIER_CLASS, 1, "See help `repair` for help." },
    { "repair",                  CLASS_MONK, SKILL_TIER_CLASS,    1, "See help `repair` for help." },
    { "repair",                  CLASS_THIEF, SKILL_TIER_CLASS,   1, "See help `repair` for help." },
    /* Missing-skill audit: real upstream SKILL_BLACKSMITHING_ADVANCED
     * (Warrior) -- gates repairing crystalline materials, a material-
     * property distinction Tobin's own `repair` doc comment already
     * disclosed it doesn't model. Real, working difference ported
     * instead: an advanced blacksmith's repairs don't wear the item
     * down at all (cmd_repair.c skips the depreciation increment when
     * this is known), vs. the base `repair` skill's permanent -1 every
     * time. */
    { "advanced blacksmithing",  CLASS_WARRIOR, SKILL_TIER_ADVANCED,  25, "See help `advanced blacksmithing` for help." },
    /* Missing-skill audit: real upstream SKILL_DEBRIDE (Warrior,
     * disc_warrior_blacksmithing.cc's doDebride()) strips an ITEM_RUSTY
     * flag Tobin has no equivalent for. Real, working reuse instead:
     * reduces an item's depreciation by 1 (the exact inverse of what
     * `repair` increases), a genuine "undo some of that wear" effect --
     * cmd_repair.c's own doc comment already covers depreciation as a
     * real, working field. */
    { "debride",                  CLASS_WARRIOR, SKILL_TIER_ADVANCED,  25, "See help `debride` for help." },
    /* Missing-skill audit: real upstream SKILL_BLOODLUST (Warrior,
     * disc_warrior_brawling.cc) is a passive per-round chance for a
     * stacking damage buff while fighting. Ported as a flat passive
     * hitroll/damroll bonus scaling with proficiency, same shape as the
     * weapon-specialization bonuses combat_strike() already applies
     * (kubo/cintai/spec_prof) -- real, working, no separate stacking-
     * affect infrastructure needed. */
    { "bloodlust",                CLASS_WARRIOR, SKILL_TIER_ADVANCED,  25, "See help `bloodlust` for help." },
    /* Missing-skill audit: real upstream SKILL_STOMP (Warrior,
     * disc_warrior_brawling.cc) is a leg/foot melee attack, part of the
     * berserk auto-proc pool. Ported as its own standalone command
     * (cmd_stomp.c), same shape as `kick`. */
    { "stomp",                     CLASS_WARRIOR, SKILL_TIER_ADVANCED,  29, "See help `stomp` for help." },
    { "focus attack",            CLASS_WARRIOR, SKILL_TIER_CLASS,   5, "See help `focus attack` for help." },
    { "shove",                   CLASS_WARRIOR, SKILL_TIER_CLASS,   6, "See help `shove` for help." },
    { "bodyslam",                CLASS_WARRIOR, SKILL_TIER_ADVANCED,   28, "See help `bodyslam` for help." },
    { "headbutt",                CLASS_WARRIOR, SKILL_TIER_ADVANCED,   27, "See help `headbutt` for help." },
    { "spin",                    CLASS_WARRIOR, SKILL_TIER_ADVANCED,   30, "See help `spin` for help." },
    { "disarm",                  CLASS_WARRIOR, SKILL_TIER_CLASS,   5, "See help `disarm` for help." },
    { "advanced berserking",     CLASS_WARRIOR, SKILL_TIER_ADVANCED,   35, "See help `advanced berserking` for help." },
    { "slam",                    CLASS_WARRIOR, SKILL_TIER_ADVANCED,   25, "See help `slam` for help." },
    { "riposte",                 CLASS_WARRIOR, SKILL_TIER_ADVANCED,   25, "See help `riposte` for help." },
    { "deathstroke",             CLASS_WARRIOR, SKILL_TIER_ADVANCED,   35, "See help `deathstroke` for help." },
    { "taunt",                   CLASS_WARRIOR, SKILL_TIER_ADVANCED,   30, "See help `taunt` for help." },
    { "whirlwind",               CLASS_WARRIOR, SKILL_TIER_ADVANCED,   25, "See help `whirlwind` for help." },
    { "kneestrike",              CLASS_WARRIOR, SKILL_TIER_CLASS,   25, "See help `kneestrike` for help." },
    { "switch opponents",        CLASS_WARRIOR, SKILL_TIER_CLASS,   25, "See help `switch opponents` for help." },
    { "trance of blades",        CLASS_WARRIOR, SKILL_TIER_ADVANCED, 25, "See help `trance of blades` for help." },
    { "weapon retention",        CLASS_WARRIOR, SKILL_TIER_ADVANCED, 25, "See help `weapon retention` for help." },
    { "brawl avoidance",         CLASS_WARRIOR, SKILL_TIER_ADVANCED, 25, "See help `brawl avoidance` for help." },
    { "close quarters fighting", CLASS_WARRIOR, SKILL_TIER_ADVANCED, 50, "See help `close quarters fighting` for help." },
    { "slash proficiency",    CLASS_WARRIOR, SKILL_TIER_COMBAT,  1, "See help `slash proficiency` for help." },
    { "blunt proficiency",    CLASS_WARRIOR, SKILL_TIER_COMBAT,  1, "See help `blunt proficiency` for help." },
    { "pierce proficiency",   CLASS_WARRIOR, SKILL_TIER_COMBAT,  1, "See help `pierce proficiency` for help." },
    { "barehand proficiency", CLASS_WARRIOR, SKILL_TIER_COMBAT,  1, "See help `barehand proficiency` for help." },
    { "ranged proficiency",   CLASS_WARRIOR, SKILL_TIER_ADVANCED, 25, "See help `ranged proficiency` for help." },

    /* ---------------- THIEF ---------------- */
    { "riding",           CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `riding` for help." },
    { "sign",             CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `sign` for help." },
    { "kick",             CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `kick` for help." },
    { "retreat",          CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `retreat` for help." },
    { "backstab",         CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `backstab` for help." },
    /* Missing-skill audit: real upstream SKILL_SWINDLE (stats.cc's
     * getSwindleBonus()) discounts shop buy prices and pads sell
     * prices. Ported into cmd_shop.c's own price formulas, scaling
     * with skill_proficiency(). */
    { "swindle",          CLASS_THIEF, SKILL_TIER_CLASS,  10, "See help `swindle` for help." },
    { "dodge",            CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `dodge` for help." },
    { "garrotte",         CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `garrotte` for help." },
    { "throatslit",       CLASS_THIEF, SKILL_TIER_ADVANCED,  26, "See help `throatslit` for help." },
    { "poison weapon",    CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `poison weapon` for help." },
    { "steal",            CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `steal` for help." },
    { "peek",             CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `peek` for help." },
    { "search",           CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `search` for help." },
    { "detect trap",      CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `detect trap` for help." },
    { "counter steal",    CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `counter steal` for help." },
    { "sneak",            CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `sneak` for help." },
    { "concealment",      CLASS_THIEF, SKILL_TIER_ADVANCED,  30, "See help `concealment` for help." },
    { "disguise",         CLASS_THIEF, SKILL_TIER_ADVANCED,  30, "See help `disguise` for help." },
    { "skulk",            CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `skulk` for help." },
    { "set trap (arrow)", CLASS_THIEF, SKILL_TIER_ADVANCED,  26, "See help `set trap (arrow)` for help." },
    { "set trap (container)", CLASS_THIEF, SKILL_TIER_ADVANCED, 27, "See help `set trap (container)` for help." },
    { "track",            CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `track` for help." },
    { "switch opponents", CLASS_THIEF, SKILL_TIER_CLASS,  1, "See help `switch opponents` for help." },
    { "set trap (door)",  CLASS_THIEF, SKILL_TIER_ADVANCED,  29, "See help `set trap (door)` for help." },
    { "pick lock",        CLASS_THIEF, SKILL_TIER_ADVANCED,  31, "See help `pick lock` for help." },
    { "stabbing",         CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `stabbing` for help." },
    { "disarm",           CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `disarm` for help." },
    { "disarm trap",      CLASS_THIEF, SKILL_TIER_ADVANCED,  25, "See help `disarm trap` for help." },
    { "subterfuge",       CLASS_THIEF, SKILL_TIER_ADVANCED, 25, "See help `subterfuge` for help." },
    { "hide",             CLASS_THIEF, SKILL_TIER_ADVANCED, 31, "See help `hide` for help." },
    { "plant",            CLASS_THIEF, SKILL_TIER_ADVANCED, 31, "See help `plant` for help." },
    { "dual wield",       CLASS_THIEF, SKILL_TIER_ADVANCED, 31, "See help `dual wield` for help." },
    { "set trap (mine)",  CLASS_THIEF, SKILL_TIER_ADVANCED, 37, "See help `set trap (mine)` for help." },
    { "spy",              CLASS_THIEF, SKILL_TIER_ADVANCED, 38, "See help `spy` for help." },
    { "cudgel",           CLASS_THIEF, SKILL_TIER_ADVANCED, 41, "See help `cudgel` for help." },
    { "set trap (grenade)", CLASS_THIEF, SKILL_TIER_ADVANCED, 50, "See help `set trap (grenade)` for help." },
    { "slash proficiency",    CLASS_THIEF, SKILL_TIER_COMBAT,  1, "See help `slash proficiency` for help." },
    { "blunt proficiency",    CLASS_THIEF, SKILL_TIER_COMBAT,  1, "See help `blunt proficiency` for help." },
    { "pierce proficiency",   CLASS_THIEF, SKILL_TIER_COMBAT,  1, "See help `pierce proficiency` for help." },
    { "barehand proficiency", CLASS_THIEF, SKILL_TIER_COMBAT,  1, "See help `barehand proficiency` for help." },
    { "ranged proficiency",   CLASS_THIEF, SKILL_TIER_ADVANCED, 25, "See help `ranged proficiency` for help." },

    /* ---------------- MONK ---------------- */
    { "riding",          CLASS_MONK, SKILL_TIER_CLASS,  1, "See help `riding` for help." },
    { "sign",            CLASS_MONK, SKILL_TIER_CLASS,  1, "See help `sign` for help." },
    { "disarm",          CLASS_MONK, SKILL_TIER_CLASS,  1, "See help `disarm` for help." },
    { "kick",            CLASS_MONK, SKILL_TIER_CLASS,  1, "See help `kick` for help." },
    { "groundfighting",  CLASS_MONK, SKILL_TIER_CLASS, 5, "See help `groundfighting` for help." },
    /* Missing-skill audit: real upstream SKILL_OOMLAT ("Oomlat
     * Philosophy", disc_monk_meditation.cc) -- a passive discipline
     * that improves armor class the more it's practiced. Ported as a
     * real being_total_ac() bonus (being.c), scaling with
     * skill_proficiency(). */
    { "Oomlat Philosophy", CLASS_MONK, SKILL_TIER_ADVANCED, 25, "See help `Oomlat Philosophy` for help." },
    { "retreat",         CLASS_MONK, SKILL_TIER_ADVANCED, 25, "See help `retreat` for help." },
    { "counter move",    CLASS_MONK, SKILL_TIER_ADVANCED, 25, "See help `counter move` for help." },
    { "switch opponents", CLASS_MONK, SKILL_TIER_ADVANCED, 25, "See help `switch opponents` for help." },
    { "chop",            CLASS_MONK, SKILL_TIER_ADVANCED, 25, "See help `chop` for help." },
    { "yoginsa",         CLASS_MONK, SKILL_TIER_CLASS,   1, "See help `yoginsa` for help." },
    { "jirin",           CLASS_MONK, SKILL_TIER_CLASS,   1, "See help `jirin` for help." },
    { "kubo",            CLASS_MONK, SKILL_TIER_CLASS,   1, "See help `kubo` for help." },
    { "chi",             CLASS_MONK, SKILL_TIER_CLASS,   1, "See help `chi` for help." },
    { "oomlat",          CLASS_MONK, SKILL_TIER_CLASS,   1, "See help `oomlat` for help." },
    { "catfall",         CLASS_MONK, SKILL_TIER_ADVANCED,   25, "See help `catfall` for help." },
    { "catleap",         CLASS_MONK, SKILL_TIER_ADVANCED,   25, "See help `catleap` for help." },
    { "cintai",          CLASS_MONK, SKILL_TIER_CLASS,   5, "See help `cintai` for help." },
    { "springleap",      CLASS_MONK, SKILL_TIER_ADVANCED,  30, "See help `springleap` for help." },
    { "advanced kicking", CLASS_MONK, SKILL_TIER_ADVANCED, 25, "See help `advanced kicking` for help." },
    { "iron fist",           CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `iron fist` for help." },
    { "hurl",                CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `hurl` for help." },
    { "chain attack",        CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `chain attack` for help." },
    { "critical hitting",    CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `critical hitting` for help." },
    { "wohlin meditation",   CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `wohlin meditation` for help." },
    { "voplat",              CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `voplat` for help." },
    { "blindfighting",       CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `blindfighting` for help." },
    { "feign death",         CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `feign death` for help." },
    { "blur",                CLASS_MONK, SKILL_TIER_ADVANCED,  25, "See help `blur` for help." },
    { "iron flesh",          CLASS_MONK, SKILL_TIER_ADVANCED, 31, "See help `iron flesh` for help." },
    { "iron skin",           CLASS_MONK, SKILL_TIER_ADVANCED, 35, "See help `iron skin` for help." },
    { "shoulder throw",      CLASS_MONK, SKILL_TIER_ADVANCED, 36, "See help `shoulder throw` for help." },
    { "iron bones",          CLASS_MONK, SKILL_TIER_ADVANCED, 38, "See help `iron bones` for help." },
    { "snofalte",            CLASS_MONK, SKILL_TIER_ADVANCED, 38, "See help `snofalte` for help." },
    { "iron muscles",        CLASS_MONK, SKILL_TIER_ADVANCED, 42, "See help `iron muscles` for help." },
    { "defenestrate",        CLASS_MONK, SKILL_TIER_ADVANCED, 42, "See help `defenestrate` for help." },
    { "quivering palm",      CLASS_MONK, SKILL_TIER_ADVANCED, 42, "See help `quivering palm` for help." },
    { "iron legs",           CLASS_MONK, SKILL_TIER_ADVANCED, 45, "See help `iron legs` for help." },
    { "iron will",           CLASS_MONK, SKILL_TIER_ADVANCED, 48, "See help `iron will` for help." },
    { "dufali",              CLASS_MONK, SKILL_TIER_ADVANCED, 48, "See help `dufali` for help." },
    { "bonebreak",           CLASS_MONK, SKILL_TIER_ADVANCED, 50, "See help `bonebreak` for help." },
    { "slash proficiency",    CLASS_MONK, SKILL_TIER_COMBAT,  1, "See help `slash proficiency` for help." },
    { "blunt proficiency",    CLASS_MONK, SKILL_TIER_COMBAT,  1, "See help `blunt proficiency` for help." },
    { "pierce proficiency",   CLASS_MONK, SKILL_TIER_COMBAT,  1, "See help `pierce proficiency` for help." },
    { "barehand proficiency", CLASS_MONK, SKILL_TIER_COMBAT,  1, "See help `barehand proficiency` for help." },
    { "ranged proficiency",   CLASS_MONK, SKILL_TIER_ADVANCED, 25, "See help `ranged proficiency` for help." },

    /* ---------------- CLERIC ---------------- */
    { "riding",               CLASS_CLERIC, SKILL_TIER_CLASS,  1, "See help `riding` for help." },
    { "sign",                 CLASS_CLERIC, SKILL_TIER_CLASS,  10, "See help `sign` for help." },
    { "slash proficiency",    CLASS_CLERIC, SKILL_TIER_COMBAT,  1, "See help `slash proficiency` for help." },
    { "blunt proficiency",    CLASS_CLERIC, SKILL_TIER_COMBAT,  1, "See help `blunt proficiency` for help." },
    { "pierce proficiency",   CLASS_CLERIC, SKILL_TIER_COMBAT,  1, "See help `pierce proficiency` for help." },
    { "barehand proficiency", CLASS_CLERIC, SKILL_TIER_COMBAT,  1, "See help `barehand proficiency` for help." },
    { "ranged proficiency",   CLASS_CLERIC, SKILL_TIER_ADVANCED, 25, "See help `ranged proficiency` for help." },
    { "heal light",       CLASS_CLERIC, SKILL_TIER_CLASS,  1, "See help `heal light` for help." },
    { "harm light",       CLASS_CLERIC, SKILL_TIER_CLASS,  1, "See help `harm light` for help." },
    { "armor",            CLASS_CLERIC, SKILL_TIER_CLASS,  1, "See help `armor` for help." },
    { "bless",            CLASS_CLERIC, SKILL_TIER_CLASS,  1, "See help `bless` for help." },
    { "attune",           CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "See help `attune` for help." },
    { "devotion",         CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "See help `devotion` for help." },
    { "penance",          CLASS_CLERIC, SKILL_TIER_CLASS,  1, "See help `penance` for help." },
    { "clot",             CLASS_CLERIC, SKILL_TIER_CLASS,  2, "See help `clot` for help." },
    /* Missing-spell audit: real upstream discArray[SPELL_STERILIZE]
     * (disc_cleric_cures.cc) -- cures a PART_INFECTED limb flag. Tobin
     * already has a matching real affect, AFFECT_DISEASE_INFECTION
     * (affect.h) -- same targeted-cure shape as clot/remove curse
     * above, just a different specific disease. */
    { "sterilize",        CLASS_CLERIC, SKILL_TIER_CLASS,  6, "See help `sterilize` for help." },
    { "create food",      CLASS_CLERIC, SKILL_TIER_CLASS, 1, "See help `create food` for help." },
    { "create water",     CLASS_CLERIC, SKILL_TIER_CLASS, 1, "See help `create water` for help." },
    { "cure poison",      CLASS_CLERIC, SKILL_TIER_CLASS, 15, "See help `cure poison` for help." },
    { "salve",            CLASS_CLERIC, SKILL_TIER_CLASS, 20, "See help `salve` for help." },
    { "heal serious",     CLASS_CLERIC, SKILL_TIER_CLASS, 5, "See help `heal serious` for help." },
    { "rain brimstone",   CLASS_CLERIC, SKILL_TIER_CLASS, 5, "See help `rain brimstone` for help." },
    { "remove curse",     CLASS_CLERIC, SKILL_TIER_CLASS, 7, "See help `remove curse` for help." },
    { "cure disease",     CLASS_CLERIC, SKILL_TIER_CLASS, 8, "See help `cure disease` for help." },
    { "refresh",          CLASS_CLERIC, SKILL_TIER_CLASS, 9, "See help `refresh` for help." },
    { "heal critical",    CLASS_CLERIC, SKILL_TIER_CLASS, 10, "See help `heal critical` for help." },
    { "harm serious",     CLASS_CLERIC, SKILL_TIER_CLASS, 10, "See help `harm serious` for help." },
    { "cure blindness",   CLASS_CLERIC, SKILL_TIER_CLASS, 12, "See help `cure blindness` for help." },
    { "flamestrike",      CLASS_CLERIC, SKILL_TIER_CLASS, 13, "See help `flamestrike` for help." },
    { "curse",            CLASS_CLERIC, SKILL_TIER_CLASS, 13, "See help `curse` for help." },
    { "expel",            CLASS_CLERIC, SKILL_TIER_CLASS, 13, "See help `expel` for help." },
    { "harm critical",    CLASS_CLERIC, SKILL_TIER_CLASS, 14, "See help `harm critical` for help." },
    { "disease",          CLASS_CLERIC, SKILL_TIER_CLASS, 14, "See help `disease` for help." },
    { "poison",           CLASS_CLERIC, SKILL_TIER_CLASS, 15, "See help `poison` for help." },
    { "numb",             CLASS_CLERIC, SKILL_TIER_CLASS, 15, "See help `numb` for help." },
    { "infect",           CLASS_CLERIC, SKILL_TIER_CLASS, 16, "See help `infect` for help." },
    { "heal",             CLASS_CLERIC, SKILL_TIER_CLASS, 17, "See help `heal` for help." },
    { "summon",           CLASS_CLERIC, SKILL_TIER_CLASS, 19, "See help `summon` for help." },
    { "harm",             CLASS_CLERIC, SKILL_TIER_CLASS, 20, "See help `harm` for help." },
    { "plague of locusts", CLASS_CLERIC, SKILL_TIER_ADVANCED, 25, "See help `plague of locusts` for help." },
    { "word of recall",   CLASS_CLERIC, SKILL_TIER_ADVANCED, 26, "See help `word of recall` for help." },
    { "blindness",        CLASS_CLERIC, SKILL_TIER_ADVANCED, 26, "See help `blindness` for help." },
    { "paralyze limb",    CLASS_CLERIC, SKILL_TIER_ADVANCED, 27, "See help `paralyze limb` for help." },
    { "knit bone",        CLASS_CLERIC, SKILL_TIER_ADVANCED, 27, "See help `knit bone` for help." },
    { "sanctuary",        CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "See help `sanctuary` for help." },
    { "bleed",            CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "See help `bleed` for help." },
    { "restore limb",     CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "See help `restore limb` for help." },
    { "heroes' feast",    CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "See help `heroes' feast` for help." },
    { "pillar of salt",   CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "See help `pillar of salt` for help." },
    { "second wind",      CLASS_CLERIC, SKILL_TIER_ADVANCED, 33, "See help `second wind` for help." },
    { "paralyze",         CLASS_CLERIC, SKILL_TIER_ADVANCED, 33, "See help `paralyze` for help." },
    { "earthquake",       CLASS_CLERIC, SKILL_TIER_ADVANCED, 33, "See help `earthquake` for help." },
    { "heal full",        CLASS_CLERIC, SKILL_TIER_ADVANCED, 28, "See help `heal full` for help." },
    { "cure paralysis",   CLASS_CLERIC, SKILL_TIER_ADVANCED, 37, "See help `cure paralysis` for help." },
    { "heal critical spray", CLASS_CLERIC, SKILL_TIER_ADVANCED, 39, "See help `heal critical spray` for help." },
    { "astral walk",      CLASS_CLERIC, SKILL_TIER_ADVANCED, 35, "See help `astral walk` for help." },
    { "bone breaker",     CLASS_CLERIC, SKILL_TIER_ADVANCED, 40, "See help `bone breaker` for help." },
    { "call lightning",   CLASS_CLERIC, SKILL_TIER_ADVANCED, 40, "See help `call lightning` for help." },
    { "consecrate",       CLASS_CLERIC, SKILL_TIER_ADVANCED, 43, "See help `consecrate` for help." },
    { "heal spray",       CLASS_CLERIC, SKILL_TIER_ADVANCED, 43, "See help `heal spray` for help." },
    { "wither limb",      CLASS_CLERIC, SKILL_TIER_ADVANCED, 48, "See help `wither limb` for help." },
    { "spontaneous combust", CLASS_CLERIC, SKILL_TIER_ADVANCED, 48, "See help `spontaneous combust` for help." },
    { "relive",           CLASS_CLERIC, SKILL_TIER_ADVANCED, 49, "See help `relive` for help." },
    { "crusade",          CLASS_CLERIC, SKILL_TIER_ADVANCED, 49, "See help `crusade` for help." },
    { "portal",           CLASS_CLERIC, SKILL_TIER_ADVANCED, 49, "See help `portal` for help." },
    { "heal full spray",  CLASS_CLERIC, SKILL_TIER_ADVANCED, 50, "See help `heal full spray` for help." },
    /* Pet/charm (Sneezy → Tobin feature audit) -- new, not a Sneezy-named
     * port like the Mage "conjure elemental" spells below (Cleric has no
     * real upstream pet-summon prayer of its own), but reuses the same
     * real seeded "swarm locusts cloud" mob (vnum 7852) as a fitting
     * Cleric flavor -- a plague made obedient, distinct from the
     * already-implemented "plague of locusts" AoE damage spell above. */
    { "summon swarm",     CLASS_CLERIC, SKILL_TIER_ADVANCED, 30, "See help `summon swarm` for help." },

    /* ---------------- MAGE ---------------- */
    { "riding",               CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `riding` for help." },
    { "sign",                 CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `sign` for help." },
    { "slash proficiency",    CLASS_MAGE, SKILL_TIER_COMBAT,  1, "See help `slash proficiency` for help." },
    { "blunt proficiency",    CLASS_MAGE, SKILL_TIER_COMBAT,  1, "See help `blunt proficiency` for help." },
    { "pierce proficiency",   CLASS_MAGE, SKILL_TIER_COMBAT,  1, "See help `pierce proficiency` for help." },
    { "barehand proficiency", CLASS_MAGE, SKILL_TIER_COMBAT,  1, "See help `barehand proficiency` for help." },
    { "ranged proficiency",   CLASS_MAGE, SKILL_TIER_ADVANCED, 25, "See help `ranged proficiency` for help." },
    { "wizardry",         CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `wizardry` for help." },
    { "mana",             CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `mana` for help." },
    { "meditate",         CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `meditate` for help." },
    { "gust",             CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `gust` for help." },
    { "sling shot",       CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `sling shot` for help." },
    { "gusher",           CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `gusher` for help." },
    { "sorcerer's globe", CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `sorcerer's globe` for help." },
    { "mage sight",       CLASS_MAGE, SKILL_TIER_CLASS,  1, "See help `mage sight` for help." },
    { "flare",            CLASS_MAGE, SKILL_TIER_CLASS,  3, "See help `flare` for help." },
    { "hands of flame",   CLASS_MAGE, SKILL_TIER_CLASS, 4, "See help `hands of flame` for help." },
    { "mystic darts",     CLASS_MAGE, SKILL_TIER_CLASS, 5, "See help `mystic darts` for help." },
    { "illuminate",       CLASS_MAGE, SKILL_TIER_CLASS,  2, "See help `illuminate` for help." },
    /* Missing-spell audit (TODO.md, 2026-08-05): real upstream
     * discArray[SPELL_PROTECTION_FROM_EARTH] (spell_info.cc) -- Mage,
     * DISC_EARTH, TAR_AREA, "You feel slightly less protected." on
     * wear-off. Never had a Tobin roster entry at all (not a stub --
     * genuinely absent). */
    { "protection from earth", CLASS_MAGE, SKILL_TIER_CLASS, 8, "See help `protection from earth` for help." },
    { "faerie fire",      CLASS_MAGE, SKILL_TIER_CLASS, 6, "See help `faerie fire` for help." },
    { "materialize",      CLASS_MAGE, SKILL_TIER_CLASS, 6, "See help `materialize` for help." },
    { "pebble spray",     CLASS_MAGE, SKILL_TIER_CLASS, 6, "See help `pebble spray` for help." },
    { "arctic blast",     CLASS_MAGE, SKILL_TIER_CLASS, 6, "See help `arctic blast` for help." },
    { "feathery descent", CLASS_MAGE, SKILL_TIER_CLASS, 7, "See help `feathery descent` for help." },
    { "gills of flesh",   CLASS_MAGE, SKILL_TIER_CLASS, 9, "See help `gills of flesh` for help." },
    { "accelerate",       CLASS_MAGE, SKILL_TIER_CLASS, 11, "See help `accelerate` for help." },
    { "levitate",         CLASS_MAGE, SKILL_TIER_CLASS, 11, "See help `levitate` for help." },
    { "dust storm",       CLASS_MAGE, SKILL_TIER_CLASS, 11, "See help `dust storm` for help." },
    { "stunning arrow",   CLASS_MAGE, SKILL_TIER_CLASS, 12, "See help `stunning arrow` for help." },
    { "color spray",      CLASS_MAGE, SKILL_TIER_CLASS, 12, "See help `color spray` for help." },
    { "granite fists",    CLASS_MAGE, SKILL_TIER_CLASS, 11, "See help `granite fists` for help." },
    { "slumber",          CLASS_MAGE, SKILL_TIER_CLASS, 13, "See help `slumber` for help." },
    { "identify",         CLASS_MAGE, SKILL_TIER_CLASS, 14, "See help `identify` for help." },
    { "icy grip",         CLASS_MAGE, SKILL_TIER_CLASS, 14, "See help `icy grip` for help." },
    { "sense life",       CLASS_MAGE, SKILL_TIER_CLASS, 14, "See help `sense life` for help." },
    { "tornado",          CLASS_MAGE, SKILL_TIER_CLASS, 15, "See help `tornado` for help." },
    { "fear",             CLASS_MAGE, SKILL_TIER_CLASS, 14, "See help `fear` for help." },
    { "stealth",          CLASS_MAGE, SKILL_TIER_CLASS, 16, "See help `stealth` for help." },
    { "telepathy",        CLASS_MAGE, SKILL_TIER_CLASS, 16, "See help `telepathy` for help." },
    { "faerie fog",       CLASS_MAGE, SKILL_TIER_CLASS, 18, "See help `faerie fog` for help." },
    { "dispel invisible", CLASS_MAGE, SKILL_TIER_CLASS, 17, "See help `dispel invisible` for help." },
    { "galvanize",        CLASS_MAGE, SKILL_TIER_CLASS, 19, "See help `galvanize` for help." },
    { "powerstone",       CLASS_MAGE, SKILL_TIER_CLASS, 19, "See help `powerstone` for help." },
    { "invisibility",     CLASS_MAGE, SKILL_TIER_CLASS, 17, "See help `invisibility` for help." },
    { "flaming sword",    CLASS_MAGE, SKILL_TIER_CLASS, 19, "See help `flaming sword` for help." },
    { "sand blast",       CLASS_MAGE, SKILL_TIER_CLASS, 19, "See help `sand blast` for help." },
    { "ice storm",        CLASS_MAGE, SKILL_TIER_CLASS, 19, "See help `ice storm` for help." },
    { "teleport",         CLASS_MAGE, SKILL_TIER_CLASS, 19, "See help `teleport` for help." },
    { "dispel magic",     CLASS_MAGE, SKILL_TIER_CLASS, 20, "See help `dispel magic` for help." },
    { "conjure elemental air", CLASS_MAGE, SKILL_TIER_CLASS, 12, "See help `conjure elemental air` for help." },
    { "ensorcer",         CLASS_MAGE, SKILL_TIER_CLASS, 15, "See help `ensorcer` for help." },
    { "conjure elemental fire", CLASS_MAGE, SKILL_TIER_CLASS, 19, "See help `conjure elemental fire` for help." },
    { "fireball",         CLASS_MAGE, SKILL_TIER_CLASS, 14, "See help `fireball` for help." },
    { "conjure elemental earth", CLASS_MAGE, SKILL_TIER_CLASS, 21, "See help `conjure elemental earth` for help." },
    { "falcon wings",     CLASS_MAGE, SKILL_TIER_CLASS, 18, "See help `falcon wings` for help." },
    { "eyes of Fertuman",  CLASS_MAGE, SKILL_TIER_CLASS, 22, "See help `eyes of Fertuman` for help." },
    { "enhance weapon",   CLASS_MAGE, SKILL_TIER_CLASS, 24, "See help `enhance weapon` for help." },
    { "conjure elemental water", CLASS_MAGE, SKILL_TIER_CLASS, 24, "See help `conjure elemental water` for help." },
    { "copy",             CLASS_MAGE, SKILL_TIER_CLASS, 23, "See help `copy` for help." },
    { "farlook",          CLASS_MAGE, SKILL_TIER_ADVANCED, 25, "See help `farlook` for help." },
    { "acid blast",       CLASS_MAGE, SKILL_TIER_ADVANCED, 25, "See help `acid blast` for help." },
    { "haste",            CLASS_MAGE, SKILL_TIER_ADVANCED, 26, "See help `haste` for help." },
    { "mage repair",      CLASS_MAGE, SKILL_TIER_CLASS, 10, "See help `mage repair` for help." },
    { "calm",             CLASS_MAGE, SKILL_TIER_CLASS, 19, "See help `calm` for help." },
    /* Missing-spell audit: real upstream discArray[SPELL_INFERNO] --
     * Mage, DISC_FIRE, TAR_CHAR_ROOM|TAR_VIOLENT|TAR_FIGHT_VICT,
     * "The burning sensation on your skin fades away." wear-off (a
     * DoT in the original; Tobin has no fire-specific DoT resource, so
     * this ports as a real instant strike, same disclosed "no
     * elemental damage-type system" scope-cut as every other
     * elemental-flavored attack spell this audit has ported). */
    { "inferno",          CLASS_MAGE, SKILL_TIER_ADVANCED, 31, "See help `inferno` for help." },
    { "detect magic",        CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `detect magic` for help." },
    { "protection from air",  CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `protection from air` for help." },
    { "protection from fire", CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `protection from fire` for help." },
    { "protection from water", CLASS_MAGE, SKILL_TIER_ADVANCED, 25, "See help `protection from water` for help." },
    { "protection from energy", CLASS_MAGE, SKILL_TIER_ADVANCED, 25, "See help `protection from energy` for help." },
    { "stone skin",           CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `stone skin` for help." },
    { "flaming flesh",        CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `flaming flesh` for help." },
    { "trail seek",           CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `trail seek` for help." },
    { "scribe",               CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `scribe` for help." },
    { "charge stave",         CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `charge stave` for help." },
    { "animate",              CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `animate` for help." },
    { "bind",                 CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `bind` for help." },
    { "detect invisibility",  CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "See help `detect invisibility` for help." },
    { "shatter",              CLASS_MAGE, SKILL_TIER_ADVANCED,  27, "See help `shatter` for help." },
    { "infravision",          CLASS_MAGE, SKILL_TIER_ADVANCED, 28, "See help `infravision` for help." },
    { "true sight",           CLASS_MAGE, SKILL_TIER_ADVANCED, 28, "See help `true sight` for help." },
    { "lightning bolt",       CLASS_MAGE, SKILL_TIER_ADVANCED, 31, "See help `lightning bolt` for help." },
    { "cloud of concealment", CLASS_MAGE, SKILL_TIER_ADVANCED, 31, "See help `cloud of concealment` for help." },
    { "watery grave",         CLASS_MAGE, SKILL_TIER_ADVANCED, 32, "See help `watery grave` for help." },
    { "spontaneous generation", CLASS_MAGE, SKILL_TIER_ADVANCED, 32, "See help `spontaneous generation` for help." },
    { "blast of fury",        CLASS_MAGE, SKILL_TIER_ADVANCED, 31, "See help `blast of fury` for help." },
    { "Garmul's tail",        CLASS_MAGE, SKILL_TIER_ADVANCED,  26, "See help `Garmul's tail` for help." },
    { "plasma mirror",        CLASS_MAGE, SKILL_TIER_ADVANCED, 34, "See help `plasma mirror` for help." },
    { "immobilize",           CLASS_MAGE, SKILL_TIER_ADVANCED, 36, "See help `immobilize` for help." },
    { "energy drain",         CLASS_MAGE, SKILL_TIER_ADVANCED, 36, "See help `energy drain` for help." },
    { "polymorph",            CLASS_MAGE, SKILL_TIER_ADVANCED, 36, "See help `polymorph` for help." },
    { "meteor swarm",         CLASS_MAGE, SKILL_TIER_ADVANCED, 39, "See help `meteor swarm` for help." },
    { "tsunami",              CLASS_MAGE, SKILL_TIER_ADVANCED, 39, "See help `tsunami` for help." },
    { "blizzard",             CLASS_MAGE, SKILL_TIER_ADVANCED, 39, "See help `blizzard` for help." },
    { "fumble",               CLASS_MAGE, SKILL_TIER_ADVANCED, 42, "See help `fumble` for help." },
    { "suffocate",            CLASS_MAGE, SKILL_TIER_ADVANCED, 42, "See help `suffocate` for help." },
    { "pierce resistance",    CLASS_MAGE, SKILL_TIER_ADVANCED, 47, "See help `pierce resistance` for help." },
    { "flight",               CLASS_MAGE, SKILL_TIER_ADVANCED, 34, "See help `flight` for help." },
    { "divination",           CLASS_MAGE, SKILL_TIER_ADVANCED, 45, "See help `divination` for help." },
    { "sand blast",           CLASS_MAGE, SKILL_TIER_ADVANCED, 42, "See help `sand blast` for help." },
    { "lava stream",          CLASS_MAGE, SKILL_TIER_ADVANCED, 47, "See help `lava stream` for help." },
    { "lava lance",           CLASS_MAGE, SKILL_TIER_ADVANCED, 50, "See help `lava lance` for help." },
    { "hellfire",             CLASS_MAGE, SKILL_TIER_ADVANCED, 47, "See help `hellfire` for help." },
    { "atomize",              CLASS_MAGE, SKILL_TIER_ADVANCED, 48, "See help `atomize` for help." },
    { "silence",              CLASS_MAGE, SKILL_TIER_ADVANCED, 48, "See help `silence` for help." },
    { "ethereal gate",        CLASS_MAGE, SKILL_TIER_ADVANCED, 48, "See help `ethereal gate` for help." },
    { "chain lightning",      CLASS_MAGE, SKILL_TIER_ADVANCED, 48, "See help `chain lightning` for help." },
    { "knot",                 CLASS_MAGE, SKILL_TIER_ADVANCED, 50, "See help `knot` for help." },

    /* ---------------- DRUID ---------------- */
    /* Custom blend, not a direct Sneezy class port (user 2026-07-11:
     * "druid class will be tricky, we will take some ranger skills and
     * cleric ability, and throw in some shaman damage reworded for
     * druids"): Ranger's real (non-stub) nature/animal skills, a subset
     * of Cleric's heal/utility ladder, and several of Shaman's working
     * damage spells renamed/reflavored to a nature theme instead of
     * their original voodoo/loa styling. Shaman's totem/golem/undead-
     * thrall/possession lines were deliberately left out -- thematically
     * a poor fit regardless of renaming. */
    { "riding",               CLASS_DRUID, SKILL_TIER_CLASS,  1, "See help `riding` for help." },
    { "sign",                 CLASS_DRUID, SKILL_TIER_CLASS,  1, "See help `sign` for help." },
    { "slash proficiency",    CLASS_DRUID, SKILL_TIER_COMBAT,  1, "See help `slash proficiency` for help." },
    { "blunt proficiency",    CLASS_DRUID, SKILL_TIER_COMBAT,  1, "See help `blunt proficiency` for help." },
    { "pierce proficiency",   CLASS_DRUID, SKILL_TIER_COMBAT,  1, "See help `pierce proficiency` for help." },
    { "barehand proficiency", CLASS_DRUID, SKILL_TIER_COMBAT,  1, "See help `barehand proficiency` for help." },
    { "ranged proficiency",   CLASS_DRUID, SKILL_TIER_ADVANCED, 25, "See help `ranged proficiency` for help." },
    /* Full spell/skill/prayer roster import: 6 named Shaman spells
     * (user 2026-07-26), ported onto Druid. Real level threshold from
     * spell_info.cc's START_1 (task_sacrifice.cc). */
    { "sacrifice",         CLASS_DRUID, SKILL_TIER_CLASS,  1, "See help `sacrifice` for help." },
    { "barkskin",          CLASS_DRUID, SKILL_TIER_CLASS,  3, "See help `barkskin` for help." },
    { "entangling roots",  CLASS_DRUID, SKILL_TIER_CLASS,  5, "See help `entangling roots` for help." },
    { "heal light",        CLASS_DRUID, SKILL_TIER_CLASS,  7, "See help `heal light` for help." },
    { "harm light",        CLASS_DRUID, SKILL_TIER_CLASS,  7, "See help `harm light` for help." },
    /* Roster gap noted during the meditate/penance pass (user 2026-07-27):
     * Mage has "meditate" and Cleric has "penance", both a level-1
     * meditative-recovery discipline -- Druid had no equivalent at all.
     * Same name as Mage's since it's dispatched through the shared
     * cast/Druid path (task_cast() matches by sk->name, see cmd_cast.c). */
    { "meditate",          CLASS_DRUID, SKILL_TIER_CLASS,  1, "See help `meditate` for help." },
    { "bramble drain",     CLASS_DRUID, SKILL_TIER_CLASS,  3, "See help `bramble drain` for help." },
    { "beast soother",     CLASS_DRUID, SKILL_TIER_CLASS,  5, "See help `beast soother` for help." },
    { "clot",              CLASS_DRUID, SKILL_TIER_CLASS,  5, "See help `clot` for help." },
    { "create water",      CLASS_DRUID, SKILL_TIER_CLASS, 9, "See help `create water` for help." },
    { "create food",       CLASS_DRUID, SKILL_TIER_CLASS, 9, "See help `create food` for help." },
    { "cure poison",       CLASS_DRUID, SKILL_TIER_CLASS, 9, "See help `cure poison` for help." },
    { "thorn barrage",     CLASS_DRUID, SKILL_TIER_CLASS, 10, "See help `thorn barrage` for help." },
    { "salve",             CLASS_DRUID, SKILL_TIER_CLASS, 12, "See help `salve` for help." },
    { "heal serious",      CLASS_DRUID, SKILL_TIER_CLASS, 14, "See help `heal serious` for help." },
    { "sunscald",          CLASS_DRUID, SKILL_TIER_CLASS, 16, "See help `sunscald` for help." },
    /* Full spell/skill/prayer roster import: 6 named Shaman spells
     * (user 2026-07-26), ported onto Druid. Real level threshold from
     * spell_info.cc's START_15 (disc_shaman.cc's stupidity()). */
    { "stupidity",         CLASS_DRUID, SKILL_TIER_CLASS, 15, "See help `stupidity` for help." },
    { "storm call",        CLASS_DRUID, SKILL_TIER_ADVANCED, 28, "See help `storm call` for help." },
    { "cure disease",      CLASS_DRUID, SKILL_TIER_CLASS, 23, "See help `cure disease` for help." },
    { "refresh",           CLASS_DRUID, SKILL_TIER_ADVANCED, 25, "See help `refresh` for help." },
    { "heal critical",     CLASS_DRUID, SKILL_TIER_ADVANCED, 25, "See help `heal critical` for help." },
    { "feral wrath",       CLASS_DRUID, SKILL_TIER_ADVANCED, 28, "See help `feral wrath` for help." },
    { "earthmaw",          CLASS_DRUID, SKILL_TIER_ADVANCED, 28, "See help `earthmaw` for help." },
    { "sky spirit",        CLASS_DRUID, SKILL_TIER_ADVANCED, 28, "See help `sky spirit` for help." },
    /* Pet/charm (Sneezy → Tobin feature audit) -- new, closest real
     * upstream analog is Ranger's own DISC_ANIMAL "beast" mechanics (no
     * Ranger class in Tobin; Druid absorbed its nature-magic flavor).
     * Reuses the real seeded "wolf fierce gray" mob (vnum 570). */
    { "animal companion",  CLASS_DRUID, SKILL_TIER_ADVANCED, 24, "See help `animal companion` for help." },
    { "cure blindness",    CLASS_DRUID, SKILL_TIER_ADVANCED, 30, "See help `cure blindness` for help." },
    { "wave crash",        CLASS_DRUID, SKILL_TIER_ADVANCED, 32, "See help `wave crash` for help." },
    { "withering touch",   CLASS_DRUID, SKILL_TIER_ADVANCED, 32, "See help `withering touch` for help." },
    { "wild agony",        CLASS_DRUID, SKILL_TIER_ADVANCED, 38, "See help `wild agony` for help." },
    { "heal",              CLASS_DRUID, SKILL_TIER_ADVANCED, 40, "See help `heal` for help." },
    { "tree walk",         CLASS_DRUID, SKILL_TIER_ADVANCED, 41, "See help `tree walk` for help." },
    { "nature's wrath",    CLASS_DRUID, SKILL_TIER_ADVANCED, 46, "See help `nature's wrath` for help." },
    { "leeching vine",     CLASS_DRUID, SKILL_TIER_ADVANCED, 48, "See help `leeching vine` for help." },
    { "wildfire",          CLASS_DRUID, SKILL_TIER_ADVANCED, 48, "See help `wildfire` for help." },
    { "word of recall",    CLASS_DRUID, SKILL_TIER_ADVANCED, 50, "See help `word of recall` for help." },
    /* Crafting & extraction (Sneezy -> Tobin feature audit) -- Druid,
     * Tobin's established Ranger-flavor analog (same mapping the roster-
     * import section above uses for Ranger's own real spell list). */
    { "butcher",           CLASS_DRUID, SKILL_TIER_CLASS,     1, "See help `butcher` for help." },
    { "skin",              CLASS_DRUID, SKILL_TIER_ADVANCED, 25, "See help `skin` for help." },
    { "forage",            CLASS_DRUID, SKILL_TIER_ADVANCED, 25, "See help `forage` for help." },
    /* Missing-skill audit (TODO.md, "Generic / cross-class"), 2026-08-05:
     * real upstream `toughness`/`focused avoidance`/`evaluate` are all
     * SKILL_GENERAL in spell_info.cc's discArray (DISC_DEFENSE/
     * DISC_ADVENTURING) -- available to every class, not tied to one --
     * so each gets one roster row per class, same "duplicated per class"
     * shape `repair`'s Cleric/Monk/Thief rows above already established
     * for a cross-class skill. `toughness`: real upstream is a per-hit
     * chance to gain a stacking damage-immunity buff (combat.cc's
     * doToughness()); ported as a flat passive damage-reduction
     * percentage scaling with proficiency instead (combat.c), same
     * "flat passive instead of true stacking" scope-cut `bloodlust`
     * above already used. `focused avoidance`: real upstream
     * (disc_advanced_defense.cc's canFocusedAvoidance()) is a passive
     * dodge check scaled by agility; ported as a flat to-hit-modifier
     * reduction against the defender, same shape `oomlat`'s AC bonus
     * already uses just below in combat.c. `evaluate`: real upstream
     * (cmd_compare.cc) gates how much detail the `compare` command
     * reveals about two items; Tobin has no `compare` command, so this
     * ports as its own new `evaluate <item>` command (cmd_evaluate.c)
     * instead, tiered by proficiency: a rough price guess at low skill,
     * condition and material tier revealed at higher skill. */
    { "toughness",          CLASS_MAGE,    SKILL_TIER_ADVANCED, 25, "See help `toughness` for help." },
    { "toughness",          CLASS_CLERIC,  SKILL_TIER_ADVANCED, 25, "See help `toughness` for help." },
    { "toughness",          CLASS_WARRIOR, SKILL_TIER_ADVANCED, 25, "See help `toughness` for help." },
    { "toughness",          CLASS_THIEF,   SKILL_TIER_ADVANCED, 25, "See help `toughness` for help." },
    { "toughness",          CLASS_DRUID,   SKILL_TIER_ADVANCED, 25, "See help `toughness` for help." },
    { "toughness",          CLASS_MONK,    SKILL_TIER_ADVANCED, 25, "See help `toughness` for help." },
    { "focused avoidance",  CLASS_MAGE,    SKILL_TIER_ADVANCED, 30, "See help `focused avoidance` for help." },
    { "focused avoidance",  CLASS_CLERIC,  SKILL_TIER_ADVANCED, 30, "See help `focused avoidance` for help." },
    { "focused avoidance",  CLASS_WARRIOR, SKILL_TIER_ADVANCED, 30, "See help `focused avoidance` for help." },
    { "focused avoidance",  CLASS_THIEF,   SKILL_TIER_ADVANCED, 30, "See help `focused avoidance` for help." },
    { "focused avoidance",  CLASS_DRUID,   SKILL_TIER_ADVANCED, 30, "See help `focused avoidance` for help." },
    { "focused avoidance",  CLASS_MONK,    SKILL_TIER_ADVANCED, 30, "See help `focused avoidance` for help." },
    { "evaluate",           CLASS_MAGE,    SKILL_TIER_CLASS,     1, "See help `evaluate` for help." },
    { "evaluate",           CLASS_CLERIC,  SKILL_TIER_CLASS,     1, "See help `evaluate` for help." },
    { "evaluate",           CLASS_WARRIOR, SKILL_TIER_CLASS,     1, "See help `evaluate` for help." },
    { "evaluate",           CLASS_THIEF,   SKILL_TIER_CLASS,     1, "See help `evaluate` for help." },
    { "evaluate",           CLASS_DRUID,   SKILL_TIER_CLASS,     1, "See help `evaluate` for help." },
    { "evaluate",           CLASS_MONK,    SKILL_TIER_CLASS,     1, "See help `evaluate` for help." },
};

#define SKILL_TOTAL (int)(sizeof(SKILLS) / sizeof(SKILLS[0]))

/* Reports how many skills/spells exist in total, across every class --
 * used to loop over the whole roster (skill_at()) without hardcoding
 * a count anywhere else. */
int skill_count(void) {
    return SKILL_TOTAL;
}

/* Fetches one entry from the skill/spell roster by its position
 * (0-based). Returns NULL if `index` is out of range instead of
 * crashing, so callers can safely loop `for (i = 0; i < skill_count(); i++)`. */
const skill_def_t *skill_at(int index) {
    if (index < 0 || index >= SKILL_TOTAL)
        return NULL;
    return &SKILLS[index];
}

/* Whether `b` currently knows the skill/spell named `name` (exact
 * match, case-insensitive) -- level and, for Class/Advanced tiers, the
 * discipline-percentage gate (progress_t.basic_disc_pct/advanced_disc_pct,
 * user 2026-07-12's practice/guildmaster request) must both pass, same
 * rules cmd_cast.c/cmd_pray.c already enforce. Immortals always know
 * everything, regardless of class (user 2026-07-12: "immortals can use
 * any skill or spell in game, no class restrictions") -- first name
 * match across the whole roster wins. Used by combat.c for "dual
 * wield" (user 2026-07-12: weapon depth). */
bool being_knows_skill(const being_t *b, const char *name) {
    if (!b)
        return false;
    bool imm = being_is_immortal(b);
    /* Monk kick mastery (user, 2026-08-04: "once kick is maxed then
     * advanced kick should take over as an automatic attack") -- a Monk
     * whose own "kick" proficiency has reached 100% is treated as
     * knowing "advanced kicking" even before level 25 / spending
     * practice points on it the normal way, so mastering the base skill
     * naturally unlocks its automatic upgrade (combat_process_run()'s
     * own bonus-strike check already treats "advanced kicking" as one
     * of the triggers for a chance at a bonus strike each round,
     * combat.c). */
    if (!imm && b->char_class == CLASS_MONK && strcasecmp(name, "advanced kicking") == 0) {
        const skill_def_t *kick_sk = skill_find(CLASS_MONK, "kick", false);
        if (kick_sk && skill_proficiency(b, kick_sk) >= 100)
            return true;
    }
    /* Weapon specializations are auto-known (user, 2026-08-04: "all of
     * those should be automatic") -- skip straight past the level check
     * and the normal combat_disc_pct guildmaster gate every other
     * Combat-tier skill needs. Still class-restricted (Warrior only,
     * matching real upstream) and still off for a level below 1 (never
     * true in practice, kept for consistency with every other gate
     * here). */
    if (!imm && b->char_class == CLASS_WARRIOR && b->progress.level >= 1
        && (strcasecmp(name, "slash specialization") == 0
            || strcasecmp(name, "blunt specialization") == 0
            || strcasecmp(name, "pierce specialization") == 0
            || strcasecmp(name, "ranged specialization") == 0
            || strcasecmp(name, "barehand specialization") == 0)) {
        return true;
    }
    int count = skill_count();
    for (int i = 0; i < count; i++) {
        const skill_def_t *sk = skill_at(i);
        if (!imm && sk->cls != b->char_class)
            continue;
        if (strcasecmp(sk->name, name) != 0)
            continue;
        if (imm)
            return true;
        if (b->progress.level < sk->min_level)
            return false;
        if (sk->tier == SKILL_TIER_CLASS && b->progress.basic_disc_pct <= 0)
            return false;
        if (sk->tier == SKILL_TIER_COMBAT && b->progress.combat_disc_pct <= 0)
            return false;
        if (sk->tier == SKILL_TIER_ADVANCED &&
            (b->progress.basic_disc_pct < 100 || b->progress.combat_disc_pct < 100
             || b->progress.advanced_disc_pct <= 0))
            return false;
        return true;
    }
    return false;
}

/* Builds the on-screen heading for one tier of one class's skill list
 * (e.g. "Advanced Warrior Skills") -- the `skills` command uses this so
 * every class's roster is titled consistently instead of each caller
 * writing its own heading text. */
const char *skill_tier_label(player_class_t cls, skill_tier_t tier, char *buf, size_t bufsz) {
    const char *cname = class_name(cls);
    switch (tier) {
        case SKILL_TIER_COMBAT:
            snprintf(buf, bufsz, "Combat");
            break;
        case SKILL_TIER_ADVANCED:
            snprintf(buf, bufsz, "Advanced %s Skills", cname);
            break;
        case SKILL_TIER_CLASS:
        default:
            snprintf(buf, bufsz, "%s Skills", cname);
            break;
    }
    return buf;
}

/* Looks up a skill definition by name, scoped to `cls` unless `any_class`
 * is set (e.g. for immortal/admin lookups that shouldn't be class-gated).
 * Returns NULL if no skill with that name is found in scope. */
const skill_def_t *skill_find(player_class_t cls, const char *name, bool any_class) {
    int count = skill_count();
    for (int i = 0; i < count; i++) {
        const skill_def_t *sk = skill_at(i);
        if (!any_class && sk->cls != cls)
            continue;
        if (strcasecmp(sk->name, name) == 0)
            return sk;
    }
    return NULL;
}

/* The discipline percentage that acts as this skill's proficiency
 * ceiling -- rising it (via `practice`) is what lets learn-by-doing keep
 * climbing, same relationship as Sneezy's getMaxSkillValue(). */
static int skill_ceiling(const being_t *ch, const skill_def_t *sk) {
    if (sk->tier == SKILL_TIER_COMBAT)   return ch->progress.combat_disc_pct;
    if (sk->tier == SKILL_TIER_ADVANCED) return ch->progress.advanced_disc_pct;
    /* Weapon specializations (user, 2026-08-04: "all of those should be
     * automatic") -- Class-tier otherwise caps at basic_disc_pct, which
     * sits at 0 for a never-practiced character and would leave these
     * permanently stuck at their 1% floor despite landing real hits.
     * Uncapped ceiling instead, same "no guildmaster gate at all" spirit
     * as being_knows_skill()'s own bypass for these 5 names. */
    if (sk->cls == CLASS_WARRIOR
        && (strcasecmp(sk->name, "slash specialization") == 0
            || strcasecmp(sk->name, "blunt specialization") == 0
            || strcasecmp(sk->name, "pierce specialization") == 0
            || strcasecmp(sk->name, "ranged specialization") == 0
            || strcasecmp(sk->name, "barehand specialization") == 0))
        return 100;
    return ch->progress.basic_disc_pct;
}

/* First-ever-attempted proficiency floor, matching Sneezy's own minimum
 * initial value (never literal 0 -- you're barely competent, not
 * hopeless, the first time you try something you already have access
 * to). */
#define SKILL_PROFICIENCY_FLOOR 1

/* Anti-grind: a skill won't gain-check again this soon after its last
 * gain-check, win or lose. Sneezy scales this 30s/3min by current skill
 * level; one flat cooldown is a deliberate simplification. */
#define SKILL_GAIN_COOLDOWN_SECS 30

/* A character's current proficiency percent in `sk`, or 0 if they've never
 * attempted it (no skill_repo row yet).
 *
 * SKILL_TIER_COMBAT (the 5 weapon/barehand proficiency skills) is a
 * special case (user 2026-08-03: "the proficiency skills in combat
 * disciplines should be gained at 1% of the disc ... for all classes
 * and all proficiencies ... they should gain in proficiency
 * automatically, when combat hits 100% they should be able to increase
 * proficiency to 100%, but they should start from level 1"). These 5
 * skills have no gameplay hook anywhere that calls
 * skill_learn_from_doing() on them (no weapon-type distinction exists
 * in obj.h to gate a per-swing roll on -- see cmd_stabbing.c's own
 * "flavor-text placeholders" note), so under the normal learn-by-doing
 * system they would sit permanently stuck at their 1% floor forever,
 * un-grindable. Auto-tracks combat_disc_pct directly instead: 0 until
 * any Combat discipline is trained, then floored at 1 ("start from
 * level 1") and rising automatically in lockstep with
 * combat_disc_pct up to its full 100% -- no separate skill_repo
 * storage or per-use roll needed for these 5. */
int skill_proficiency(const being_t *ch, const skill_def_t *sk) {
    /* User 2026-08-03: "immortals should have all skills/spells at maxed
     * potential without spending practice points" -- a read-time
     * override, not a DB write, so no practice_points are spent and
     * player_skill / *_disc_pct storage stays untouched (also covers a
     * demoted-then-repromoted immortal with zero extra bookkeeping). */
    if (being_is_immortal(ch))
        return 100;
    if (sk->tier == SKILL_TIER_COMBAT) {
        if (ch->progress.combat_disc_pct <= 0)
            return 0;
        int pct = ch->progress.combat_disc_pct;
        return pct < SKILL_PROFICIENCY_FLOOR ? SKILL_PROFICIENCY_FLOOR : pct;
    }
    skill_proficiency_t sp;
    if (!skill_repo_get(ch->player_id, sk->name, &sp))
        return 0;
    return sp.pct;
}

/* See skill.h's doc comment. */
const char *skill_proficiency_word_colored(int pct) {
    if (pct >= 100) return "<G>mastered<z>";
    if (pct >= 90)  return "<g>expert<z>";
    if (pct >= 75)  return "<c>skilled<z>";
    if (pct >= 50)  return "<y>adept<z>";
    if (pct >= 25)  return "<o>competent<z>";
    if (pct >= 1)   return "<r>novice<z>";
    return "<k>untrained<z>";
}

/* Learn-by-doing gain check: called after `ch` uses skill `sk`, this may
 * raise their stored proficiency toward its discipline-percent ceiling
 * (skill_ceiling()), gated by a cooldown and a headroom-shrinking chance
 * curve softened by Wisdom. Returns the resulting (possibly unchanged)
 * proficiency. First-ever use sets the floor with no roll.
 *
 * SKILL_TIER_COMBAT is never actually called through here (see
 * skill_proficiency()'s own doc comment -- nothing in the codebase has
 * a hook to call this for those 5 skills), but routes straight to
 * skill_proficiency() if it ever is, rather than writing a now-unused
 * skill_repo row that skill_proficiency() would just ignore anyway. */
int skill_learn_from_doing(being_t *ch, const skill_def_t *sk) {
    if (sk->tier == SKILL_TIER_COMBAT)
        return skill_proficiency(ch, sk);

    int ceiling = skill_ceiling(ch, sk);
    if (ceiling <= 0)
        return 0; /* shouldn't happen -- the caller's discipline gate already blocks this */

    skill_proficiency_t sp;
    long now = (long)time(NULL);

    if (!skill_repo_get(ch->player_id, sk->name, &sp)) {
        /* First-ever attempt: establish the floor, no roll needed. */
        sp.pct = SKILL_PROFICIENCY_FLOOR;
        if (sp.pct > ceiling)
            sp.pct = ceiling;
        sp.last_gain_at = now;
        skill_repo_set(ch->player_id, sk->name, sp.pct, sp.last_gain_at);
        return sp.pct;
    }

    if (sp.pct >= ceiling)
        return sp.pct; /* capped -- practice the discipline further to raise the ceiling */

    if (now - sp.last_gain_at < SKILL_GAIN_COOLDOWN_SECS)
        return sp.pct; /* too soon since the last gain-check */

    /* Headroom shrinks the gain chance as proficiency nears its ceiling
     * (Sneezy: chance = 1000 * headroom^power). Wisdom softens the
     * curve by lowering the exponent -- a high-Wisdom character keeps a
     * good gain chance further into their climb. Integer exponent
     * (instead of Sneezy's continuous 1.0-3.0 float via pow()) avoids
     * needing libm, matching practice.c's own no-math.h precedent. */
    int power = 2;
    if (ch->attrs.wisdom >= ATTR_BASE + 10)
        power = 1;
    else if (ch->attrs.wisdom <= ATTR_BASE - 10)
        power = 3;

    double headroom = (double)(ceiling - sp.pct) / (double)ceiling;
    double chance_ratio = headroom;
    for (int i = 1; i < power; i++)
        chance_ratio *= headroom;
    int chance = (int)(1000.0 * chance_ratio);
    if (chance < 15)
        chance = 15;

    if (rand() % 1000 < chance) {
        int before = sp.pct;
        sp.pct++;
        if (sp.pct > ceiling)
            sp.pct = ceiling;
        /* Player-visible feedback for every real proficiency gain (user
         * 2026-07-27) -- `before` guards the already-at-ceiling edge case
         * (sp.pct clamped back down to `before`, no actual gain to
         * announce). Silent for a mob (ch->desc is NULL for one). */
        if (sp.pct > before && ch->desc) {
            char msg[128];
            snprintf(msg, sizeof(msg), "<c>You have become better at %s! (%d%%)<z>\r\n",
                     sk->name, sp.pct);
            descriptor_send(ch->desc, msg);
        }
    }
    sp.last_gain_at = now;
    skill_repo_set(ch->player_id, sk->name, sp.pct, sp.last_gain_at);
    return sp.pct;
}

/* Flat percent-chance success roll for a skill use, given its proficiency
 * `pct` (0 always fails, 100+ always succeeds). Shared by any command that
 * needs a plain "does this skill attempt succeed" check. */
bool skill_roll_success(int pct) {
    if (pct >= 100)
        return true;
    if (pct <= 0)
        return false;
    return (rand() % 100) < pct;
}
