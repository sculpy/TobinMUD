/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <strings.h>

#include "account.h"

/* Persists the connection's color choice to the account so it sticks across
 * logins (account.color_pref), matching the preference asked at creation. */
static void persist_color(descriptor_t *d) {
    d->account.color_pref = d->color_enabled;
    if (d->account.account_id > 0)
        account_set_color(d->account.account_id, d->color_enabled);
}

bool cmd_color(descriptor_t *d, const char *args) {
    if (strcasecmp(args, "on") == 0) {
        d->color_enabled = true;
        persist_color(d);
        descriptor_send(d, "Color is now ON.\r\n");
    } else if (strcasecmp(args, "off") == 0) {
        d->color_enabled = false;
        persist_color(d);
        descriptor_send(d, "Color is now OFF.\r\n");
    } else if (!*args) {
        descriptor_send(d, d->color_enabled
                             ? "Color is currently ON. Usage: color on|off\r\n"
                             : "Color is currently OFF. Usage: color on|off\r\n");
    } else {
        descriptor_send(d, "Usage: color on|off\r\n");
    }
    return true; /* instant -- no wait-state cost */
}
