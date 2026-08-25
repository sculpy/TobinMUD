/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "being.h"
#include "combat.h"
#include "skill.h"
#include "thing.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Monster-lore classification -- the data behind the Know-X lore skills
 * (know animal / demon / giantkin / other / people / reptile / undead /
 * veggie), the `know` command (cmd_know.c), and Sneezy's consider-family
 * SKILL_CONS_* creature identification.
 *
 * Every MOB_RACE_NAMES[] index maps to exactly one of eight "kingdoms",
 * ported verbatim from real SneezyMUD's per-race `lore` keyword (the
 * `lore animal|veggie|diabolic|reptile|undead|giant|people|other` line in
 * each lib/races/RACE_* file -> Race::Kingdom, misc/race.cc; TBeing::
 * isAnimal()/isReptile()/... in misc/oldrace.cc just test that field).
 * "diabolic" is surfaced to players as the "demon" lore skill. The switch
 * is exhaustive over the 127 races in being.c's MOB_RACE_NAMES[]; anything
 * out of range falls through to LORE_OTHER. */

mob_lore_t mob_race_lore_category(int idx) {
    switch (idx) {
        /* animal -- mundane and fantastical beasts (Sneezy lore animal) */
        case 7:   /* PEGASUS */
        case 12:  /* INSECT */
        case 13:  /* ARACHNID */
        case 15:  /* FISH */
        case 16:  /* BIRD */
        case 23:  /* HIPPOPOTAMUS */
        case 27:  /* ANT */
        case 34:  /* PRIMATE */
        case 41:  /* RODENT */
        case 45:  /* FELINE */
        case 46:  /* CANINE */
        case 47:  /* HORSE */
        case 53:  /* OCTOPUS */
        case 54:  /* CRUSTACEAN */
        case 56:  /* BOVINE */
        case 57:  /* GOAT */
        case 58:  /* SHEEP */
        case 59:  /* DEER */
        case 60:  /* BEAR */
        case 61:  /* WEASEL */
        case 62:  /* SQUIRREL */
        case 63:  /* RABBIT */
        case 64:  /* BADGER */
        case 65:  /* OTTER */
        case 66:  /* BEAVER */
        case 67:  /* PIG */
        case 68:  /* BOAR */
        case 69:  /* TURTLE */
        case 70:  /* GIRAFFE */
        case 71:  /* CENTIPEDE */
        case 85:  /* LION */
        case 86:  /* TIGER */
        case 87:  /* LEOPARD */
        case 88:  /* COUGAR */
        case 89:  /* FROG */
        case 90:  /* ELEPHANT */
        case 91:  /* RHINO */
        case 94:  /* OX */
        case 107: /* GOPHER */
        case 110: /* BAT */
        case 114: /* BAANTA */
        case 119: /* PENGUIN */
        case 120: /* OSTRICH */
        case 124: /* WYVELIN */
        case 125: /* FLYINSECT */
            return LORE_ANIMAL;

        /* veggie -- plants, oozes, fungal/parasitic life */
        case 19:  /* PARASITE */
        case 20:  /* SLIME */
        case 24:  /* TREE */
        case 25:  /* VEGGIE */
        case 55:  /* MOSS */
            return LORE_VEGGIE;

        /* demon -- Sneezy "diabolic": fiends, constructs, aberrations */
        case 8:   /* LYCANTH */
        case 21:  /* DEMON */
        case 26:  /* ELEMENT */
        case 28:  /* DEVIL */
        case 33:  /* MFLAYER */
        case 37:  /* GOLEM */
        case 95:  /* GREMLIN */
        case 117: /* MIMIC */
        case 118: /* MEDUSA */
            return LORE_DEMON;

        /* reptile -- scaled and draconic life */
        case 9:   /* DRAGON */
        case 14:  /* DINOSAUR */
        case 22:  /* SNAKE */
        case 29:  /* FROGMAN */
        case 39:  /* PANTATH */
        case 48:  /* AMPHIB */
        case 50:  /* REPTILE */
        case 82:  /* DRAGONNE */
        case 92:  /* NAGA */
        case 105: /* LIZARD_MAN */
        case 112: /* WYVERN */
        case 121: /* TROG */
        case 122: /* COATL */
            return LORE_REPTILE;

        /* undead */
        case 10:  /* UNDEAD */
        case 38:  /* BANSHEE */
        case 49:  /* VAMPIRE */
        case 52:  /* VAMPIREBAT */
            return LORE_UNDEAD;

        /* giant -- giantkin and the goblinoid/large-humanoid line */
        case 6:   /* OGRE */
        case 17:  /* GIANT */
        case 30:  /* GOBLIN */
        case 31:  /* TROLL */
        case 43:  /* TYTAN */
        case 72:  /* MOUND */
        case 73:  /* PIERCER */
        case 77:  /* SPHINX */
        case 78:  /* SHEDU */
        case 79:  /* LAMMASU */
        case 81:  /* PHOENIX */
        case 100: /* BUGBEAR */
        case 103: /* KOBOLD */
        case 116: /* HOBGOBLIN */
            return LORE_GIANT;

        /* people -- sapient humanoids */
        case 0:   /* NORACE */
        case 1:   /* HUMAN */
        case 2:   /* ELVEN */
        case 3:   /* DWARF */
        case 4:   /* HOBBIT */
        case 5:   /* GNOME */
        case 11:  /* ORC */
        case 18:  /* BIRDMAN */
        case 35:  /* FAERIE */
        case 36:  /* DROW */
        case 42:  /* FISHMAN */
        case 44:  /* WOODELF */
        case 98:  /* SATYR */
        case 99:  /* DRYAD */
        case 101: /* MINOTAUR */
        case 111: /* PYGMY */
        case 113: /* KUOTOA */
        case 115: /* GNOLL */
        case 126: /* RATMAN */
            return LORE_PEOPLE;

        /* other -- outsiders and one-off oddities (Sneezy lore other) */
        default:
            return LORE_OTHER;
    }
}

