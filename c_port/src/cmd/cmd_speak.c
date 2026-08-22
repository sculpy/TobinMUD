/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "language.h"
#include "skill.h"

/* `speak [language]` (Tier-4 port, 2026-08-16). Sets the tongue this
 * character speaks in: from then on `say`/`whisper`/`tell` are garbled for
 * anyone who hasn't learned it (see language.c). With no argument, reports
 * the current tongue and lists the languages this character knows.
 *
 * Common is the default and is always available (switching back to the
 * lingua franca needs no skill); every other tongue must be `known`
 * (class + level gate via being_knows_skill, learned-by-doing thereafter),
 * exactly like any other roster skill. Immortals may speak anything. */
bool cmd_speak(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nobody.\r\n");
        return true;
    }

    /* No argument -> status + known-language list. */
    if (!args || !*args) {
        char out[512];
        snprintf(out, sizeof(out), "<c>You are currently speaking <z>%s<c>.<z>\r\n",
                 language_name(ch->spoken_language));
        descriptor_send(d, out);
        descriptor_send(d, "<c>Languages you can speak:<z>\r\n");
        /* Common is always speakable; list the rest you actually know. */
        descriptor_send(d, "  Common\r\n");
        for (int i = LANG_COMMON + 1; i < NUM_LANGUAGES; i++) {
            if (being_is_immortal(ch) ||
                being_knows_skill(ch, language_skill_name(i))) {
                snprintf(out, sizeof(out), "  %s\r\n", language_name(i));
                descriptor_send(d, out);
            }
        }
        descriptor_send(d, "<k>(Type `speak <language>` to switch.)<z>\r\n");
        return true;
    }

    int lang = language_by_name(args);
    if (lang < 0) {
        descriptor_send(d, "That's not a language anyone speaks.\r\n");
        return true;
    }

    if (lang != LANG_COMMON && !being_is_immortal(ch) &&
        !being_knows_skill(ch, language_skill_name(lang))) {
        char out[128];
        snprintf(out, sizeof(out), "You don't know how to speak %s.\r\n",
                 language_name(lang));
        descriptor_send(d, out);
        return true;
    }

    ch->spoken_language = lang;
    char out[128];
    snprintf(out, sizeof(out), "<c>You will now speak in <z>%s<c>.<z>\r\n",
             language_name(lang));
    descriptor_send(d, out);
    return true;
}
