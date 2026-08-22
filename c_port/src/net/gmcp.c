/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "gmcp.h"

#include <stdbool.h>
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

/* Appends the `"exits":{...}` object (open, non-secret exits only) and
 * the closing `}` onto the "Room.Info {...\"exits\":{" prefix already
 * written into buf[0..written). Returns the new total length, or 0 if
 * it didn't fit. */
static size_t append_exits(char *buf, size_t bufsz, size_t written,
                            const int exits[ROOM_NUM_EXITS], const int exit_cond[ROOM_NUM_EXITS]) {
    bool first = true;
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        if (exits[i] < 0 || (exit_cond[i] & EXIT_COND_SECRET))
            continue;
        int n = snprintf(buf + written, bufsz - written, "%s\"%s\":%d",
                          first ? "" : ",", DIR_NAMES[i], exits[i]);
        if (n < 0 || written + (size_t)n >= bufsz)
            return 0;
        written += (size_t)n;
        first = false;
    }
    int n = snprintf(buf + written, bufsz - written, "}}");
    if (n < 0 || written + (size_t)n >= bufsz)
        return 0;
    return written + (size_t)n;
}

size_t gmcp_build_room_info(char *buf, size_t bufsz, int vnum, const char *name,
                             const int exits[ROOM_NUM_EXITS], const int exit_cond[ROOM_NUM_EXITS],
                             int x, int y, int z) {
    char escaped[192];
    json_escape(name, escaped, sizeof(escaped));
    int n = snprintf(buf, bufsz,
                      "Room.Info {\"num\":%d,\"name\":\"%s\",\"x\":%d,\"y\":%d,\"z\":%d,\"exits\":{",
                      vnum, escaped, x, y, z);
    if (n < 0 || (size_t)n >= bufsz)
        return 0;
    return append_exits(buf, bufsz, (size_t)n, exits, exit_cond);
}
