/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

/* `edit social [name]` (55+): menu-driven editor for socials/emotes (the
 * `social` DB table, socials.c/social_repo.h) -- the second half of the
 * "Socials -> DB + full Sneezy set + edsocial" TODO.md item, the DB-port
 * half having shipped earlier the same day. No argument opens the full
 * list (browse, or type a name/"new"); an exact existing name jumps
 * straight to that social's detail view. See descriptor_edsocial_begin()
 * and the CONN_EDSOCIAL_* cases in descriptor.c for the actual menus. */
bool cmd_edsocial(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char name[80];
    if (sscanf(args, "%79s", name) != 1)
        name[0] = '\0';

    descriptor_edsocial_begin(d, name);
    return true;
}
