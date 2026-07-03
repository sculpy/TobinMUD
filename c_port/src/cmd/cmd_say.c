#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "room.h"
#include "thing.h"

/* `say <message>` (and the `'` one-character shorthand -- see the special
 * case in cmd_table.c's cmd_dispatch()) broadcasts to everyone else in the
 * speaker's room: the speaker sees `You say, "<message>"`, everyone else
 * sees `<Name> says, "<message>"`. Mirrors the original's TBeing::doSay()
 * (misc/talk.cc): same message format, same empty-message guard, and no
 * auto-added punctuation -- whatever the player typed is used verbatim.
 * Not replicated: the original's garble() (drunk/language distortion) and
 * color-coding of the name/message. */
bool cmd_say(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Yes, but WHAT do you want to say?\r\n");
        return true;
    }

    /* If the message might contain color tags, close it with <z> BEFORE
     * the closing quote, so the quote mark and everything after stay
     * uncolored even when the speaker never reset (user finding, Session
     * 20). Plain messages get nothing appended -- their output stays
     * byte-identical. */
    const char *close = strchr(args, '<') ? "<z>" : "";

    char msg[320];
    snprintf(msg, sizeof(msg), "You say, \"%s%s\"\r\n", args, close);
    descriptor_send(d, msg);

    room_t *r = d->character->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t == &d->character->base || t->kind != THING_PC)
            continue;
        being_t *other = (being_t *)t;
        if (!other->desc)
            continue;
        snprintf(msg, sizeof(msg), "%s says, \"%s%s\"\r\n", d->character->base.name, args, close);
        descriptor_send(other->desc, msg);
    }

    return true;
}
