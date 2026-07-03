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
        descriptor_send(d, "Usage: hedit <topic>\r\n");
        return true;
    }
    for (char *p = topic; *p; p++)
        *p = (char)tolower((unsigned char)*p);

    snprintf(d->edit_topic, sizeof(d->edit_topic), "%s", topic);
    d->edit_buf[0] = '\0';
    d->edit_len = 0;

    char existing[HELP_BODY_MAX];
    bool exists = help_topic_load_exact(topic, existing, sizeof(existing));
    if (exists) {
        snprintf(d->edit_buf, sizeof(d->edit_buf), "%s", existing);
        d->edit_len = (int)strlen(d->edit_buf);
    }

    char head[192];
    snprintf(head, sizeof(head),
             "\r\n-- Editing help topic '%s' (%s) --\r\n"
             "Type lines to append. '.' alone saves, '~' alone aborts.\r\n",
             topic, exists ? "existing text below" : "new topic");
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
