/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_LANGUAGE_H
#define TOBIN_LANGUAGE_H

#include <stddef.h>

#include "being.h"

/* Speak-a-language subsystem (Tier-4 port, 2026-08-16). SneezyMUD models
 * each tongue as a general (cross-class) skill and, when a speaker talks,
 * garbles the text for EACH listener per-word by getLanguageChance()
 * (misc/garble.cc) -- a function of the listener's learned proficiency in
 * that tongue (plus their "ear") and the speaker's Common fluency and
 * wits. This file ports that model to Tobin:
 *
 *   - Each language is a real SKILL_TIER_COMBAT entry in skill.c's roster
 *     (cross-class, like fishing/climbing), so proficiency is learned by
 *     doing -- you understand a tongue better the more you're exposed to
 *     it, and speak it better the more you use it.
 *   - Common is the lingua franca: a player's default spoken language, and
 *     speech in Common is NEVER garbled. Garbling only happens once a
 *     player `speak`s a foreign tongue.
 *   - The per-language transforms (accent substitutions, injected
 *     gibberish, syllable-breaking) are ported table-for-table from
 *     Sneezy's garble.cc.
 *
 * Disclosed simplifications vs. upstream: Tobin has no perception stat, so
 * the listener's "ear" bonus reads Wisdom instead (getLanguageChance's
 * STAT_PER term); and the transforms emit lowercase rather than restoring
 * the original word's case (Sneezy's sstring::matchCase), which reads as a
 * plain accent and avoids porting the case-matching helper. */

typedef enum {
    LANG_COMMON = 0,
    LANG_TROLLISH,
    LANG_AVIAN,
    LANG_FISHBURBLE,
    LANG_BULLYCROAK,
    LANG_GUTTER_CANT,
    LANG_GNOLL_JARGON,
    LANG_TROGLODYTE_PIDGIN,
    NUM_LANGUAGES
} language_t;

/* Display name ("Trollish", "Fish Burble", ...) for a language index. */
const char *language_name(int lang);

/* The roster skill name ("trollish", "fish burble", ...) that backs a
 * language -- i.e. the exact skill.c SKILLS[] entry whose proficiency
 * gates comprehension. Common maps to "common". */
const char *language_skill_name(int lang);

/* Case-insensitive prefix lookup of a player-typed token against the
 * language names ("troll" -> LANG_TROLLISH). Returns the language index,
 * or -1 if nothing matches. */
int language_by_name(const char *tok);

/* getLanguageChance() port: the 0-100 per-word chance that a word spoken
 * by `from` in the tongue backed by `skill_name` comes out GARBLED to
 * listener `to`. Higher = more garbled. Drops as the listener's learned
 * proficiency (and Wisdom "ear") rise and as the speaker's Common fluency
 * and Intelligence rise. Side effect: runs one learn-by-doing attempt for
 * the LISTENER (they improve by being exposed), matching Sneezy's
 * bSuccess()-drives-a-gain ordering. `to` may be NULL (returns worst-case
 * garble). */
int language_garble_chance(being_t *from, being_t *to, const char *skill_name);

/* Renders `in` as spoken by `from` in language `lang`, garbled for
 * listener `to`, into `out` (NUL-terminated, bounded by outsz). For
 * LANG_COMMON this is a straight copy. */
void language_garble(int lang, being_t *from, being_t *to, const char *in,
                     char *out, size_t outsz);

/* One learn-by-doing tick for the SPEAKER in the language they're
 * speaking -- call once per utterance so using a tongue trains it. No-op
 * for Common or for a speaker who doesn't know the skill. */
void language_speaker_practice(being_t *from, int lang);

#endif
