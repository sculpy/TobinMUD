/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
#include "gmcp_json.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* Finds `"key":` in `json` and returns a pointer just past the colon
 * (skipping any whitespace), or NULL if not found. Naive substring
 * search -- fine for the small, flat, server-controlled payloads this
 * parses; would false-positive on a key name appearing inside a
 * string VALUE, which none of Tobin's current messages do. */
static const char *find_value(const char *json, const char *key) {
    char pattern[80];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    if (n < 0 || (size_t)n >= sizeof(pattern))
        return NULL;
    const char *p = strstr(json, pattern);
    if (!p)
        return NULL;
    p += n;
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}
bool gmcp_json_get_int(const char *json, const char *key, int *out) {
    const char *p = find_value(json, key);
    if (!p)
        return false;
    char *end;
    long v = strtol(p, &end, 10);
    if (end == p)
        return false;
    *out = (int)v;
    return true;
}
bool gmcp_json_get_string(const char *json, const char *key, char *out, size_t bufsz) {
    const char *p = find_value(json, key);
    if (!p || *p != '"')
        return false;
    p++;
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < bufsz) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n')
                out[o++] = '\n';
            else if (*p == 'r')
                out[o++] = '\r';
            else
                out[o++] = *p;
        } else {
            out[o++] = *p;
        }
        p++;
    }
    out[o] = '\0';
    return true;
}
bool gmcp_json_get_object(const char *json, const char *key, char *out, size_t bufsz) {
    const char *p = find_value(json, key);
    if (!p || *p != '{')
        return false;
    p++;
    const char *start = p;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '{')
            depth++;
        else if (*p == '}')
            depth--;
        if (depth > 0)
            p++;
    }
    if (depth != 0)
        return false; /* unterminated -- malformed payload */
    size_t len = (size_t)(p - start);
    if (len + 1 > bufsz)
        return false;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}
bool gmcp_json_object_iter_next(const char **cursor, char *key_out, size_t key_out_sz, int *val_out) {
    const char *p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == ',')
        p++;
    if (*p != '"')
        return false;
    p++;
    size_t o = 0;
    while (*p && *p != '"') {
        if (o + 1 < key_out_sz)
            key_out[o++] = *p;
        p++;
    }
    if (*p != '"')
        return false;
    key_out[o] = '\0';
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != ':')
        return false;
    p++;
    while (*p == ' ' || *p == '\t')
        p++;
    char *end;
    long v = strtol(p, &end, 10);
    if (end == p)
        return false;
    *val_out = (int)v;
    *cursor = end;
    return true;
}
