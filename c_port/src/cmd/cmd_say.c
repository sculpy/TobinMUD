/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "room.h"
#include "thing.h"
#include "trigger.h"

/* `say <message>` (and the `'` one-character shorthand -- see the special
 * case in cmd_table.c's cmd_dispatch()) broadcasts to everyone else in the
 * speaker's room: the speaker sees `You say, "<message>"`, everyone else
 * sees `<Name> says, "<message>"`. Mirrors the original's TBeing::doSay()
 * (misc/talk.cc): same message format, same empty-message guard, and no
 * auto-added punctuation -- whatever the player typed is used verbatim.
 * Not replicated: the original's garble() (drunk/language distortion) and
 * color-coding of the name/message. */
/* Case-insensitive "does haystack contain needle" (strcasestr is GNU-only,
 * same duplicated-helper precedent as cmd_scan.c/cmd_who.c/combat.c/...). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0)
        return true;
    for (; *haystack; haystack++)
        if (strncasecmp(haystack, needle, nl) == 0)
            return true;
    return false;
}

/* Fires every mob-in-`r`'s "speech" trigger whose match_text is a
 * substring of what was just said (user, 2026-07-11: "interaction with
 * mobs objs and room via scripts"). */
static void run_speech_triggers(being_t *speaker, room_t *r, const char *said) {
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *mob = (being_t *)t;
        trigger_t trigs[8];
        int n = trigger_repo_load_for("mob", mob->base.id, "speech", trigs, 8);
        if (n == 0)
            continue;
        char capbuf[128];
        snprintf(capbuf, sizeof(capbuf), "%s", mob->base.short_descr);
        if (capbuf[0])
            capbuf[0] = (char)toupper((unsigned char)capbuf[0]);
        for (int i = 0; i < n; i++) {
            if (!trigs[i].match_text[0] || !ci_contains(said, trigs[i].match_text))
                continue;
            trigger_run(&trigs[i], speaker, r, capbuf[0] ? capbuf : NULL);
        }
    }
}

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
    snprintf(msg, sizeof(msg), "<c>You say, \"<z>%s<c>\"<z>\r\n", args);
    descriptor_send(d, msg);

    room_t *r = d->character->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t == &d->character->base || t->kind != THING_PC)
            continue;
        being_t *other = (being_t *)t;
        if (!other->desc)
            continue;
        snprintf(msg, sizeof(msg), "<c>%s says, \"<z>%s<c>\"<z>\r\n",
                 d->character->base.name, args);
        descriptor_notify(other->desc, msg);
    }

    run_speech_triggers(d->character, r, args);

    return true;
}
