/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "liquids.h"

#include <stddef.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj.h"
#include "thing.h"

/* Verbatim ordinal order + display names from liquids.cc's own
 * LIQ_WATER..LIQ_ISLA_VERDE table (indices 0-34) -- see liquids.h's own
 * comment for why the ordering can never change. `thirst`/`hunger` are
 * NOT a 1:1 copy of the original's raw numbers, though: those were tuned
 * against the original's own separate thirst/hunger scale, which Tobin's
 * vitals.c doesn't share. Rescaled here (original value * 2.5, rounded)
 * so a full 4-unit `drink` of plain water lands at exactly +100 thirst --
 * the same number cmd_drink.c's existing fountain-drink already uses
 * (DRINK_FOUNTAIN_THIRST_GAIN) -- while keeping every other liquid's
 * relative ordering intact (coffee/whisky/hard liquor are still net
 * dehydrating, milk/mead are still the most filling). The original's
 * "drunk" (alcohol) field is dropped entirely -- Tobin's vitals.c has no
 * drunkenness stat, same simplification as Money's "GOLD-COIN-ONLY, no
 * commodities" precedent. */
static const liquid_type_t LIQUID_TYPES[LIQUID_TYPE_COUNT] = {
    { "<c>water<1>",                        0,  25 },  /*  0 WATER */
    { "<o>beer<1>",                        -5,  18 },  /*  1 BEER */
    { "<W>white wine<1>",                  -3,  15 },  /*  2 WINE */
    { "<o>ale<1>",                          -8,  13 },  /*  3 ALE */
    { "<k>dark<1> <o>ale<1>",               -3,  13 },  /*  4 DARKALE */
    { "<y>whiskey<1>",                       0,   3 },  /*  5 WHISKY */
    { "<y>lemonade<1>",                      3,  20 },  /*  6 LEMONADE */
    { "<g>firebreather<1>",                  0,  -8 },  /*  7 FIREBRT */
    { "local special",                      -3,   5 },  /*  8 LOCALSPC */
    { "<G>juice<1>",                         3,  20 },  /*  9 SLIME */
    { "<W>milk<1>",                          5,  15 },  /* 10 MILK */
    { "<o>tea<1>",                          -3,  15 },  /* 11 TEA */
    { "<k>coffee<1>",                       -8,  13 },  /* 12 COFFEE */
    { "<r>blood<1>",                         5,  -3 },  /* 13 BLOOD */
    { "salt water",                          3, -13 },  /* 14 SALTWATER */
    { "<k>mead<1>",                          5,  10 },  /* 15 MEAD */
    { "vodka",                              -8,  -3 },  /* 16 VODKA */
    { "rum",                                -8,  -3 },  /* 17 RUM */
    { "<o>brandy<1>",                        3,   8 },  /* 18 BRANDY */
    { "<R>red wine<1>",                     -3,  15 },  /* 19 RED_WINE */
    { "<k>warm mead<1>",                     3,  13 },  /* 20 WARM_MEAD */
    { "champagne",                          -5,  10 },  /* 21 CHAMPAGNE */
    { "holy water",                          3,  25 },  /* 22 HOLYWATER */
    { "<R>port<1>",                         -3,  13 },  /* 23 PORT */
    { "<g>mushroom<1><o> ale<1>",           -3,  13 },  /* 24 MUSHROOM_ALE */
    { "<G>v<o>o<G>m<o>i<G>t<1>",            -3,  13 },  /* 25 VOMIT */
    { "<o>cola<1>",                          5,  13 },  /* 26 COLA */
    { "<r>strawberry margarita<1>",          5,  13 },  /* 27 STRAWBERRY_MARGARITA */
    { "<b>blue margarita<1>",                5,  13 },  /* 28 BLUE_MARGARITA */
    { "<Y>gold margarita<1>",                5,  13 },  /* 29 GOLD_MARGARITA */
    { "<r>strawberry daiquiri<1>",           5,  13 },  /* 30 STRAWBERRY_DAIQUIRI */
    { "<Y>banana daiquiri<1>",               5,  13 },  /* 31 BANANA_DAIQUIRI */
    { "<W>pina colada<1>",                   5,  13 },  /* 32 PINA_COLADA */
    { "<o>tequila sunrise<1>",               5,  13 },  /* 33 TEQUILA_SUNRISE */
    { "<g>isla verde<1>",                    5,  13 },  /* 34 ISLA_VERDE */
};

const liquid_type_t *liquid_info(int type) {
    if (type < 0 || type >= LIQUID_TYPE_COUNT)
        type = LIQUID_TYPE_DEFAULT;
    return &LIQUID_TYPES[type];
}

/* Same case-insensitive keyword-prefix matching duplicated across
 * cmd_drink.c/cmd_sip.c/cmd_object.c -- kept as its own local copy here
 * too rather than exported, same precedent those files already note. */
static bool keyword_matches(const char *keywords, const char *tok, size_t tok_len) {
    if (tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}

void liquid_bare_name(int type, char *out, size_t outsz) {
    const char *name = liquid_info(type)->name;
    size_t oi = 0;
    for (const char *p = name; *p && oi + 1 < outsz; p++) {
        if (*p == '<') {
            while (*p && *p != '>')
                p++;
            continue;
        }
        out[oi++] = *p;
    }
    out[oi] = '\0';
}

int liquid_type_from_keywords(const char *keywords) {
    /* cmd_pour.c always builds a puddle's keywords as exactly
     * "puddle pool <bare name>" -- an EXACT match against the trailing
     * "puddle pool " prefix is required (not a substring/contains check)
     * so a short bare name that happens to be a substring of a longer one
     * ("ale" inside "dark ale"/"mushroom ale") can never falsely match. */
    static const char PREFIX[] = "puddle pool ";
    size_t prefix_len = sizeof(PREFIX) - 1;
    if (strncasecmp(keywords, PREFIX, prefix_len) != 0)
        return LIQUID_TYPE_DEFAULT;
    const char *suffix = keywords + prefix_len;

    char bare[64];
    for (int i = 0; i < LIQUID_TYPE_COUNT; i++) {
        liquid_bare_name(i, bare, sizeof(bare));
        if (strcasecmp(suffix, bare) == 0)
            return i;
    }
    return LIQUID_TYPE_DEFAULT;
}

struct obj *liquid_find_carried_container(const struct being *ch, const char *tok) {
    const char *rest;
    int ordinal = thing_parse_ordinal(tok, &rest);
    size_t len = strlen(rest);
    int seen = 0;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_DRINK)
            continue;
        if (!keyword_matches(t->name, rest, len))
            continue;
        seen++;
        if (seen == ordinal)
            return o;
    }
    return NULL;
}
