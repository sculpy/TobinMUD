/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_GMCP_H
#define TOBIN_GMCP_H

#include <stddef.h>

/* GMCP (TobinMUD Client project, 2026-08-05) -- "Module.Name
 * {<json>}" text pushed inside an IAC SB GMCP ... IAC SE packet
 * (descriptor_send_subneg(), descriptor.h). v1 scope is OUTBOUND only:
 * a small, fixed vocabulary of real messages the server pushes on
 * state change, built directly with snprintf() rather than a general
 * JSON encoder -- there is no free-form data to encode generically,
 * just a couple of known shapes, so a generic encoder would be
 * unused complexity. No decoder exists yet either (nothing inbound to
 * decode -- see descriptor.c's drain_lines() comment on captured-but-
 * unused inbound subnegotiation payloads); add one if/when a real
 * client-initiated GMCP message needs handling.
 *
 * Every gmcp_build_* function writes a complete, null-terminated
 * "Module.Name {json}" string into `buf` and returns its length (or 0
 * if it didn't fit -- same "won't silently truncate JSON mid-token"
 * caution as anywhere else a fixed buffer meets untrusted-length
 * input, even though `name` here is always server-controlled). */

/* "Char.Vitals {"hp":42,"maxhp":100,"vit":10,"maxvit":20,"mana":5,"maxmana":30}" -- Tobin
 * has no mana stat (confirmed against being.h's progress_t -- no
 * such field exists), so the second resource is Vitality (`vit`/
 * `max_vit`, the real HP-parallel movement-cost resource Tobin
 * actually has), not an invented `mana` key a real client would
 * be lied to about. */
size_t gmcp_build_char_vitals(char *buf, size_t bufsz, int hp, int maxhp, int vit, int maxvit,
                               int mana, int maxmana);

/* "Room.Info {"num":942900,"name":"A bare sandbox room."}" -- `name` is
 * JSON-string-escaped (quotes/backslashes/control bytes) since it's
 * DB-sourced room-name text, not a fixed literal like the keys above. */
size_t gmcp_build_room_info(char *buf, size_t bufsz, int vnum, const char *name);

#endif
