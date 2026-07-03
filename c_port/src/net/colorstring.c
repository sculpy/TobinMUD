#include "colorstring.h"

#include <string.h>

/* Non-immortal-only subset of the original's color table (sys/ansi.h /
 * colorstring.cc). Uppercase = bold/bright variant, except K/k which the
 * original defines the other way around (K = plain black, k = bold gray)
 * -- kept faithful to that quirk rather than "fixed", since matching the
 * original's actual behavior is the point. */
static const char *ansi_for_tag(char c) {
    switch (c) {
        case 'r': return "\033[31m";
        case 'R': return "\033[1;31m";
        case 'g': return "\033[32m";
        case 'G': return "\033[1;32m";
        case 'b': return "\033[34m";
        case 'B': return "\033[1;34m";
        case 'y': case 'o': return "\033[33m";
        case 'Y': case 'O': return "\033[1;33m";
        case 'p': return "\033[35m";
        case 'P': return "\033[1;35m";
        case 'c': return "\033[36m";
        case 'C': return "\033[1;36m";
        case 'w': return "\033[37m";
        case 'W': return "\033[1;37m";
        case 'K': return "\033[30m";
        case 'k': return "\033[1;30m";
        case '1': case 'z': case 'Z': return "\033[0m";
        default: return NULL;
    }
}

static const char ANSI_RESET[] = "\033[0m";

size_t colorstring_translate_maxlen(size_t src_len) {
    /* Worst case is ~7/3 bytes per source byte (every "<X>" -> up to
     * "\033[1;31m") plus a possible trailing auto-reset; *7 is a
     * generously safe upper bound, not a tight one. */
    return src_len * 7 + sizeof(ANSI_RESET);
}

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
            memcpy(dst + di, ANSI_RESET, reset_len);
            di += reset_len;
        }
    }

    if (di < dst_size)
        dst[di] = '\0';
    else if (dst_size > 0)
        dst[dst_size - 1] = '\0';

    return di;
}
