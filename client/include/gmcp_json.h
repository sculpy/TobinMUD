/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
#ifndef TOBINCLIENT_GMCP_JSON_H
#define TOBINCLIENT_GMCP_JSON_H
#include <stdbool.h>
#include <stddef.h>
/* Minimal field extractor for the FLAT (single-level, no nesting) JSON
 * objects the server's own gmcp.c actually sends (Char.Vitals,
 * Room.Info) -- not a general JSON parser, matching gmcp.c's own "small
 * fixed vocabulary" scope. Scans for `"key":value` textually; safe
 * against extra whitespace but not against nested objects/arrays
 * appearing elsewhere in the payload with the same key name (there are
 * none to handle yet). */
/* Finds an integer value for `key`. Returns false if not found. */
bool gmcp_json_get_int(const char *json, const char *key, int *out);
/* Finds a JSON string value for `key`, un-escaping \" \\ \n \r, into
 * `out` (bufsz includes the null terminator). Returns false if not
 * found. */
bool gmcp_json_get_string(const char *json, const char *key, char *out, size_t bufsz);
/* Finds the one-level-nested object value for `key` (e.g. Room.Info's
 * `"exits":{"north":942901,"east":942902}`) and copies its RAW,
 * unparsed contents -- everything between the matching `{`/`}`, braces
 * excluded -- into `out`. Still no general nesting support: the copied
 * contents themselves must be flat key:int pairs, which is all
 * gmcp_json_object_iter_next() below understands. Returns false if the
 * key isn't found, isn't followed by `{`, or doesn't fit `out`. */
bool gmcp_json_get_object(const char *json, const char *key, char *out, size_t bufsz);
/* Iterates flat `"key":123` pairs out of a raw object string previously
 * extracted by gmcp_json_get_object() (e.g. an exits object). Pass
 * `*cursor = obj` to start; each call advances `*cursor` past the pair
 * it just read and returns true, writing the key into `key_out` and
 * the int value into `val_out`. Returns false once exhausted (or on a
 * malformed pair, which just ends iteration early rather than looping
 * forever -- the source is always this client's own map.dat/the
 * server's own GMCP push, never arbitrary untrusted input). */
bool gmcp_json_object_iter_next(const char **cursor, char *key_out, size_t key_out_sz, int *val_out);
#endif
