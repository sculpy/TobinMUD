/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "gametime.h"

/* `timeshift <amount> [minutes|hours|days|months]` (Administrator, 59+):
 * shifts the shared game clock forward, or back with a negative amount, by
 * the given amount (defaults to hours). Lets staff jump the calendar -- for
 * instance to roll into a new game-month so the monthly treasury allocation
 * (treasury.c) and daily bank interest fire on their next tick. */
bool cmd_timeshift(descriptor_t *d, const char *args) {
    long amount = 0;
    char unit[16] = "";
    if (sscanf(args, "%ld %15s", &amount, unit) < 1) {
        char clk[16], msg[192];
        gametime_clock_string(gametime_hour(), gametime_minute(), clk, sizeof(clk));
        snprintf(msg, sizeof(msg),
                 "Usage: timeshift <amount> [minutes|hours|days|months]  (negative to rewind)\r\n"
                 "It is now %s, %s %d, Year %d.\r\n",
                 clk, gametime_month_name(gametime_month()), gametime_day() + 1, gametime_year());
        descriptor_send(d, msg);
        return true;
    }

    long minutes;
    if (unit[0] == '\0' || strncasecmp(unit, "hours", strlen(unit)) == 0)
        minutes = amount * 60;
    else if (strncasecmp(unit, "minutes", strlen(unit)) == 0)
        minutes = amount;
    else if (strncasecmp(unit, "days", strlen(unit)) == 0)
        minutes = amount * 60 * 24;
    else if (strncasecmp(unit, "months", strlen(unit)) == 0)
        minutes = amount * 60 * 24 * 28;   /* fixed 28-day calendar (gametime.c) */
    else {
        descriptor_send(d, "Unit must be minutes, hours, days, or months.\r\n");
        return true;
    }

    gametime_shift_minutes(minutes);

    char clk[16], msg[192];
    gametime_clock_string(gametime_hour(), gametime_minute(), clk, sizeof(clk));
    snprintf(msg, sizeof(msg),
             "Time shifted. It is now %s, %s, %s %d, Year %d.\r\n",
             clk, gametime_weekday_name(gametime_weekday()),
             gametime_month_name(gametime_month()), gametime_day() + 1, gametime_year());
    descriptor_send(d, msg);
    return true;
}
