/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "account.h"
#include "gametime.h"

/* `time`: the current mud clock, weekday, and date -- matches the shape
 * of Sneezy's own `time` command (misc/info.cc doTime()): "It is 3:45 PM,
 * on Wednesday" / "The 5th day of March, Year 1." Sunrise/sunset lines
 * dropped (weather-dependent, Tobin has no weather system yet -- see
 * gametime.h). Also shows real-world time adjusted by the account's
 * `time_adjust` offset (chosen at account creation), and `time
 * <difference>` re-sets that offset -- the personal real-time-zone
 * sub-feature, ported from Sneezy's doTime() (misc/info.cc). */
static const char *ordinal_suffix(int n) {
    if (n % 100 >= 11 && n % 100 <= 13)
        return "th";
    switch (n % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

/* `time [<offset>]` command -- see file-top comment for the full port
 * rationale. With an argument, just re-sets the caller's account-level
 * time_adjust timezone offset. With none, prints the mud clock/date via
 * gametime.h plus a real-world clock line shifted by that offset. */
bool cmd_time(descriptor_t *d, const char *args) {
    if (args && args[0]) {
        char *endptr = NULL;
        long hours = strtol(args, &endptr, 10);
        if (endptr == args || *endptr != '\0' || hours < -23 || hours > 23) {
            descriptor_send(d, "Usage: time <difference from Eastern, -23 to 23>\r\n");
            return true;
        }
        d->account.time_adjust = (int)hours;
        account_set_timezone(d->account.account_id, (int)hours);
        descriptor_send(d, "Time zone offset updated.\r\n");
        return true;
    }

    char clock[16];
    gametime_clock_string(gametime_hour(), gametime_minute(), clock, sizeof(clock));

    int day = gametime_day() + 1; /* display 1-indexed */
    char msg[256];
    int len = snprintf(msg, sizeof(msg),
             "It is %s, on %s\r\nThe %d%s day of %s, Year %d.\r\n",
             clock, gametime_weekday_name(gametime_weekday()),
             day, ordinal_suffix(day), gametime_month_name(gametime_month()),
             gametime_year());

    /* Real-world time: the server's own local (Eastern) clock, shifted by
     * the account's chosen offset. Only the clock face is shown, so plain
     * minute-of-day arithmetic avoids DST/date edge cases from mktime(). */
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    int total_min = ((tm_buf.tm_hour * 60 + tm_buf.tm_min) + d->account.time_adjust * 60) % 1440;
    if (total_min < 0)
        total_min += 1440;
    char real_clock[16];
    gametime_clock_string(total_min / 60, total_min % 60, real_clock, sizeof(real_clock));
    snprintf(msg + len, sizeof(msg) - (size_t)len,
             "It is %s where you are (real time).\r\n", real_clock);

    descriptor_send(d, msg);
    return true;
}
