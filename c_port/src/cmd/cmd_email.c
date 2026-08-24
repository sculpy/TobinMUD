/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "account.h"

/* `email [<address>|clear]` -- self-service equivalent of the account-
 * creation email prompt (descriptor.c's CONN_GET_EMAIL, user 2026-08-08),
 * same shape as `time <difference>`/`color on|off` doubling as both a
 * viewer and a setter for their own account.email. `clear` re-opts the
 * account out (empty string), same as pressing Enter at the creation
 * prompt. Never shared -- MUD-related communications only, same promise
 * the creation prompt makes. */
bool cmd_email(descriptor_t *d, const char *args) {
    if (!args || !args[0]) {
        char msg[320];
        if (d->account.email[0])
            snprintf(msg, sizeof(msg), "Your email is currently set to: %s\r\n"
                     "Usage: email <address> to change it, or `email clear` to opt out.\r\n",
                     d->account.email);
        else
            snprintf(msg, sizeof(msg), "You have not provided an email address (opted out).\r\n"
                     "Usage: email <address> to set one. We never share it -- "
                     "TobinMUD-related communications only.\r\n");
        descriptor_send(d, msg);
        return true;
    }

    if (strcasecmp(args, "clear") == 0) {
        d->account.email[0] = '\0';
        account_set_email(d->account.account_id, "");
        descriptor_send(d, "Email cleared -- you're opted out.\r\n");
        return true;
    }

    const char *at = strchr(args, '@');
    if (!at || at == args || !at[1] || strchr(at + 1, '.') == NULL) {
        descriptor_send(d, "That doesn't look like a valid email address.\r\n"
                            "Usage: email <address>, or `email clear` to opt out.\r\n");
        return true;
    }

    snprintf(d->account.email, sizeof(d->account.email), "%s", args);
    account_set_email(d->account.account_id, args);
    descriptor_send(d, "Email updated.\r\n");
    return true;
}
