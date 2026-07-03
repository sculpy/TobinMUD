#include "cmd_internal.h"

#include <stdio.h>

#include "room.h"
#include "thing.h"

bool cmd_look(descriptor_t *d, const char *args) {
    (void)args;

    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    room_t *r = d->character->base.roomp;

    char out[ROOM_DESCRIPTION_MAX + 512];
    int n = snprintf(out, sizeof(out), "\r\n%s\r\n%s\r\n", r->base.name, r->description);
    if (n < 0)
        n = 0;

    int any = 0;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t == &d->character->base)
            continue;
        if ((size_t)n >= sizeof(out))
            break;
        if (!any) {
            n += snprintf(out + n, sizeof(out) - (size_t)n, "\r\n");
            any = 1;
        }
        if ((size_t)n >= sizeof(out))
            break;
        const char *label = t->short_descr[0] ? t->short_descr : t->name;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "%s is here.\r\n", label);
    }

    descriptor_send(d, out);
    return true;
}
