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

    /* Colorized wrapper (user spec, Tier 3): the say framing -- name,
     * "say(s)," and the opening quote -- renders cyan, reset before the
     * message so the player's text shows as typed (including their own
     * color tags), and a final <z> before the closing quote so an
     * unterminated tag can never color the quote or bleed onward (the
     * Session 20 finding, preserved). Tags strip cleanly when color is
     * off. */
    char msg[336];
    snprintf(msg, sizeof(msg), "<c>You say, \"<z>%s<z>\"\r\n", args);
    descriptor_send(d, msg);

    room_t *r = d->character->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t == &d->character->base || t->kind != THING_PC)
            continue;
        being_t *other = (being_t *)t;
        if (!other->desc)
            continue;
        snprintf(msg, sizeof(msg), "<c>%s says, \"<z>%s<z>\"\r\n",
                 d->character->base.name, args);
        descriptor_send(other->desc, msg);
    }

    return true;
}
