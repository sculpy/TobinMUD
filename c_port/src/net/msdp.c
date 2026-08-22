/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "msdp.h"

#include <stdio.h>
#include <string.h>

/* Real assigned MSDP control-byte values (not Tobin-invented -- the
 * same ones every MSDP-aware client already expects). Only VAR/VAL are
 * used by this v1 (flat values, no TABLE/ARRAY nesting -- see msdp.h). */
enum { MSDP_VAR = 1, MSDP_VAL = 2 };

static size_t append_var(unsigned char *buf, size_t bufsz, size_t o, const char *name, int value) {
    char valstr[16];
    int vlen = snprintf(valstr, sizeof(valstr), "%d", value);
    size_t namelen = strlen(name);
    size_t need = 2 + namelen + (size_t)vlen;
    if (o + need > bufsz)
        return o; /* doesn't fit -- drop this var rather than write past bufsz */
    buf[o++] = MSDP_VAR;
    memcpy(buf + o, name, namelen);
    o += namelen;
    buf[o++] = MSDP_VAL;
    memcpy(buf + o, valstr, (size_t)vlen);
    o += (size_t)vlen;
    return o;
}

static size_t append_str_var(unsigned char *buf, size_t bufsz, size_t o, const char *name, const char *value) {
    size_t namelen = strlen(name), vlen = strlen(value);
    if (o + 2 + namelen + vlen > bufsz)
        return o;
    buf[o++] = MSDP_VAR;
    memcpy(buf + o, name, namelen); o += namelen;
    buf[o++] = MSDP_VAL;
    memcpy(buf + o, value, vlen); o += vlen;
    return o;
}

size_t msdp_build_vitals(unsigned char *buf, size_t bufsz, int hp, int maxhp, int vit, int maxvit,
                          int mana, int maxmana, const char *manalabel) {
    size_t o = 0;
    o = append_var(buf, bufsz, o, "HEALTH", hp);
    o = append_var(buf, bufsz, o, "HEALTH_MAX", maxhp);
    o = append_var(buf, bufsz, o, "VITALITY", vit);
    o = append_var(buf, bufsz, o, "VITALITY_MAX", maxvit);
    o = append_var(buf, bufsz, o, "MANA", mana);
    o = append_var(buf, bufsz, o, "MANA_MAX", maxmana);
    o = append_str_var(buf, bufsz, o, "MANA_LABEL", manalabel ? manalabel : "Mana");
    return o;
}
