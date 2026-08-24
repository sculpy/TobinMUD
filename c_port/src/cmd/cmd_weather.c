/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "gametime.h"
#include "weather.h"

/* `weather` (Sneezy → Tobin feature audit, "Weather & light levels").
 * Checked Sneezy's own help topic first: "displays the present weather
 * conditions as well as a theoretical prediction about the future" --
 * same two-part shape here (current condition + weather_forecast_hint()'s
 * fixed flavor line), just without the real pressure-trend math behind
 * the original's actual prediction (see weather.h's own doc comment for
 * why that's trimmed). Also notes day/night, since gametime_is_daytime()
 * already existed and is the other half of "light levels" (see
 * cmd_look.c/cmd_exits.c's darkness gate for where that actually matters
 * mechanically). */
bool cmd_weather(descriptor_t *d, const char *args) {
    (void)args;

    char msg[256];
    snprintf(msg, sizeof(msg), "The sky is %s. %s\r\nIt is currently %s outside.\r\n",
             weather_name(weather_current()), weather_forecast_hint(),
             gametime_is_daytime() ? "daylight" : "dark");
    descriptor_send(d, msg);
    return true;
}
