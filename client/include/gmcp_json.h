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
 * (there are none to handle yet). */

/* Finds an integer value for `key`. Returns false if not found. */
bool gmcp_json_get_int(const char *json, const char *key, int *out);

/* Finds a JSON string value for `key`, un-escaping \" \\ \n \r, into
 * `out` (bufsz includes the null terminator). Returns false if not
 * found. */
bool gmcp_json_get_string(const char *json, const char *key, char *out, size_t bufsz);

#endif
