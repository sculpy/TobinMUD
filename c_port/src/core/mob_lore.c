/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "being.h"

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
