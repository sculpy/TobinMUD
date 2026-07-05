/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include "socials.h"

/* `socials`: lists the available social verbs (smile, nod, wave, ...). Each
 * is used as its own command -- `smile` or `smile <name>`. */
bool cmd_socials(descriptor_t *d, const char *args) {
    (void)args;
    char buf[600];
    social_names(buf, sizeof(buf));
    descriptor_send(d, "\r\nSocials you can use:\r\n  ");
    descriptor_send(d, buf);
    descriptor_send(d, "\r\nType one on its own (`smile`) or aim it at someone "
                       "(`smile <name>`).\r\n");
    return true;
}
