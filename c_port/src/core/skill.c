/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "skill.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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
    { "bash",                    CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "Knock your target down, stunning them briefly." },
    { "berserk",                 CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "Forgo defense for a burst of offense -- but you're much harder to rescue or parry while raging." },
    { "rally",                   CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "A battlecry that boosts nearby allies' combat prowess." },
    { "retreat",                 CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "Voluntarily disengage from your current opponent." },
    { "parry",                   CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "A passive chance to block an incoming melee attack outright." },
    { "grapple",                 CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "Grab and hold an opponent, restricting what they can do." },
    { "trip",                    CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "Knock an opponent to the ground." },
    { "doorbash",                CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "Force your way through a closed door." },
    { "dual wield",              CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "Passively reduces the damage penalty for your off-hand weapon." },
    { "power move",              CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "Improves your odds of landing a solid, hard-hitting strike." },
    { "two-handed specialization", CLASS_WARRIOR, SKILL_TIER_COMBAT, 1, "Bonus damage while wielding a two-handed weapon." },
    { "fortify",                 CLASS_WARRIOR, SKILL_TIER_COMBAT,   1, "A defensive shield-wall stance -- requires a shield." },
    { "rescue",                  CLASS_WARRIOR, SKILL_TIER_CLASS,   1, "Swap places with an ally in combat, pulling their attacker onto yourself." },
    { "focus attack",            CLASS_WARRIOR, SKILL_TIER_CLASS,   5, "A single concentrated strike for extra damage." },
    { "shove",                   CLASS_WARRIOR, SKILL_TIER_CLASS,   6, "Push an opponent, knocking them off balance." },
    { "bodyslam",                CLASS_WARRIOR, SKILL_TIER_CLASS,   10, "A grappling throw that slams your opponent down for damage." },
    { "headbutt",                CLASS_WARRIOR, SKILL_TIER_CLASS,   15, "A headfirst strike -- needs a target roughly your own height." },
    { "spin",                    CLASS_WARRIOR, SKILL_TIER_CLASS,   17, "A spinning grapple-style strike -- needs a free hand." },
    { "disarm",                  CLASS_WARRIOR, SKILL_TIER_CLASS,   17, "Knock the weapon out of an opponent's hand." },
    { "advanced berserking",     CLASS_WARRIOR, SKILL_TIER_CLASS,   20, "An upgraded berserk with a stronger effect." },
    { "slam",                    CLASS_WARRIOR, SKILL_TIER_CLASS,   20, "A heavy shield-and-body slam for extra damage and a stun." },
    { "riposte",                 CLASS_WARRIOR, SKILL_TIER_CLASS,   20, "A successful parry gives you a chance to counter-attack immediately." },
    { "deathstroke",             CLASS_WARRIOR, SKILL_TIER_CLASS,   20, "A heavy, finishing-style attack against a single target." },
    { "taunt",                   CLASS_WARRIOR, SKILL_TIER_CLASS,   22, "Provoke a target into focusing their aggression on you." },
    { "whirlwind",               CLASS_WARRIOR, SKILL_TIER_CLASS,   25, "A spinning attack that can strike every opponent in the room." },
    { "kneestrike",              CLASS_WARRIOR, SKILL_TIER_CLASS,   25, "A knee strike -- unusable while crawling." },
    { "switch opponents",        CLASS_WARRIOR, SKILL_TIER_CLASS,   25, "Change which opponent you're actively fighting." },
    { "trance of blades",        CLASS_WARRIOR, SKILL_TIER_ADVANCED, 25, "A defensive stance that sharpens your reflexes at the cost of offense." },
    { "weapon retention",        CLASS_WARRIOR, SKILL_TIER_ADVANCED, 25, "A passive chance to keep your grip on your weapon when disarmed or fumbling." },
    { "brawl avoidance",         CLASS_WARRIOR, SKILL_TIER_ADVANCED, 25, "Passive resistance to grapple- and trip-style attacks." },
    { "close quarters fighting", CLASS_WARRIOR, SKILL_TIER_ADVANCED, 50, "A combat bonus while grappled or fighting at point-blank range." },

    /* ---------------- THIEF ---------------- */
    { "kick",             CLASS_THIEF, SKILL_TIER_COMBAT,  1, "An unarmed kick attack." },
    { "retreat",          CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Disengage from your current opponent." },
    { "backstab",         CLASS_THIEF, SKILL_TIER_COMBAT,  1, "A devastating sneak attack against an unaware or from-behind target." },
    { "dodge",            CLASS_THIEF, SKILL_TIER_COMBAT,  1, "A passive chance to evade an incoming melee attack." },
    { "garrotte",         CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Strangle a victim from behind with a cord." },
    { "throatslit",       CLASS_THIEF, SKILL_TIER_COMBAT,  1, "A lethal sneak attack targeting the throat." },
    { "poison weapon",    CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Coat your weapon in poison so your hits inflict it." },
    { "steal",            CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Pick a victim's pocket." },
    { "search",           CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Find hidden objects or secret exits." },
    { "detect trap",      CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Spot a trap before it triggers." },
    { "counter steal",    CLASS_THIEF, SKILL_TIER_COMBAT,  1, "A passive chance to catch someone trying to steal from you." },
    { "sneak",            CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Move around without waking sleepers or drawing attention." },
    { "concealment",      CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Hide your movement trail from trackers." },
    { "disguise",         CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Alter your apparent identity." },
    { "skulk",            CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Move from room to room while remaining hidden." },
    { "set trap (arrow)", CLASS_THIEF, SKILL_TIER_COMBAT,  1, "Rig an arrow trap." },
    { "set trap (container)", CLASS_THIEF, SKILL_TIER_COMBAT, 1, "Rig a trap on a container." },
    { "track",            CLASS_THIEF, SKILL_TIER_CLASS,  13, "Follow a target's trail between rooms." },
    { "switch opponents", CLASS_THIEF, SKILL_TIER_CLASS,  1, "Change which opponent you're actively fighting." },
    { "set trap (door)",  CLASS_THIEF, SKILL_TIER_CLASS,  19, "Rig a trap on a door." },
    { "pick lock",        CLASS_THIEF, SKILL_TIER_CLASS,  21, "Pick a locked door or container." },
    { "stabbing",         CLASS_THIEF, SKILL_TIER_CLASS,  25, "A piercing-weapon melee attack." },
    { "disarm",           CLASS_THIEF, SKILL_TIER_CLASS,  25, "Knock the weapon out of an opponent's hand." },
    { "disarm trap",      CLASS_THIEF, SKILL_TIER_CLASS,  25, "Safely disable a trap you've detected." },
    { "subterfuge",       CLASS_THIEF, SKILL_TIER_ADVANCED, 25, "Deceive or redirect an opponent mid-combat." },
    { "hide",             CLASS_THIEF, SKILL_TIER_ADVANCED, 31, "Conceal yourself in the room you're standing in." },
    { "plant",            CLASS_THIEF, SKILL_TIER_ADVANCED, 31, "Surreptitiously plant an item on a victim." },
    { "dual wield",       CLASS_THIEF, SKILL_TIER_ADVANCED, 31, "Passively reduces the damage penalty for your off-hand weapon." },
    { "set trap (mine)",  CLASS_THIEF, SKILL_TIER_ADVANCED, 37, "Rig a mine-type trap." },
    { "spy",              CLASS_THIEF, SKILL_TIER_ADVANCED, 38, "Covertly watch a room from elsewhere." },
    { "cudgel",           CLASS_THIEF, SKILL_TIER_ADVANCED, 41, "A blunt, non-lethal knockout-style attack." },
    { "set trap (grenade)", CLASS_THIEF, SKILL_TIER_ADVANCED, 50, "Rig a grenade-type trap." },

    /* ---------------- MONK ---------------- */
    { "disarm",          CLASS_MONK, SKILL_TIER_COMBAT,  1, "Knock the weapon out of an opponent's hand." },
    { "kick",            CLASS_MONK, SKILL_TIER_COMBAT,  3, "An unarmed kick attack." },
    { "groundfighting",  CLASS_MONK, SKILL_TIER_COMBAT, 5, "Reduces the penalty for fighting while knocked down." },
    { "retreat",         CLASS_MONK, SKILL_TIER_COMBAT, 15, "Disengage from your current opponent." },
    { "counter move",    CLASS_MONK, SKILL_TIER_COMBAT, 25, "Resist being shoved or thrown out of position." },
    { "switch opponents", CLASS_MONK, SKILL_TIER_COMBAT, 25, "Change which opponent you're actively fighting." },
    { "chop",            CLASS_MONK, SKILL_TIER_COMBAT, 25, "An edge-of-hand strike." },
    { "yoginsa",         CLASS_MONK, SKILL_TIER_CLASS,   1, "Meditate to recover HP, movement, and mana." },
    { "jirin",           CLASS_MONK, SKILL_TIER_CLASS,   1, "Dodge, block, or deflect an incoming unarmed attack." },
    { "kubo",            CLASS_MONK, SKILL_TIER_CLASS,   1, "Your unarmed strikes scale with skill and level." },
    { "chi",             CLASS_MONK, SKILL_TIER_CLASS,   1, "A mana-based healing touch, on yourself, another, or an object." },
    { "oomlat",          CLASS_MONK, SKILL_TIER_CLASS,   1, "A passive armor bonus while fighting unarmed." },
    { "catfall",         CLASS_MONK, SKILL_TIER_CLASS,   1, "Halves damage from a fall." },
    { "catleap",         CLASS_MONK, SKILL_TIER_CLASS,   1, "Leap and glide a direction, out of combat." },
    { "cintai",          CLASS_MONK, SKILL_TIER_CLASS,   5, "A passive to-hit bonus while unarmed." },
    { "springleap",      CLASS_MONK, SKILL_TIER_CLASS,  20, "Spring instantly from sitting or resting to standing." },
    { "advanced kicking", CLASS_MONK, SKILL_TIER_CLASS, 25, "More of your unarmed strikes land as kicks, boosting extra-attack odds." },
    { "iron fist",           CLASS_MONK, SKILL_TIER_ADVANCED,  25, "Bonus strength-based damage while your hands are bare." },
    { "hurl",                CLASS_MONK, SKILL_TIER_ADVANCED,  25, "Throw a victim bodily out of the room." },
    { "chain attack",        CLASS_MONK, SKILL_TIER_ADVANCED,  25, "A chance at a bonus follow-up strike each round." },
    { "critical hitting",    CLASS_MONK, SKILL_TIER_ADVANCED,  25, "Improves your access to the harshest critical-hit outcomes." },
    { "wohlin meditation",   CLASS_MONK, SKILL_TIER_ADVANCED,  25, "While meditating, unlocks bonus self-cure effects." },
    { "voplat",              CLASS_MONK, SKILL_TIER_ADVANCED,  25, "Makes your unarmed damage magical." },
    { "blindfighting",       CLASS_MONK, SKILL_TIER_ADVANCED,  25, "Reduces the penalty for fighting while blinded." },
    { "feign death",         CLASS_MONK, SKILL_TIER_ADVANCED,  25, "Play dead to avoid detection or attack." },
    { "blur",                CLASS_MONK, SKILL_TIER_ADVANCED,  25, "A chance at an extra unarmed attack each round while empty-handed." },
    { "iron flesh",          CLASS_MONK, SKILL_TIER_ADVANCED, 31, "A passive armor bonus on unequipped body slots." },
    { "iron skin",           CLASS_MONK, SKILL_TIER_ADVANCED, 35, "Resistance to skin-condition damage effects." },
    { "shoulder throw",      CLASS_MONK, SKILL_TIER_ADVANCED, 36, "Throw a victim to the ground, damaging and stunning them." },
    { "iron bones",          CLASS_MONK, SKILL_TIER_ADVANCED, 38, "Resistance to bone-breaking damage." },
    { "snofalte",            CLASS_MONK, SKILL_TIER_ADVANCED, 38, "A mental discipline that can suppress your own bleeding." },
    { "iron muscles",        CLASS_MONK, SKILL_TIER_ADVANCED, 42, "A passive strength bonus." },
    { "defenestrate",        CLASS_MONK, SKILL_TIER_ADVANCED, 42, "Throw a victim through a window into another room." },
    { "quivering palm",      CLASS_MONK, SKILL_TIER_ADVANCED, 42, "A delayed death-touch strike." },
    { "iron legs",           CLASS_MONK, SKILL_TIER_ADVANCED, 45, "Increases your maximum movement points." },
    { "iron will",           CLASS_MONK, SKILL_TIER_ADVANCED, 48, "Resistance to non-magical damage and effects." },
    { "dufali",              CLASS_MONK, SKILL_TIER_ADVANCED, 48, "Passive resistance to paralysis." },
    { "bonebreak",           CLASS_MONK, SKILL_TIER_ADVANCED, 50, "A high-damage grapple attack that breaks a limb." },

    /* ---------------- CLERIC ---------------- */
    { "slash proficiency",    CLASS_CLERIC, SKILL_TIER_COMBAT,  1, "Basic proficiency with slashing weapons." },
    { "blunt proficiency",    CLASS_CLERIC, SKILL_TIER_COMBAT,  1, "Basic proficiency with blunt weapons." },
    { "pierce proficiency",   CLASS_CLERIC, SKILL_TIER_COMBAT,  1, "Basic proficiency with piercing weapons." },
    { "barehand proficiency", CLASS_CLERIC, SKILL_TIER_COMBAT,  1, "Basic proficiency fighting unarmed." },
    { "ranged proficiency",   CLASS_CLERIC, SKILL_TIER_COMBAT, 25, "Basic proficiency with ranged weapons." },
    { "heal light",       CLASS_CLERIC, SKILL_TIER_CLASS,  1, "A minor healing touch." },
    { "harm light",       CLASS_CLERIC, SKILL_TIER_CLASS,  1, "A minor bolt of negative energy." },
    { "armor",            CLASS_CLERIC, SKILL_TIER_CLASS,  1, "A protective blessing that improves armor class." },
    { "bless",            CLASS_CLERIC, SKILL_TIER_CLASS,  1, "A blessing that improves hit and damage rolls." },
    { "attune",           CLASS_CLERIC, SKILL_TIER_CLASS,  1, "Bind a holy symbol to your faction." },
    { "devotion",         CLASS_CLERIC, SKILL_TIER_CLASS,  1, "Passive prayer-point regeneration." },
    { "penance",          CLASS_CLERIC, SKILL_TIER_CLASS,  1, "A background discipline governing how fast you gain divine favor." },
    { "clot",             CLASS_CLERIC, SKILL_TIER_CLASS,  2, "Stops a victim's bleeding." },
    { "create food",      CLASS_CLERIC, SKILL_TIER_CLASS, 3, "Conjures food from nothing." },
    { "create water",     CLASS_CLERIC, SKILL_TIER_CLASS, 3, "Fills a container with water." },
    { "cure poison",      CLASS_CLERIC, SKILL_TIER_CLASS, 3, "Removes poison from a victim." },
    { "salve",            CLASS_CLERIC, SKILL_TIER_CLASS, 4, "Treats a minor wound." },
    { "heal serious",     CLASS_CLERIC, SKILL_TIER_CLASS, 5, "A stronger healing touch." },
    { "rain brimstone",   CLASS_CLERIC, SKILL_TIER_CLASS, 5, "Calls down a bolt of divine fire." },
    { "remove curse",     CLASS_CLERIC, SKILL_TIER_CLASS, 7, "Strips a curse from a person or object." },
    { "cure disease",     CLASS_CLERIC, SKILL_TIER_CLASS, 8, "Removes disease from a victim." },
    { "refresh",          CLASS_CLERIC, SKILL_TIER_CLASS, 9, "Restores movement points." },
    { "heal critical",    CLASS_CLERIC, SKILL_TIER_CLASS, 10, "A powerful healing touch." },
    { "harm serious",     CLASS_CLERIC, SKILL_TIER_CLASS, 10, "A stronger bolt of negative energy." },
    { "cure blindness",   CLASS_CLERIC, SKILL_TIER_CLASS, 12, "Removes blindness from a victim." },
    { "flamestrike",      CLASS_CLERIC, SKILL_TIER_CLASS, 13, "A column of divine flame." },
    { "curse",            CLASS_CLERIC, SKILL_TIER_CLASS, 13, "Curses a target or object." },
    { "expel",            CLASS_CLERIC, SKILL_TIER_CLASS, 13, "Expels vermin or a possessing affliction." },
    { "harm critical",    CLASS_CLERIC, SKILL_TIER_CLASS, 14, "A powerful bolt of negative energy." },
    { "disease",          CLASS_CLERIC, SKILL_TIER_CLASS, 14, "Inflicts a wasting disease." },
    { "poison",           CLASS_CLERIC, SKILL_TIER_CLASS, 15, "Inflicts a poison." },
    { "numb",             CLASS_CLERIC, SKILL_TIER_CLASS, 15, "Numbs a limb, reducing what a victim can do with it." },
    { "infect",           CLASS_CLERIC, SKILL_TIER_CLASS, 16, "Infects a wound." },
    { "heal",             CLASS_CLERIC, SKILL_TIER_CLASS, 17, "A near-total healing touch." },
    { "summon",           CLASS_CLERIC, SKILL_TIER_CLASS, 19, "Teleports a target to you." },
    { "harm",             CLASS_CLERIC, SKILL_TIER_CLASS, 20, "The strongest bolt of negative energy." },
    { "plague of locusts", CLASS_CLERIC, SKILL_TIER_CLASS, 21, "A swarm that damages everyone in the room." },
    { "word of recall",   CLASS_CLERIC, SKILL_TIER_CLASS, 21, "Teleports yourself or an ally to the recall point." },
    { "blindness",        CLASS_CLERIC, SKILL_TIER_CLASS, 21, "Blinds a target." },
    { "paralyze limb",    CLASS_CLERIC, SKILL_TIER_CLASS, 22, "Paralyzes one of a target's limbs." },
    { "knit bone",        CLASS_CLERIC, SKILL_TIER_CLASS, 25, "Repairs a broken bone." },
    { "sanctuary",        CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "A strong aura that reduces incoming damage." },
    { "bleed",            CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "Opens a wound that bleeds over time." },
    { "restore limb",     CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "Restores a withered or severed limb." },
    { "heroes' feast",    CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "A feast that buffs everyone who partakes." },
    { "pillar of salt",   CLASS_CLERIC, SKILL_TIER_ADVANCED,  25, "A single-target divine damage spell." },
    { "second wind",      CLASS_CLERIC, SKILL_TIER_ADVANCED, 33, "Restores a victim's movement points." },
    { "paralyze",         CLASS_CLERIC, SKILL_TIER_ADVANCED, 33, "Fully paralyzes a target." },
    { "earthquake",       CLASS_CLERIC, SKILL_TIER_ADVANCED, 33, "A room-wide tremor that damages everyone in it." },
    { "heal full",        CLASS_CLERIC, SKILL_TIER_ADVANCED, 28, "The strongest single-target heal." },
    { "cure paralysis",   CLASS_CLERIC, SKILL_TIER_ADVANCED, 37, "Removes paralysis from a victim." },
    { "heal critical spray", CLASS_CLERIC, SKILL_TIER_ADVANCED, 39, "Heals everyone in the room." },
    { "astral walk",      CLASS_CLERIC, SKILL_TIER_ADVANCED, 35, "Travel the astral plane to another location." },
    { "bone breaker",     CLASS_CLERIC, SKILL_TIER_ADVANCED, 40, "Breaks one of a target's limbs." },
    { "call lightning",   CLASS_CLERIC, SKILL_TIER_ADVANCED, 40, "Calls down a bolt of lightning." },
    { "consecrate",       CLASS_CLERIC, SKILL_TIER_ADVANCED, 43, "Fills the room with a persistent holy effect." },
    { "heal spray",       CLASS_CLERIC, SKILL_TIER_ADVANCED, 43, "A strong area heal for everyone in the room." },
    { "wither limb",      CLASS_CLERIC, SKILL_TIER_ADVANCED, 48, "Withers one of a target's limbs." },
    { "spontaneous combust", CLASS_CLERIC, SKILL_TIER_ADVANCED, 48, "Sets a target ablaze from within." },
    { "relive",           CLASS_CLERIC, SKILL_TIER_ADVANCED, 49, "Resurrects a corpse -- requires a holy relic." },
    { "crusade",          CLASS_CLERIC, SKILL_TIER_ADVANCED, 49, "A group-wide blessing for everyone in the room." },
    { "portal",           CLASS_CLERIC, SKILL_TIER_ADVANCED, 49, "Opens a portal to a named location." },
    { "heal full spray",  CLASS_CLERIC, SKILL_TIER_ADVANCED, 50, "The strongest area heal, for everyone in the room." },

    /* ---------------- MAGE ---------------- */
    { "slash proficiency",    CLASS_MAGE, SKILL_TIER_COMBAT,  1, "Basic proficiency with slashing weapons." },
    { "blunt proficiency",    CLASS_MAGE, SKILL_TIER_COMBAT,  1, "Basic proficiency with blunt weapons." },
    { "pierce proficiency",   CLASS_MAGE, SKILL_TIER_COMBAT,  1, "Basic proficiency with piercing weapons." },
    { "barehand proficiency", CLASS_MAGE, SKILL_TIER_COMBAT,  1, "Basic proficiency fighting unarmed." },
    { "ranged proficiency",   CLASS_MAGE, SKILL_TIER_COMBAT, 25, "Basic proficiency with ranged weapons." },
    { "wizardry",         CLASS_MAGE, SKILL_TIER_CLASS,  1, "The core skill of casting itself." },
    { "mana",             CLASS_MAGE, SKILL_TIER_CLASS,  1, "Governs the size of your mana pool." },
    { "meditate",         CLASS_MAGE, SKILL_TIER_CLASS,  1, "Rest to recover mana faster." },
    { "gust",             CLASS_MAGE, SKILL_TIER_CLASS,  1, "A bolt of wind damage." },
    { "sling shot",       CLASS_MAGE, SKILL_TIER_CLASS,  1, "A bolt of earthen damage." },
    { "gusher",           CLASS_MAGE, SKILL_TIER_CLASS,  1, "A bolt of water damage." },
    { "sorcerer's globe", CLASS_MAGE, SKILL_TIER_CLASS,  1, "A magical shield that buffs the group's defense." },
    { "mage sight",       CLASS_MAGE, SKILL_TIER_CLASS,  1, "A bundle of detection senses -- infravision, true sight, and more." },
    { "flare",            CLASS_MAGE, SKILL_TIER_CLASS,  3, "Lights up the room." },
    { "hands of flame",   CLASS_MAGE, SKILL_TIER_CLASS, 4, "A fiery touch attack." },
    { "mystic darts",     CLASS_MAGE, SKILL_TIER_CLASS, 5, "A bolt of raw magical energy." },
    { "illuminate",       CLASS_MAGE, SKILL_TIER_CLASS,  2, "Lights up an object." },
    { "faerie fire",      CLASS_MAGE, SKILL_TIER_CLASS, 6, "Marks a target with a pink aura, easier to hit." },
    { "materialize",      CLASS_MAGE, SKILL_TIER_CLASS, 6, "Conjures a named item out of thin air, for a price." },
    { "pebble spray",     CLASS_MAGE, SKILL_TIER_CLASS, 6, "An area-effect burst of earthen damage." },
    { "arctic blast",     CLASS_MAGE, SKILL_TIER_CLASS, 6, "An area-effect burst of cold damage." },
    { "feathery descent", CLASS_MAGE, SKILL_TIER_CLASS, 7, "A group buff that softens falls." },
    { "gills of flesh",   CLASS_MAGE, SKILL_TIER_CLASS, 9, "A group buff that lets you breathe underwater." },
    { "accelerate",       CLASS_MAGE, SKILL_TIER_CLASS, 11, "A group buff that speeds everyone up." },
    { "levitate",         CLASS_MAGE, SKILL_TIER_CLASS, 11, "A buff that lets you float above the ground." },
    { "dust storm",       CLASS_MAGE, SKILL_TIER_CLASS, 11, "An area-effect burst of wind and grit damage." },
    { "stunning arrow",   CLASS_MAGE, SKILL_TIER_CLASS, 12, "A magical arrow that damages and stuns." },
    { "color spray",      CLASS_MAGE, SKILL_TIER_CLASS, 12, "An area-effect burst of damage that can blind." },
    { "granite fists",    CLASS_MAGE, SKILL_TIER_CLASS, 11, "A stone-infused unarmed strike." },
    { "slumber",          CLASS_MAGE, SKILL_TIER_CLASS, 13, "Puts a target to sleep." },
    { "identify",         CLASS_MAGE, SKILL_TIER_CLASS, 14, "Reveals what an item really is." },
    { "icy grip",         CLASS_MAGE, SKILL_TIER_CLASS, 14, "A bolt of cold damage." },
    { "sense life",       CLASS_MAGE, SKILL_TIER_CLASS, 14, "Detects living creatures nearby." },
    { "tornado",          CLASS_MAGE, SKILL_TIER_CLASS, 15, "An area-effect burst of wind damage." },
    { "fear",             CLASS_MAGE, SKILL_TIER_CLASS, 14, "Forces a target to flee in terror." },
    { "stealth",          CLASS_MAGE, SKILL_TIER_CLASS, 16, "A group buff for moving quietly." },
    { "telepathy",        CLASS_MAGE, SKILL_TIER_CLASS, 16, "Sends a message across any distance." },
    { "faerie fog",       CLASS_MAGE, SKILL_TIER_CLASS, 18, "Obscures the room in an illusory fog." },
    { "dispel invisible", CLASS_MAGE, SKILL_TIER_CLASS, 17, "Reveals an invisible target or object." },
    { "galvanize",        CLASS_MAGE, SKILL_TIER_CLASS, 19, "Charges an item with electricity." },
    { "powerstone",       CLASS_MAGE, SKILL_TIER_CLASS, 19, "Imbues an item as a mana battery." },
    { "invisibility",     CLASS_MAGE, SKILL_TIER_CLASS, 17, "Turns yourself or an object invisible." },
    { "flaming sword",    CLASS_MAGE, SKILL_TIER_CLASS, 19, "Imbues a weapon strike with fire damage." },
    { "sand blast",       CLASS_MAGE, SKILL_TIER_CLASS, 19, "An area-effect burst of earthen damage." },
    { "ice storm",        CLASS_MAGE, SKILL_TIER_CLASS, 19, "An area-effect burst of cold damage." },
    { "teleport",         CLASS_MAGE, SKILL_TIER_CLASS, 19, "Teleports you to a random or chosen location." },
    { "dispel magic",     CLASS_MAGE, SKILL_TIER_CLASS, 20, "Strips magical effects from a being or object." },
    { "conjure elemental air", CLASS_MAGE, SKILL_TIER_CLASS, 12, "Summons an air elemental ally." },
    { "ensorcer",         CLASS_MAGE, SKILL_TIER_CLASS, 15, "Charms or dominates a target." },
    { "conjure elemental fire", CLASS_MAGE, SKILL_TIER_CLASS, 19, "Summons a fire elemental ally." },
    { "fireball",         CLASS_MAGE, SKILL_TIER_CLASS, 14, "A powerful area-effect burst of fire damage." },
    { "conjure elemental earth", CLASS_MAGE, SKILL_TIER_CLASS, 21, "Summons an earth elemental ally." },
    { "falcon wings",     CLASS_MAGE, SKILL_TIER_CLASS, 18, "A group buff for flight-like movement." },
    { "eyes of Fertuman",  CLASS_MAGE, SKILL_TIER_CLASS, 22, "Scries a location by name." },
    { "enhance weapon",   CLASS_MAGE, SKILL_TIER_CLASS, 24, "Permanently enchants a weapon." },
    { "conjure elemental water", CLASS_MAGE, SKILL_TIER_CLASS, 24, "Summons a water elemental ally." },
    { "copy",             CLASS_MAGE, SKILL_TIER_CLASS, 23, "Duplicates an item." },
    { "farlook",          CLASS_MAGE, SKILL_TIER_CLASS, 25, "Scries a remote location." },
    { "acid blast",       CLASS_MAGE, SKILL_TIER_CLASS, 25, "An area-effect burst of acid damage." },
    { "haste",            CLASS_MAGE, SKILL_TIER_CLASS, 23, "A group buff for extra speed and actions." },
    { "mage repair",      CLASS_MAGE, SKILL_TIER_CLASS, 17, "Magically repairs a piece of equipment." },
    { "calm",             CLASS_MAGE, SKILL_TIER_CLASS, 19, "Pacifies a target, stopping violence." },
    { "detect magic",        CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Lets you see magical auras." },
    { "protection from air",  CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Resistance to wind damage." },
    { "protection from fire", CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Resistance to fire damage." },
    { "protection from water", CLASS_MAGE, SKILL_TIER_ADVANCED, 25, "Resistance to water damage." },
    { "protection from energy", CLASS_MAGE, SKILL_TIER_ADVANCED, 25, "Resistance to raw energy damage." },
    { "stone skin",           CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "A buff that reduces incoming damage." },
    { "flaming flesh",        CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Wreathes you in fire, damaging attackers." },
    { "trail seek",           CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Tracks a target's trail." },
    { "scribe",               CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Writes a spell onto a scroll." },
    { "charge stave",         CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Charges a magical stave." },
    { "animate",              CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Animates an object to fight for you." },
    { "bind",                 CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Webs a target in place." },
    { "detect invisibility",  CLASS_MAGE, SKILL_TIER_ADVANCED,  25, "Lets you see invisible things." },
    { "shatter",              CLASS_MAGE, SKILL_TIER_ADVANCED,  27, "A destructive shattering attack." },
    { "infravision",          CLASS_MAGE, SKILL_TIER_ADVANCED, 28, "Lets you see in the dark." },
    { "true sight",           CLASS_MAGE, SKILL_TIER_ADVANCED, 28, "Sees through illusions and disguises." },
    { "lightning bolt",       CLASS_MAGE, SKILL_TIER_ADVANCED, 31, "A bolt of lightning damage." },
    { "cloud of concealment", CLASS_MAGE, SKILL_TIER_ADVANCED, 31, "Fills the room with concealing mist." },
    { "watery grave",         CLASS_MAGE, SKILL_TIER_ADVANCED, 32, "A drowning attack against a single target." },
    { "spontaneous generation", CLASS_MAGE, SKILL_TIER_ADVANCED, 32, "Conjures raw material by name, for a price." },
    { "blast of fury",        CLASS_MAGE, SKILL_TIER_ADVANCED, 31, "A bolt of raw energy damage." },
    { "Garmul's tail",        CLASS_MAGE, SKILL_TIER_ADVANCED,  26, "Reduces a target's mobility in water." },
    { "plasma mirror",        CLASS_MAGE, SKILL_TIER_ADVANCED, 34, "A reflective shield buff." },
    { "immobilize",           CLASS_MAGE, SKILL_TIER_ADVANCED, 36, "Roots a target in place." },
    { "energy drain",         CLASS_MAGE, SKILL_TIER_ADVANCED, 36, "Drains a resource from a target while damaging them." },
    { "polymorph",            CLASS_MAGE, SKILL_TIER_ADVANCED, 36, "Shapechanges a target into something else." },
    { "meteor swarm",         CLASS_MAGE, SKILL_TIER_ADVANCED, 39, "A powerful single-target strike from above." },
    { "tsunami",              CLASS_MAGE, SKILL_TIER_ADVANCED, 39, "An area-effect wave of water damage." },
    { "blizzard",             CLASS_MAGE, SKILL_TIER_ADVANCED, 39, "A room-persistent area-effect cold storm." },
    { "fumble",               CLASS_MAGE, SKILL_TIER_ADVANCED, 42, "Causes a target to fumble and disarm themselves." },
    { "suffocate",            CLASS_MAGE, SKILL_TIER_ADVANCED, 42, "Deprives a target of air." },
    { "pierce resistance",    CLASS_MAGE, SKILL_TIER_ADVANCED, 47, "Passive resistance to piercing damage." },
    { "flight",               CLASS_MAGE, SKILL_TIER_ADVANCED, 34, "A group buff for true flight." },
    { "divination",           CLASS_MAGE, SKILL_TIER_ADVANCED, 45, "Reveals information about an object or being." },
    { "sand blast",           CLASS_MAGE, SKILL_TIER_ADVANCED, 42, "An area-effect burst of earthen damage." },
    { "lava stream",          CLASS_MAGE, SKILL_TIER_ADVANCED, 47, "An area-effect burst of molten rock damage." },
    { "lava lance",           CLASS_MAGE, SKILL_TIER_ADVANCED, 50, "A devastating single-target bolt of molten rock." },
    { "hellfire",             CLASS_MAGE, SKILL_TIER_ADVANCED, 47, "An area-effect burst of fire damage." },
    { "atomize",              CLASS_MAGE, SKILL_TIER_ADVANCED, 48, "An overwhelming single-target burst of energy." },
    { "silence",              CLASS_MAGE, SKILL_TIER_ADVANCED, 48, "Mutes a target, blocking their spellcasting." },
    { "ethereal gate",        CLASS_MAGE, SKILL_TIER_ADVANCED, 48, "Opens a portal to a named location." },
    { "chain lightning",      CLASS_MAGE, SKILL_TIER_ADVANCED, 48, "A bolt of lightning that arcs between targets." },
    { "knot",                 CLASS_MAGE, SKILL_TIER_ADVANCED, 50, "A powerful self-ward." },

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
    { "slash proficiency",    CLASS_DRUID, SKILL_TIER_COMBAT,  1, "Basic proficiency with slashing weapons." },
    { "blunt proficiency",    CLASS_DRUID, SKILL_TIER_COMBAT,  1, "Basic proficiency with blunt weapons." },
    { "pierce proficiency",   CLASS_DRUID, SKILL_TIER_COMBAT,  1, "Basic proficiency with piercing weapons." },
    { "barehand proficiency", CLASS_DRUID, SKILL_TIER_COMBAT,  1, "Basic proficiency fighting unarmed." },
    { "ranged proficiency",   CLASS_DRUID, SKILL_TIER_COMBAT, 25, "Basic proficiency with ranged weapons." },
    { "barkskin",          CLASS_DRUID, SKILL_TIER_CLASS,  1, "Your skin turns to bark, granting a strong armor bonus." },
    { "entangling roots",  CLASS_DRUID, SKILL_TIER_CLASS,  1, "Roots erupt underfoot, tripping and damaging a target -- only works outdoors." },
    { "heal light",        CLASS_DRUID, SKILL_TIER_CLASS,  1, "A minor healing touch." },
    { "harm light",        CLASS_DRUID, SKILL_TIER_CLASS,  1, "A minor bolt of natural energy." },
    { "bramble drain",     CLASS_DRUID, SKILL_TIER_CLASS,  3, "A thorned vine that drains a small amount of life to you." },
    { "beast soother",     CLASS_DRUID, SKILL_TIER_CLASS,  5, "Calms a hostile or hunting animal." },
    { "clot",              CLASS_DRUID, SKILL_TIER_CLASS,  5, "Stops a victim's bleeding." },
    { "create water",      CLASS_DRUID, SKILL_TIER_CLASS, 9, "Fills a container with water." },
    { "create food",       CLASS_DRUID, SKILL_TIER_CLASS, 9, "Conjures food from nothing." },
    { "cure poison",       CLASS_DRUID, SKILL_TIER_CLASS, 9, "Removes poison from a victim." },
    { "thorn barrage",     CLASS_DRUID, SKILL_TIER_CLASS, 10, "Several beams of thorny energy lash the target." },
    { "salve",             CLASS_DRUID, SKILL_TIER_CLASS, 12, "Treats a minor wound." },
    { "heal serious",      CLASS_DRUID, SKILL_TIER_CLASS, 14, "A stronger healing touch." },
    { "sunscald",          CLASS_DRUID, SKILL_TIER_CLASS, 16, "Sears a target with focused sunlight." },
    { "storm call",        CLASS_DRUID, SKILL_TIER_CLASS, 23, "Calls down lightning or hail -- only works in the right weather." },
    { "cure disease",      CLASS_DRUID, SKILL_TIER_CLASS, 23, "Removes disease from a victim." },
    { "refresh",           CLASS_DRUID, SKILL_TIER_CLASS, 25, "Restores movement points." },
    { "heal critical",     CLASS_DRUID, SKILL_TIER_ADVANCED, 25, "A powerful healing touch." },
    { "feral wrath",       CLASS_DRUID, SKILL_TIER_ADVANCED, 28, "Channel a spirit animal for a temporary stat boost." },
    { "earthmaw",          CLASS_DRUID, SKILL_TIER_ADVANCED, 28, "The ground splits beneath a target, damaging and knocking them down -- outdoors only." },
    { "sky spirit",        CLASS_DRUID, SKILL_TIER_ADVANCED, 28, "Summons a phantasmal bird spirit to strike a target -- scales with your skill." },
    { "cure blindness",    CLASS_DRUID, SKILL_TIER_ADVANCED, 30, "Removes blindness from a victim." },
    { "wave crash",        CLASS_DRUID, SKILL_TIER_ADVANCED, 32, "A crashing wave slams into a target, possibly knocking them down." },
    { "withering touch",   CLASS_DRUID, SKILL_TIER_ADVANCED, 32, "A draining touch that saps a victim's vitality into you." },
    { "wild agony",        CLASS_DRUID, SKILL_TIER_ADVANCED, 38, "Inflicts a burst of raw, unnatural pain." },
    { "heal",              CLASS_DRUID, SKILL_TIER_ADVANCED, 40, "A near-total healing touch." },
    { "tree walk",         CLASS_DRUID, SKILL_TIER_ADVANCED, 41, "Teleports you (and your group) to a tree you're bonded to." },
    { "nature's wrath",    CLASS_DRUID, SKILL_TIER_ADVANCED, 46, "Beams of raw natural fury tear into a target." },
    { "leeching vine",     CLASS_DRUID, SKILL_TIER_ADVANCED, 48, "A grasping vine drains a target's life into you." },
    { "wildfire",          CLASS_DRUID, SKILL_TIER_ADVANCED, 48, "An intense, heavy burst of flame." },
    { "word of recall",    CLASS_DRUID, SKILL_TIER_ADVANCED, 50, "Teleports yourself or an ally to the recall point." },
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

int skill_proficiency(const being_t *ch, const skill_def_t *sk) {
    skill_proficiency_t sp;
    if (!skill_repo_get(ch->player_id, sk->name, &sp))
        return 0;
    return sp.pct;
}

int skill_learn_from_doing(being_t *ch, const skill_def_t *sk) {
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
        sp.pct++;
        if (sp.pct > ceiling)
            sp.pct = ceiling;
    }
    sp.last_gain_at = now;
    skill_repo_set(ch->player_id, sk->name, sp.pct, sp.last_gain_at);
    return sp.pct;
}

bool skill_roll_success(int pct) {
    if (pct >= 100)
        return true;
    if (pct <= 0)
        return false;
    return (rand() % 100) < pct;
}
