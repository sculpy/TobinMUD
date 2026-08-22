/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"

/* Runs the `affects` command: lists whatever buffs/debuffs are
 * currently active on the caller, and how many rounds each has left,
 * or says plainly that there aren't any. */
bool cmd_affects(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    char out[512];
    size_t n = 0;
    n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n<c>-- Active affects --<z>\r\n");

    bool any = false;
    for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
        if (ch->affects[i].type == AFFECT_NONE)
            continue;
        any = true;
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %d round%s left\r\n",
                              affect_name(ch->affects[i].type), ch->affects[i].rounds_left,
                              ch->affects[i].rounds_left == 1 ? "" : "s");
    }
    if (!any)
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  (none)\r\n");

    descriptor_send(d, out);
    return true;
}
