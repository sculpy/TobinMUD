#ifndef TOBIN_COLORSTRING_H
#define TOBIN_COLORSTRING_H

#include <stdbool.h>
#include <stddef.h>

/* C replacement for sys/{ansi,colorstring}.{h,cc}'s colorString(). The
 * original stores raw "<X>" tags in text (room descriptions, etc.) and
 * translates them to ANSI escapes centrally, once per recipient, at the
 * single output choke-point (see descriptor_send() in net/descriptor.c).
 * This is a direct port of that syntax and that centralization, trimmed
 * to the non-immortal-only color set -- see STATUS.md for what's deferred
 * (flash/background codes, per-category toggles, DB-persisted preference).
 *
 * Tag syntax: exactly 3 bytes, '<' + one letter + '>'. Unrecognized tags
 * (including the original's immortal-only codes, not implemented here)
 * pass through unmodified rather than vanishing, so nothing silently
 * disappears until it has a real handler. */

/* Translates src into dst: recognized "<X>" tags become their ANSI escape
 * (or are stripped, if color_on is false); everything else copies through
 * unchanged. dst must be at least colorstring_translate_maxlen(strlen(src))
 * + 1 bytes. Returns the number of bytes written to dst, excluding the
 * NUL terminator (which is always written). */
size_t colorstring_translate(const char *src, char *dst, size_t dst_size, bool color_on);

/* Worst-case dst size needed (excluding the NUL) for a source string of
 * length src_len -- every 3-byte tag can expand to at most 7 ANSI bytes
 * (e.g. "<R>" -> "\033[1;31m"). */
size_t colorstring_translate_maxlen(size_t src_len);

#endif
