/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "language.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "skill.h"

/* Work-buffer ceiling for a single garbled message. Generous headroom
 * over cmd_say.c's 336-char message cap: several transforms EXPAND the
 * text (birdtalk/fishtalk inject whole squawk words; gutter's phonetic
 * rules lengthen many words), so the intermediate can run well past the
 * input length. */
#define LANG_BUF 2048

typedef void (*garble_fn)(int chance, const char *in, char *out, size_t outsz);

/* ------------------------------------------------------------------ *
 * small string helpers (C stand-ins for Sneezy's sstring methods)
 * ------------------------------------------------------------------ */

/* rand 0..100 inclusive -- the `number(0, 100)` the garbles roll against. */
static int roll100(void) { return rand() % 101; }

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Lowercase copy of `in` into `out` (bounded). The transforms all work in
 * lowercase (see language.h's disclosed-simplification note). */
static void to_lower_buf(const char *in, char *out, size_t outsz) {
    size_t i = 0;
    for (; in[i] && i + 1 < outsz; i++)
        out[i] = (char)tolower((unsigned char)in[i]);
    out[i] = '\0';
}

/* Replace every occurrence of `from` with `to` in `buf` (bounded by
 * bufsz), the equivalent of sstring::inlineReplaceString. Non-overlapping,
 * left to right; the replacement text is NOT re-scanned. */
static void replace_all(char *buf, size_t bufsz, const char *from,
                        const char *to) {
    size_t flen = strlen(from);
    if (!flen)
        return;
    size_t tlen = strlen(to);
    char tmp[LANG_BUF];
    size_t w = 0;
    const char *p = buf;
    while (*p) {
        if (strncmp(p, from, flen) == 0) {
            if (w + tlen >= sizeof(tmp))
                break;
            memcpy(tmp + w, to, tlen);
            w += tlen;
            p += flen;
        } else {
            if (w + 1 >= sizeof(tmp))
                break;
            tmp[w++] = *p++;
        }
    }
    tmp[w] = '\0';
    strncpy(buf, tmp, bufsz - 1);
    buf[bufsz - 1] = '\0';
}

/* Strip one leading and one trailing space (the transforms pad the string
 * with spaces so " word"/"word " boundary rules can fire, then trim). */
static void trim_pad(char *buf) {
    size_t len = strlen(buf);
    if (len && buf[len - 1] == ' ')
        buf[len - 1] = '\0';
    if (buf[0] == ' ')
        memmove(buf, buf + 1, strlen(buf));
}

static int is_vowel(char c) {
    c = (char)tolower((unsigned char)c);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

/* ------------------------------------------------------------------ *
 * per-language transforms (ports of misc/garble.cc's tables)
 * ------------------------------------------------------------------ */

/* Whole-string replace-table languages: lowercase, pad with spaces, then
 * apply each rule either always (indices < always_n) or with probability
 * `chance` (indices >= always_n). Ported from garble_trolltalk /
 * garble_frogtalk / garble_gutter. */
static void garble_table(int chance, const char *in, char *out, size_t outsz,
                         const char rep[][2][12], int nrep, int always_n) {
    char buf[LANG_BUF];
    buf[0] = ' ';
    to_lower_buf(in, buf + 1, sizeof(buf) - 2);
    size_t len = strlen(buf);
    if (len + 1 < sizeof(buf)) {
        buf[len] = ' ';
        buf[len + 1] = '\0';
    }
    for (int i = 0; i < nrep; i++) {
        if (i >= always_n && !(chance > roll100()))
            continue;
        replace_all(buf, sizeof(buf), rep[i][0], rep[i][1]);
    }
    trim_pad(buf);
    strncpy(out, buf, outsz - 1);
    out[outsz - 1] = '\0';
}

/* Trollish -- gutteral consonant swaps (garble_trolltalk). */
static void garble_trollish(int chance, const char *in, char *out,
                            size_t outsz) {
    static const char rep[][2][12] = {
        {"ph", "'"},   {"h", "g'"},   {"z", "kz"},   {"qu", "kw"},
        {"q", "k"},    {"ck", "k"},   {"c", "k"},    {" s", " ch"},
        {"ss", "auch"},{"es ", "'k "},{"s ", "'k "}, {"s", "'"},
        {"fr", "r"},   {"fl", "l"},   {"of", "uv"},  {"f", "'"},
        {"'''", "'"},  {"''", "'"},
    };
    garble_table(chance, in, out, outsz, rep,
                 (int)(sizeof(rep) / sizeof(rep[0])), 0);
}

/* Bullycroak -- frogman soft-palate swaps (garble_frogtalk). */
static void garble_bullycroak(int chance, const char *in, char *out,
                              size_t outsz) {
    static const char rep[][2][12] = {
        {"ll", "y"},  {"l", "y"},   {"rr", "y"},  {"r", "y"},
        {"cc", "k"},  {"ck", "k"},  {"ch", "sh"}, {"c", "s"},
        {"st", "ss"}, {"ts", "ss"}, {"tt", "sh"}, {"th", "ph"},
        {"t", "s"},
    };
    garble_table(chance, in, out, outsz, rep,
                 (int)(sizeof(rep) / sizeof(rep[0])), 0);
}

/* Gutter cant -- streetwise slur (garble_gutter). First 10 rules always
 * fire; the rest are chance-gated. (Sneezy uses a slash-star guard token
 * to keep an "s'" from being re-slurred; that quirk is dropped here as an
 * immaterial cosmetic difference.) */
static void garble_gutter(int chance, const char *in, char *out,
                          size_t outsz) {
    static const char rep[][2][12] = {
        {" this ", " dis "},  {" that", " dats"},   {" their ", " deys "},
        {" theirs ", " deys "},{" they", " deys"},  {" them", " dems"},
        {" the ", " dah "},   {" there ", " ders "},{" are ", " is "},
        {"yre ", "'s "},      {"'re ", "'s "},      {"'nt ", "n' "},
        {" wha", " who"},     {" th", " f"},        {"th ", "f "},
        {"th", "v"},          {"nd", "n'"},         {" h", " '"},
        {"ing ", "in' "},     {"ool ", "oo' "},     {"oll ", "oe "},
        {"ol ", "o' "},       {"al ", "ow "},       {"ill ", "iw "},
        {"il ", "ew "},       {"le ", "ow "},       {"tt", "h'"},
        {"te ", "' "},        {"it ", "ih' "},      {"ot ", "oh' "},
        {"at ", "ah' "},      {"et ", "eh' "},      {"er ", "ah "},
        {"'''", "'"},
    };
    garble_table(chance, in, out, outsz, rep,
                 (int)(sizeof(rep) / sizeof(rep[0])), 10);
}

/* Word-by-word replace, applied to a single word (used by birdtalk /
 * fishtalk / gnoll). */
static void word_replace(char *word, size_t wsz, const char rep[][2][12],
                         int nrep) {
    for (int i = 0; i < nrep; i++)
        replace_all(word, wsz, rep[i][0], rep[i][1]);
}

/* Append `s` to out[] at *w (bounded); returns nothing, advances *w. */
static void append(char *out, size_t outsz, size_t *w, const char *s) {
    while (*s && *w + 1 < outsz)
        out[(*w)++] = *s++;
    out[*w] = '\0';
}

/* Avian -- clipped, squawking birdspeech (garble_birdtalk): per word, a
 * chance to prepend a squawk-word, and a chance to apply the consonant
 * clip rules. */
static void garble_avian(int chance, const char *in, char *out, size_t outsz) {
    static const char *pre[] = {"mwr", "bwr", "wr", "buk", "pwr", "squ"};
    static const char *suf[] = {"awk", "aah", "awr", "awrk", "ak"};
    static const char rep[][2][12] = {
        {"gh", "k"},  {"ww", "wr"}, {"th", "t'"}, {"sh", "s'"},
        {"st", "'t"}, {"''", "'"},  {"'s", "'"},  {"ts", "t'"},
        {"sp", "s'"}, {"ng", "'"},  {"wh", "w'"}, {"'''", "'"},
        {"''", "'"},
    };
    char low[LANG_BUF];
    to_lower_buf(in, low, sizeof(low));
    char work[LANG_BUF];
    strncpy(work, low, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    size_t w = 0;
    out[0] = '\0';
    char *save = NULL;
    for (char *word = strtok_r(work, " ", &save); word;
         word = strtok_r(NULL, " ", &save)) {
        if (w)
            append(out, outsz, &w, " ");
        if (chance / 4 > roll100()) {
            char sq[24];
            snprintf(sq, sizeof(sq), "%s%s",
                     pre[rand() % (int)(sizeof(pre) / sizeof(pre[0]))],
                     suf[rand() % (int)(sizeof(suf) / sizeof(suf[0]))]);
            append(out, outsz, &w, sq);
            append(out, outsz, &w, " ");
        }
        char wb[256];
        strncpy(wb, word, sizeof(wb) - 1);
        wb[sizeof(wb) - 1] = '\0';
        if (chance > roll100())
            word_replace(wb, sizeof(wb), rep,
                         (int)(sizeof(rep) / sizeof(rep[0])));
        append(out, outsz, &w, wb);
    }
}

/* Fish burble -- watery gurgles (garble_fishtalk): per word, a chance to
 * inject a bubble-word, and a chance to apply the aquatic consonant
 * swaps. rep given in lowercase (Tobin lowercases the message first). */
static void garble_fishburble(int chance, const char *in, char *out,
                              size_t outsz) {
    static const char *watery[] = {
        "glug",   "glub",   "gug",     "glurg",  "gurgle",
        "guggle", "glubble","glurble", "blug",   "bluggle",
        "blurgle","rgrle",  "rglurg",  "aahhrrgl","glglrraa",
        "bloop",  "gloop",  "goop",    "grloop",
    };
    static const char rep[][2][12] = {
        {"r", "rr"}, {"g", "gr"}, {"l", "gl"}, {"th", "wr"}, {"t", "w"},
    };
    char low[LANG_BUF];
    to_lower_buf(in, low, sizeof(low));
    char work[LANG_BUF];
    strncpy(work, low, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    size_t w = 0;
    out[0] = '\0';
    char *save = NULL;
    for (char *word = strtok_r(work, " ", &save); word;
         word = strtok_r(NULL, " ", &save)) {
        if (w)
            append(out, outsz, &w, " ");
        if (chance / 4 > roll100()) {
            append(out, outsz, &w,
                   watery[rand() % (int)(sizeof(watery) / sizeof(watery[0]))]);
            append(out, outsz, &w, " ");
        }
        char wb[256];
        strncpy(wb, word, sizeof(wb) - 1);
        wb[sizeof(wb) - 1] = '\0';
        /* th before t, so apply in table order. */
        if (chance > roll100())
            word_replace(wb, sizeof(wb), rep,
                         (int)(sizeof(rep) / sizeof(rep[0])));
        append(out, outsz, &w, wb);
    }
}

/* Gnoll jargon -- lolspeak-style munge (garble_gnoll): per word, a chance
 * to apply the (large) lolcats replace table to a space-padded copy of
 * the word. Punctuation is protected by " *,* "-style guard tokens that
 * are stripped at the end. */
static void garble_gnoll(int chance, const char *in, char *out, size_t outsz) {
    static const char rep[][2][12] = {
        {",", " *,* "}, {";", " *;* "}, {".", " *.* "}, {"!", " *!* "},
        {"?", " *?* "},
        {" am ", " iz "}, {" are ", " iz "}, {" is ", " iz "},
        {" you ", " u "}, {" your ", " ur "}, {" have ", " haz "},
        {" my ", " mai "},{" the ", " da "}, {" this ", " diz "},
        {" that ", " dat "},{" them ", " dem "},{" dont ", " dun "},
        {" myself ", " me "},
        {"tion ", "shun "},{"tial ", "shal "},{"ight ", "ite "},
        {"ever ", "evar "},{"pped ", "opt "}, {"ude ", "ewd "},
        {"ith ", "if "},   {"ies ", "eez "},  {"ood ", "ud "},
        {"ger ", "guh "},  {"ove ", "uv "},   {"cks ", "x "},
        {"ear ", "eer "},  {"ets ", "itz "},  {"ing ", "in "},
        {"ty ", "teh "},   {" qu", " kw"},    {"ow ", "ao "},
        {"en ", "eh "},    {"es ", "z "},     {"ck ", "kk "},
        {"le ", "el "},    {"ed ", "d "},     {"x ", "ks "},
        {"s ", "z "},      {"c", "k"},        {"wr", "r"},
        {" *,* ", ","},    {" *;* ", ";"},    {" *.* ", "."},
        {" *!* ", "!"},    {" *?* ", "?"},
    };
    int nrep = (int)(sizeof(rep) / sizeof(rep[0]));
    char low[LANG_BUF];
    to_lower_buf(in, low, sizeof(low));
    char work[LANG_BUF];
    strncpy(work, low, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    size_t w = 0;
    out[0] = '\0';
    char *save = NULL;
    for (char *word = strtok_r(work, " ", &save); word;
         word = strtok_r(NULL, " ", &save)) {
        if (w)
            append(out, outsz, &w, " ");
        if (chance > roll100()) {
            char munged[300];
            snprintf(munged, sizeof(munged), " %s ", word);
            word_replace(munged, sizeof(munged), rep, nrep);
            trim_pad(munged);
            append(out, outsz, &w, munged);
        } else {
            append(out, outsz, &w, word);
        }
    }
}

/* Troglodyte pidgin -- broken, syllable-hyphenated phonics
 * (garble_trogtalk): per word, walk from the end inserting '-' at each
 * consonant->vowel->consonant boundary. */
static void garble_troglodyte(int chance, const char *in, char *out,
                              size_t outsz) {
    char low[LANG_BUF];
    to_lower_buf(in, low, sizeof(low));
    char work[LANG_BUF];
    strncpy(work, low, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    size_t w = 0;
    out[0] = '\0';
    char *save = NULL;
    for (char *word = strtok_r(work, " ", &save); word;
         word = strtok_r(NULL, " ", &save)) {
        if (w)
            append(out, outsz, &w, " ");
        char wb[256];
        strncpy(wb, word, sizeof(wb) - 1);
        wb[sizeof(wb) - 1] = '\0';

        if (chance > roll100() && strlen(wb) > 3) {
            int end = (int)strlen(wb) - 1;
            int state = 0; /* 0 seek, 2 saw-consonant, 3 saw-vowel */
            while (end > 2) {
                if (state == 0 && !is_vowel(wb[end]))
                    state = 2;
                else if (state == 0 && is_vowel(wb[end]))
                    state = 3;
                else if (state == 2 && is_vowel(wb[end]))
                    state = 3;
                else if (state == 3 && !is_vowel(wb[end])) {
                    /* insert '-' after position `end` */
                    memmove(wb + end + 1, wb + end, strlen(wb + end) + 1);
                    wb[end] = '-';
                    end--;
                    state = 2;
                }
                end--;
            }
        }
        append(out, outsz, &w, wb);
    }
}

/* ------------------------------------------------------------------ *
 * language table + public API
 * ------------------------------------------------------------------ */

typedef struct {
    const char *name;  /* display */
    const char *skill; /* roster skill name */
    garble_fn fn;      /* NULL = never garbled (Common) */
} lang_def_t;

static const lang_def_t LANGS[NUM_LANGUAGES] = {
    {"Common", "common", NULL},
    {"Trollish", "trollish", garble_trollish},
    {"Avian", "avian", garble_avian},
    {"Fish Burble", "fish burble", garble_fishburble},
    {"Bullycroak", "bullycroak", garble_bullycroak},
    {"Gutter Cant", "gutter cant", garble_gutter},
    {"Gnoll Jargon", "gnoll jargon", garble_gnoll},
    {"Troglodyte Pidgin", "troglodyte pidgin", garble_troglodyte},
};

const char *language_name(int lang) {
    if (lang < 0 || lang >= NUM_LANGUAGES)
        return "Common";
    return LANGS[lang].name;
}

const char *language_skill_name(int lang) {
    if (lang < 0 || lang >= NUM_LANGUAGES)
        return "common";
    return LANGS[lang].skill;
}

int language_by_name(const char *tok) {
    if (!tok || !*tok)
        return -1;
    size_t len = strlen(tok);
    for (int i = 0; i < NUM_LANGUAGES; i++)
        if (strncasecmp(tok, LANGS[i].name, len) == 0)
            return i;
    return -1;
}

/* getLanguageChance() port -- see language.h. */
int language_garble_chance(being_t *from, being_t *to, const char *skill_name) {
    int chance = 0;

    if (to) {
        int learning = 0;
        if (being_is_immortal(to)) {
            learning = 100;
        } else {
            const skill_def_t *sk = skill_find(to->char_class, skill_name, false);
            if (sk && being_knows_skill(to, skill_name))
                learning = skill_learn_from_doing(to, sk); /* exposure trains the ear */
        }
        if (learning > 0 && skill_roll_success(learning))
            chance = learning * 9 / 10;
        /* Listener's "ear": Wisdom stands in for Sneezy's perception. */
        chance += clampi(8 + (to->attrs.wisdom - ATTR_BASE) / 4, 0, 16);
    }

    if (from) {
        int common = 0;
        if (being_is_immortal(from)) {
            common = 100;
        } else if (being_knows_skill(from, "common")) {
            const skill_def_t *ck = skill_find(from->char_class, "common", false);
            common = ck ? skill_proficiency(from, ck) : 0;
        }
        if (common > 0 && skill_roll_success(common))
            chance += common * 4 / 5;
        chance += clampi((from->attrs.intelligence - ATTR_BASE) / 3, -10, 10);
    }

    return clampi(100 - chance, 0, 100);
}

void language_garble(int lang, being_t *from, being_t *to, const char *in,
                     char *out, size_t outsz) {
    if (lang <= LANG_COMMON || lang >= NUM_LANGUAGES || !LANGS[lang].fn) {
        strncpy(out, in, outsz - 1);
        out[outsz - 1] = '\0';
        return;
    }
    int chance = language_garble_chance(from, to, LANGS[lang].skill);
    LANGS[lang].fn(chance, in, out, outsz);
}

void language_speaker_practice(being_t *from, int lang) {
    if (!from || lang <= LANG_COMMON || lang >= NUM_LANGUAGES)
        return;
    if (being_is_immortal(from))
        return;
    const char *skill_name = LANGS[lang].skill;
    if (!being_knows_skill(from, skill_name))
        return;
    const skill_def_t *sk = skill_find(from->char_class, skill_name, false);
    if (sk)
        (void)skill_learn_from_doing(from, sk);
}
