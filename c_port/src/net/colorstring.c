/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "colorstring.h"

#include <string.h>

/* Non-immortal-only subset of the original's color table (sys/ansi.h /
 * colorstring.cc). Uppercase = bold/bright variant, except K/k which the
 * original defines the other way around (K = plain black, k = bold gray)
 * -- kept faithful to that quirk rather than "fixed", since matching the
 * original's actual behavior is the point. */
static const char *ansi_for_tag(char c) {
    switch (c) {
        /* Lowercase (regular-intensity) tags lead with "0;" -- a bare
         * "\033[36m" does NOT clear a preceding bold (SGR intensity and
         * color are independent parameters; most terminals leave bold
         * stuck on until it's explicitly cleared), so a bright tag
         * followed by a regular one of the same letter (e.g. <C>text<c>)
         * rendered everything bold. Bug found colorizing the pager's
         * MORE prompt (Session 43, user: "highlight the available
         * command/keys in bright. the rest regular") -- "0;" forces a
         * full attribute reset before applying the color, so regular
         * really is regular regardless of what came before. */
        case 'r': return "\033[0;31m";
        case 'R': return "\033[1;31m";
        case 'g': return "\033[0;32m";
        case 'G': return "\033[1;32m";
        case 'b': return "\033[0;34m";
        case 'B': return "\033[1;34m";
        case 'y': case 'o': return "\033[0;33m";
        case 'Y': case 'O': return "\033[1;33m";
        case 'p': case 'm': return "\033[0;35m"; /* 'm' = magenta alias for 'p' (purple) */
        case 'P': case 'M': return "\033[1;35m";
        case 'c': return "\033[0;36m";
        case 'C': return "\033[1;36m";
        case 'w': return "\033[0;37m";
        case 'W': return "\033[1;37m";
        case 'K': return "\033[0;30m";
        case 'k': return "\033[1;30m";
        /* Sneezy's <d>/<D>: a standalone BOLD toggle (sys/colorstring.cc,
         * `buf += ch->bold()`), distinct from the R/G/B/... tags above
         * (which already bundle bold into their own bright/uppercase
         * variant) -- <d> stacks bold onto whatever color is already
         * active, e.g. "<g><d>bold green<z>", rather than setting a color
         * itself. Investigated + added per user request (Session 43). */
        case 'd': case 'D': return "\033[1m";
        case '1': case 'z': case 'Z': return "\033[0m";
        default: return NULL;
    }
}

static const char ANSI_RESET[] = "\033[0m";

/* <h>/<H> aren't color codes at all -- in the original (sys/colorstring.cc's
 * colorString()) they're a name-template substitution: <h> inserts
 * MUD_NAME ("SneezyMUD"), <H> the versioned MUD_NAME_VERS string, into
 * seeded content so it isn't hardcoded to one mud's name (e.g. an obj's
 * long_desc reading "The <h> new player's guide"). Never ported here, so
 * they fell through to the "unrecognized tag, pass through literally"
 * branch below and showed up as literal "<h>" text in-game (user,
 * 2026-07-17: "ive noticed a <h> tag ... what does <h> mean and get it
 * working"). Tobin has no separate versioned-name string, so <H> aliases
 * the same text as <h>. Matches the original's asymmetry: substituted
 * when colors are on, stripped to nothing when they're off (same as
 * every other tag below), NOT left as literal text either way. */
#define TOBIN_MUD_NAME "TobinMUD"

/* Upper bound on translated output size for a source string of src_len
 * bytes -- callers use this to size the dst buffer passed to
 * colorstring_translate() before calling it. */
size_t colorstring_translate_maxlen(size_t src_len) {
    /* Worst case is ~7/3 bytes per source byte (every "<X>" -> up to
     * "\033[1;31m") plus a possible trailing auto-reset; *7 is a
     * generously safe upper bound, not a tight one. */
    return src_len * 7 + sizeof(ANSI_RESET);
}

/* The port's replacement for the original colorString(): scans src for
 * "<X>" tags and emits the matching ANSI escape (or strips the tag if
 * color_on is false), copying everything else through unchanged. Meant
 * to be called once per recipient at the single output choke-point, not
 * scattered through content-producing code. */
size_t colorstring_translate(const char *src, char *dst, size_t dst_size, bool color_on) {
    size_t len = strlen(src);
    size_t si = 0, di = 0;
    /* Deviation from the original (which trusts content to reset itself):
     * remember the last color code emitted, and if the message ends still
     * "inside" a color, append a reset so a missing <z> can't bleed into
     * the prompt and every message after it. Explicit <z> is unaffected. */
    const char *last_code = NULL;

    while (si < len) {
        if (src[si] == '<' && si + 2 < len && src[si + 2] == '>') {
            if (src[si + 1] == 'h' || src[si + 1] == 'H') {
                if (color_on) {
                    size_t name_len = strlen(TOBIN_MUD_NAME);
                    if (di + name_len < dst_size) {
                        memcpy(dst + di, TOBIN_MUD_NAME, name_len);
                        di += name_len;
                    }
                }
                si += 3;
                continue;
            }
            const char *code = ansi_for_tag(src[si + 1]);
            if (code) {
                if (color_on) {
                    size_t code_len = strlen(code);
                    if (di + code_len < dst_size) {
                        memcpy(dst + di, code, code_len);
                        di += code_len;
                    }
                    last_code = code;
                }
                /* color off -> tag is stripped (nothing written) */
                si += 3;
                continue;
            }
            /* well-formed but unrecognized tag -- pass it through
             * literally rather than silently eating it */
            for (int k = 0; k < 3; k++) {
                if (di + 1 < dst_size)
                    dst[di++] = src[si + k];
            }
            si += 3;
            continue;
        }

        if (di + 1 < dst_size)
            dst[di++] = src[si];
        si++;
    }

    if (last_code != NULL && strcmp(last_code, ANSI_RESET) != 0) {
        size_t reset_len = sizeof(ANSI_RESET) - 1;
        if (di + reset_len < dst_size) {
            /* Land the reset BEFORE any trailing line break, not after it
             * (user finding, Session 20): a reset that arrives on the next
             * line leaves the break itself colored, and picky clients
             * (Windows telnet among them) paint artifacts from that. */
            size_t p = di;
            while (p > 0 && (dst[p - 1] == '\n' || dst[p - 1] == '\r'))
                p--;
            memmove(dst + p + reset_len, dst + p, di - p);
            memcpy(dst + p, ANSI_RESET, reset_len);
            di += reset_len;
        }
    }

    if (di < dst_size)
        dst[di] = '\0';
    else if (dst_size > 0)
        dst[dst_size - 1] = '\0';

    return di;
}
