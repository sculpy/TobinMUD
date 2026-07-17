/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "practice.h"

/* `balance class|race <name>` (Implementor-only, 60+ -- user 2026-07-12:
 * "a balance command (60) where you take args: balance <class|race>
 * that is menu driven to adjust balance numbers/modifiers that will
 * apply gamewide to the class or race you just balanced"). Resolves
 * the class/race name (abbreviation-friendly, same prefix-match
 * convention as edplayer's class/race fields), then opens the menu-
 * driven balance editor (CONN_BALANCE_* in descriptor.c) on it: HP
 * multiplier, damage multiplier, to-hit modifier, AC modifier -- see
 * balance.h for how these apply gamewide (being_calc_max_hp(),
 * combat_strike(), being_total_ac()). */
bool cmd_balance(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    while (*args == ' ')
        args++;

    char kind[16] = "", name[32] = "";
    sscanf(args, "%15s %31s", kind, name);

    if (!kind[0]) {
        descriptor_send(d, "Usage: balance class <name>  |  balance race <name>  |  balance wisdom [<value>]\r\n");
        return true;
    }

    /* `balance wisdom [<value>]` -- view or set the wisdom->practice-points
     * scalar (practice.h). No value = display, value = update cache + DB. */
    if (strncasecmp(kind, "wisdom", strlen(kind)) == 0) {
        if (!name[0]) {
            char buf[80];
            snprintf(buf, sizeof(buf), "Wisdom practice modifier: <c>%g<z>\r\n",
                     wisdom_practice_modifier());
            descriptor_send(d, buf);
        } else {
            double v = atof(name);
            wisdom_practice_modifier_set(v);
            char buf[80];
            snprintf(buf, sizeof(buf), "Wisdom practice modifier set to <c>%g<z>.\r\n", v);
            descriptor_send(d, buf);
        }
        return true;
    }

    bool is_class;
    if (strncasecmp(kind, "class", strlen(kind)) == 0) {
        is_class = true;
    } else if (strncasecmp(kind, "race", strlen(kind)) == 0) {
        is_class = false;
    } else {
        descriptor_send(d, "Usage: balance class <name>  or  balance race <name>\r\n");
        return true;
    }

    if (!name[0]) {
        descriptor_send(d, is_class
            ? "Balance which class? mage, cleric, warrior, thief, druid, or monk.\r\n"
            : "Balance which race? human, elf, ogre, dwarf, hobbit, or gnome.\r\n");
        return true;
    }

    int index = -1;
    if (is_class) {
        static const char *const NAMES[CLASS_COUNT] =
            { "mage", "cleric", "warrior", "thief", "druid", "monk" };
        for (int i = 0; i < CLASS_COUNT; i++) {
            if (strncasecmp(NAMES[i], name, strlen(name)) == 0) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            descriptor_send(d, "Usage: mage, cleric, warrior, thief, druid, or monk.\r\n");
            return true;
        }
    } else {
        static const char *const NAMES[RACE_COUNT] =
            { "human", "elf", "ogre", "dwarf", "hobbit", "gnome" };
        for (int i = 0; i < RACE_COUNT; i++) {
            if (strncasecmp(NAMES[i], name, strlen(name)) == 0) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            descriptor_send(d, "Usage: human, elf, ogre, dwarf, hobbit, or gnome.\r\n");
            return true;
        }
    }

    if (!descriptor_balance_begin(d, is_class, index))
        descriptor_send(d, "That class/race couldn't be loaded.\r\n");
    return true;
}
