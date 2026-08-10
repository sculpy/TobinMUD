/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "practice.h"
#include "rent.h"

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
        descriptor_send(d, "Usage: balance class <name> | balance race <name> | balance wisdom [<value>] | balance rent [tax|free <n>]\r\n");
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

    /* `balance rent [tax|free <value>]` -- view or set the gamewide rent-cost
     * settings (rent.h): the max-level tax and the free-below level. With no
     * subcommand it displays the current values and example costs. Needs >=2
     * letters so bare "r" still resolves to `balance race`, not rent. */
    if (strlen(kind) >= 2 && strncasecmp(kind, "rent", strlen(kind)) == 0) {
        char sub[16] = "", valstr[24] = "";
        sscanf(args, "%*s %15s %23s", sub, valstr);
        if (sub[0] && (strncasecmp(sub, "tax", strlen(sub)) == 0
                       || strncasecmp(sub, "free", strlen(sub)) == 0)) {
            if (!valstr[0]) {
                descriptor_send(d, "Usage: balance rent tax <gold>   |   balance rent free <level>\r\n");
                return true;
            }
            char buf[128];
            if (strncasecmp(sub, "tax", strlen(sub)) == 0) {
                rent_tax_at_max_set(atoi(valstr));
                snprintf(buf, sizeof(buf), "Rent tax at max level set to <c>%d<z> gold.\r\n", rent_tax_at_max());
            } else {
                rent_free_level_set(atoi(valstr));
                snprintf(buf, sizeof(buf), "Rent is now free at or below level <c>%d<z>.\r\n", rent_free_level());
            }
            descriptor_send(d, buf);
            return true;
        }
        int tax = rent_tax_at_max();
        long denom = (long)MORTAL_LEVEL_MAX * MORTAL_LEVEL_MAX * MORTAL_LEVEL_MAX;
        int c10 = (int)((long)tax * 10 * 10 * 10 / denom);
        int c25 = (int)((long)tax * 25 * 25 * 25 / denom);
        int c40 = (int)((long)tax * 40 * 40 * 40 / denom);
        char buf[512];
        snprintf(buf, sizeof(buf),
            "\r\n<c>Rent settings (gamewide, live):<z>\r\n"
            "  <p>Tax at max level (L%d)<z> : <c>%d<z> gold   (balance rent tax <n>)\r\n"
            "  <p>Free at/below level<z>    : <c>%d<z>       (balance rent free <n>)\r\n"
            "  Cost scales with level cubed; paid from wallet, then bank for any shortfall.\r\n"
            "  Examples at the current tax: L10 ~%d, L25 ~%d, L40 ~%d, L%d %d gold.\r\n",
            MORTAL_LEVEL_MAX, tax, rent_free_level(), c10, c25, c40, MORTAL_LEVEL_MAX, tax);
        descriptor_send(d, buf);
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
