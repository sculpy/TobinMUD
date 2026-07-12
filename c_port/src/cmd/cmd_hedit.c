/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "help_repo.h"

/* `hedit <topic>`: level-56+ in-game help editor (user-specified gate --
 * within the "God" title tier, above ordinary 51-53 immortals). Starts the
 * classic DikuMUD-style line editor: existing text is preloaded and shown,
 * typed lines are appended, "." on its own line saves to the DB, "~"
 * aborts. The actual line handling lives in descriptor.c's CONN_PLAYING
 * case (d->editing_help); this command just validates and arms it. */
bool cmd_hedit(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    char topic[HELP_TOPIC_NAME_LEN];
    if (sscanf(args, "%31s", topic) != 1) {
        descriptor_send(d, "Usage: edit help <topic>\r\n");
        return true;
    }
    for (char *p = topic; *p; p++)
        *p = (char)tolower((unsigned char)*p);

    snprintf(d->edit_topic, sizeof(d->edit_topic), "%s", topic);
    d->edit_buf[0] = '\0';
    d->edit_len = 0;
    d->edit_related[0] = '\0';

    char existing[HELP_BODY_MAX];
    bool exists = help_topic_load_exact(topic, existing, sizeof(existing));
    if (exists) {
        /* Pull a trailing "Related: ..." line out of the preloaded text
         * (same convention cmd_help.c displays) into edit_related, so
         * re-editing shows the clean body and the /r value is already
         * populated -- rather than showing "Related: ..." as if it were
         * ordinary body text the author has to notice and not duplicate. */
        size_t elen = strlen(existing);
        while (elen > 0 && (existing[elen - 1] == '\n' || existing[elen - 1] == '\r'))
            elen--;
        size_t last_nl = elen;
        for (size_t i = elen; i > 0; i--) {
            if (existing[i - 1] == '\n') { last_nl = i; break; }
            if (i == 1) last_nl = 0;
        }
        const char *last_line = existing + last_nl;
        size_t last_line_len = elen - last_nl;
        if (last_line_len > 8 && strncasecmp(last_line, "Related:", 8) == 0) {
            const char *r = last_line + 8;
            while (*r == ' ')
                r++;
            size_t rlen = (size_t)(last_line + last_line_len - r);
            if (rlen >= sizeof(d->edit_related))
                rlen = sizeof(d->edit_related) - 1;
            memcpy(d->edit_related, r, rlen);
            d->edit_related[rlen] = '\0';
            elen = last_nl;
            while (elen > 0 && (existing[elen - 1] == '\n' || existing[elen - 1] == '\r'))
                elen--;
            existing[elen] = '\0';
        }
        snprintf(d->edit_buf, sizeof(d->edit_buf), "%s", existing);
        d->edit_len = (int)strlen(d->edit_buf);
    }

    char head[320];
    int hn = snprintf(head, sizeof(head),
             "\r\n-- Editing help topic '%s' (%s) --\r\n"
             "Type lines to append. /s saves, /a aborts, /b blanks, "
             "/f reflows to width, /r <topics> sets the Related: footer "
             "(bare /r clears it).\r\n",
             topic, exists ? "existing text below" : "new topic");
    if (d->edit_related[0] && hn > 0 && (size_t)hn < sizeof(head))
        snprintf(head + hn, sizeof(head) - (size_t)hn,
                 "Current related topics: %s\r\n", d->edit_related);
    descriptor_send(d, head);
    if (exists) {
        descriptor_send(d, existing);
        if (existing[0] && existing[strlen(existing) - 1] != '\n')
            descriptor_send(d, "\r\n");
    }
    descriptor_send(d, "] ");
    d->edit_kind = EDIT_HELP_TOPIC;
    return true;
}
