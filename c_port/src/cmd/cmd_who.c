/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "world.h"

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

/* Copies `title` into `out`, replacing every "<N>"/"<n>" token with `name`
 * (the character's name may appear anywhere in the title -- user spec, e.g.
 * "You are not paranoid, <N> really is out to get you!"). Returns true if at
 * least one token was substituted, so the caller knows the name is already
 * embedded and can skip printing it separately. Other tags (colors) pass
 * through untouched for the colorstring translator. */
static bool title_with_name(const char *title, const char *name, char *out, size_t outsz) {
    size_t o = 0;
    bool did = false;
    for (size_t i = 0; title[i] != '\0';) {
        if (o + 1 >= outsz)
            break;
        if (title[i] == '<' && (title[i + 1] == 'N' || title[i + 1] == 'n')
            && title[i + 2] == '>') {
            for (const char *p = name; *p && o + 1 < outsz; p++)
                out[o++] = *p;
            i += 3;
            did = true;
        } else {
            out[o++] = title[i++];
        }
    }
    out[o] = '\0';
    return did;
}

/* Case-insensitive substring test (strcasestr is GNU-only; do it by hand
 * for portability, same as cmd_log.c). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0)
        return true;
    for (const char *p = haystack; *p; p++) {
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    }
    return false;
}

/* Argument filter for who: everyone, only immortals, only mortals, or a
 * case-insensitive name substring. */
typedef enum { WHO_ALL, WHO_IMMS, WHO_MORTS, WHO_NAME } who_filter_t;

static bool who_matches(who_filter_t f, const char *needle, being_t *ch) {
    switch (f) {
        case WHO_IMMS:  return being_is_immortal(ch);
        case WHO_MORTS: return !being_is_immortal(ch);
        case WHO_NAME:  return ci_contains(ch->base.name, needle);
        case WHO_ALL:
        default:        return true;
    }
}

bool cmd_who(descriptor_t *d, const char *args) {
    /* Parse an optional filter argument. Keywords "imm[ortals]"/"mort[als]"
     * scope by rank; anything else is treated as a name substring. */
    while (*args == ' ')
        args++;
    who_filter_t filter = WHO_ALL;
    char needle[64] = "";
    if (*args) {
        char tok[64];
        sscanf(args, "%63s", tok);
        if (strncasecmp(tok, "imm", 3) == 0)
            filter = WHO_IMMS;
        else if (strncasecmp(tok, "mort", 4) == 0)
            filter = WHO_MORTS;
        else {
            filter = WHO_NAME;
            snprintf(needle, sizeof(needle), "%s", tok);
        }
    }

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\n-- Who's online --\r\n");
    if (n < 0)
        n = 0;

    int shown = 0;
    for (descriptor_t *o = g_descriptors; o; o = o->next) {
        /* NOT `state != CONN_PLAYING` -- that excludes every editor sub-
         * state, making a builder mid-edroom/edplayer/edzone vanish from
         * who entirely even though they're online (Session 43 audit). */
        if (!o->character)
            continue;
        if (!who_matches(filter, needle, o->character))
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

        /* Idle for over five minutes -> tagged (idle); any input clears it. */
        bool idle = (long)time(NULL) - o->last_active > 300;
        /* Tint the level/rank BRACKET by immortal rank tier (mortals: no
         * color) -- the name itself stays uncolored (user spec). */
        const char *col = being_rank_color(o->character->progress.level);
        const char *reset = col[0] ? "<z>" : "";
        /* Player-set title. If it contains a <N> token the name is embedded
         * in the title (shown alone); otherwise the title just trails the
         * name (" the Brave"). Empty title = name only. */
        const char *ptitle = o->character->title;
        const char *idletag = idle ? " (idle)" : "";
        if (ptitle[0]) {
            char dtitle[BEING_TITLE_LEN + PLAYER_NAME_LEN + 8];
            bool embedded = title_with_name(ptitle, o->character->base.name,
                                            dtitle, sizeof(dtitle));
            if (embedded)
                n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s[%s]%s %s%s\r\n",
                              col, centered, reset, dtitle, idletag);
            else
                n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s[%s]%s %s %s%s\r\n",
                              col, centered, reset, o->character->base.name, dtitle, idletag);
        } else {
            n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s[%s]%s %s%s\r\n",
                          col, centered, reset, o->character->base.name, idletag);
        }
        shown++;
    }

    if (shown == 0 && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  No one%s is online.\r\n",
                      filter == WHO_ALL ? "" : " matching that");

    /* Summary footer (user 2026-07-17: "who should report player count
     * (active links) and linkdeads in a total player count") -- always the
     * GLOBAL numbers regardless of any filter above, since this is a
     * server-health stat, not a scoped listing. A linkdead body (desc ==
     * NULL) has no descriptor_t at all, so it's invisible to the loop
     * above entirely -- world_count_linkdead() walks every room directly
     * to find them (see `purge linkdead`, cmd_purge.c, which uses the same
     * scan to remove them). */
    int active = 0;
    for (descriptor_t *o = g_descriptors; o; o = o->next) {
        if (o->character)
            active++;
    }
    int linkdead = world_count_linkdead();
    if ((size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n,
                      "\r\n<c>Active: [%d]  Linkdead: [%d]  Total players: [%d]<z>\r\n",
                      active, linkdead, active + linkdead);

    descriptor_send(d, out);
    return true;
}