/* The roster skill name for a lore category (skill.c SKILLS[]).  Returned
 * strings are stable literals suitable for being_knows_skill()/skill_find(). */
const char *mob_lore_skill_name(mob_lore_t cat) {
    switch (cat) {
        case LORE_ANIMAL:  return "know animal";
        case LORE_VEGGIE:  return "know veggie";
        case LORE_DEMON:   return "know demon";
        case LORE_REPTILE: return "know reptile";
        case LORE_UNDEAD:  return "know undead";
        case LORE_GIANT:   return "know giantkin";
        case LORE_PEOPLE:  return "know people";
        case LORE_OTHER:   return "know other";
    }
    return "know other";
}

/* Short human phrase for the lore field, used in the `know` flavor line
 * ("Using your knowledge of <X> ..."). */
const char *mob_lore_field_name(mob_lore_t cat) {
    switch (cat) {
        case LORE_ANIMAL:  return "animals";
        case LORE_VEGGIE:  return "plants and fungi";
        case LORE_DEMON:   return "demons and aberrations";
        case LORE_REPTILE: return "reptiles and dragonkind";
        case LORE_UNDEAD:  return "the undead";
        case LORE_GIANT:   return "giantkin";
        case LORE_PEOPLE:  return "the civilized peoples";
        case LORE_OTHER:   return "strange creatures";
    }
    return "strange creatures";
}

static const char *mob_lore_hp_ratio_word(int mob_max, int self_max) {
    if (self_max <= 0)
        self_max = 1;
    /* mob HP as a multiple of the studier's own -- descriptive, never a
     * raw number, same spirit as Sneezy's DescRatio(). */
    int pct = (mob_max * 100) / self_max;
    if (pct >= 400) return "vastly greater than your own";
    if (pct >= 200) return "far greater than your own";
    if (pct >= 130) return "greater than your own";
    if (pct >= 80)  return "about the same as your own";
    if (pct >= 45)  return "less than your own";
    return "far less than your own";
}

static const char *mob_lore_ac_word(int ac) {
    if (ac >= 40) return "all but impenetrable";
    if (ac >= 25) return "very well protected";
    if (ac >= 12) return "well protected";
    if (ac >= 5)  return "lightly protected";
    if (ac > 0)   return "poorly protected";
    return "unarmored";
}

/* Auto-triggered Know-X reveal, shared by cmd_know.c (explicit `know`),
 * cmd_consider.c, and cmd_look.c (user 2026-08-24: "fix the know* skills
 * to be automatic when you look at or consider the target mob"). */
bool mob_lore_try_reveal(being_t *ch, being_t *victim, bool spend_wait,
                          char *out, size_t outsz, size_t *n) {
    if (!ch || !victim || victim == ch || victim->base.kind != THING_MOB)
        return false;
    mob_lore_t cat = mob_race_lore_category(victim->mob_race);
    const char *skname = mob_lore_skill_name(cat);
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, skname))
        return false;
    /* Learn-by-doing + proficiency read. Immortals read at full mastery. */
    int learn = 100;
    if (!imm) {
        const skill_def_t *sk = skill_find(ch->char_class, skname, false);
        if (sk) {
            learn = skill_learn_from_doing(ch, sk);
            if (learn < skill_proficiency(ch, sk))
                learn = skill_proficiency(ch, sk);
        }
        if (spend_wait)
            being_set_wait(ch, 12); /* ~1 combat round of study */
    }
    /* Nice-cased race name. */
    char race[48];
    snprintf(race, sizeof(race), "%s", mob_race_name(victim->mob_race));
    for (char *p = race; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    const char *art = strchr("aeiou", race[0]) ? "an" : "a";
    const char *vname = victim->base.short_descr[0] ? victim->base.short_descr
                                                    : victim->base.name;
    *n += (size_t)snprintf(out + *n, outsz - *n,
                     "<g>Drawing on your knowledge of %s, you discern that %s is %s %s.<1>\r\n",
                     mob_lore_field_name(cat), vname, art, race);
    /* Reveal ladder, gated by proficiency (Sneezy's learnedness tiers). */
    if (learn > 20)
        *n += (size_t)snprintf(out + *n, outsz - *n,
                      "<c>Vitality:<1> its constitution seems %s.\r\n",
                      mob_lore_hp_ratio_word(victim->progress.max_hp, ch->progress.max_hp));
    if (learn > 45)
        *n += (size_t)snprintf(out + *n, outsz - *n,
                      "<c>Defenses:<1> it appears %s.\r\n",
                      mob_lore_ac_word(being_total_ac(victim)));
    if (learn > 70) {
        const char *disp = victim->mob_align > 200 ? "benevolent"
                         : victim->mob_align < -200 ? "malevolent"
                         : "indifferent";
        *n += (size_t)snprintf(out + *n, outsz - *n,
                      "<c>Disposition:<1> it regards the world as %s.\r\n", disp);
    }
    if (!imm && learn <= 20)
        *n += (size_t)snprintf(out + *n, outsz - *n,
                      "Deeper study will come with practice.\r\n");
    return true;
}
