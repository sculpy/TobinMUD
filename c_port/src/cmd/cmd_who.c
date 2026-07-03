#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

/* Fixed width for who's bracketed level/title field -- matches the longest
 * title ("Administrator", 13 chars) so every row lines up regardless of
 * whether it's showing a title or a "Level: N" fallback. */
#define WHO_LEVEL_FIELD_WIDTH 13

static void center_pad(char *out, size_t out_size, const char *text, int width) {
    int len = (int)strlen(text);
    if (len >= width) {
        snprintf(out, out_size, "%s", text);
        return;
    }
    int left_pad = (width - len) / 2;
    int right_pad = width - len - left_pad;
    snprintf(out, out_size, "%*s%s%*s", left_pad, "", text, right_pad, "");
}

bool cmd_who(descriptor_t *d, const char *args) {
    (void)args;

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\n-- Who's online --\r\n");
    if (n < 0)
        n = 0;

    for (descriptor_t *o = g_descriptors; o; o = o->next) {
        if (o->state != CONN_PLAYING || !o->character)
            continue;
        if ((size_t)n >= sizeof(out))
            break;

        char level_field[24];
        const char *title = being_level_title(o->character->progress.level);
        if (title)
            snprintf(level_field, sizeof(level_field), "%s", title);
        else
            snprintf(level_field, sizeof(level_field), "Level: %d", o->character->progress.level);

        char centered[32];
        center_pad(centered, sizeof(centered), level_field, WHO_LEVEL_FIELD_WIDTH);

        n += snprintf(out + n, sizeof(out) - (size_t)n, "  [%s] %s\r\n",
                      centered, o->character->base.name);
    }

    descriptor_send(d, out);
    return true;
}
