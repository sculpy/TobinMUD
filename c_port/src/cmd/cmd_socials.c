/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "socials.h"

#define SOCIALS_PAGE_SIZE 20 /* lines per page (the descriptor pager, like `news`) */

/* `socials`: lists the available social verbs (smile, nod, wave, ...). Each
 * is used as its own command -- `smile` or `smile <name>`. Full port of the
 * upstream set (~155 verbs, db/import-socials.py) comfortably overflows a
 * single screen, so this is paged like `news`/`vnum`/`help`. Four columns
 * (not one comma-joined line, like `help`'s send_columns()) so the pager --
 * which counts '\n' bytes, not wrapped screen rows -- actually sees enough
 * lines to page instead of dumping the whole list in one shot. */
bool cmd_socials(descriptor_t *d, const char *args) {
    (void)args;
    int cnt = social_cache_count();

    char out[8192];
    size_t n = (size_t)snprintf(out, sizeof(out), "\r\nSocials you can use:\r\n");
    for (int i = 0; i < cnt && n < sizeof(out); i++) {
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-14s", social_cache_at(i)->name);
        if (i % 4 == 3)
            n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0, "\r\n");
    }
    if (cnt % 4 != 0 && n < sizeof(out))
        n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n");
    if (n < sizeof(out))
        snprintf(out + n, sizeof(out) - n,
                "\r\nType one on its own (`smile`) or aim it at someone (`smile <name>`).\r\n");

    descriptor_page_start(d, out, SOCIALS_PAGE_SIZE);
    return true;
}
