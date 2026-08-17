/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "gmcp.h"

#include <stdio.h>
#include <string.h>

/* JSON-escapes `src` into `dst` (quote, backslash, and control bytes
 * only -- the small subset that can actually appear in Tobin's own
 * DB-sourced short room names/descriptions; a full \uXXXX escaper for
 * arbitrary Unicode control points is more than this needs). Always
 * null-terminates; truncates (rather than overflowing) if `src` is
 * too long for `dst`. */
static void json_escape(const char *src, char *dst, size_t dst_sz) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && o + 2 < dst_sz; p++) {
        if (*p == '"' || *p == '\\') {
            dst[o++] = '\\';
            dst[o++] = (char)*p;
        } else if (*p == '\n') {
            dst[o++] = '\\';
            dst[o++] = 'n';
        } else if (*p == '\r') {
            dst[o++] = '\\';
            dst[o++] = 'r';
        } else if (*p < 0x20) {
            continue; /* other control bytes: drop rather than escape */
        } else {
            dst[o++] = (char)*p;
        }
    }
    dst[o] = '\0';
}

size_t gmcp_build_char_vitals(char *buf, size_t bufsz, int hp, int maxhp, int vit, int maxvit,
                               int mana, int maxmana, const char *manalabel) {
    /* manalabel is a fixed class label (Mana/Piety/Lifeforce), no JSON
     * escaping needed. A client uses it to title its resource gauge. */
    int n = snprintf(buf, bufsz,
                      "Char.Vitals {\"hp\":%d,\"maxhp\":%d,\"vit\":%d,\"maxvit\":%d,"
                      "\"mana\":%d,\"maxmana\":%d,\"manalabel\":\"%s\"}",
                      hp, maxhp, vit, maxvit, mana, maxmana,
                      manalabel ? manalabel : "Mana");
    if (n < 0 || (size_t)n >= bufsz)
        return 0;
    return (size_t)n;
}

size_t gmcp_build_room_info(char *buf, size_t bufsz, int vnum, const char *name) {
    char escaped[192];
    json_escape(name, escaped, sizeof(escaped));
    int n = snprintf(buf, bufsz, "Room.Info {\"num\":%d,\"name\":\"%s\"}", vnum, escaped);
    if (n < 0 || (size_t)n >= bufsz)
        return 0;
    return (size_t)n;
}
