/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MSDP_H
#define TOBIN_MSDP_H

#include <stddef.h>

/* MSDP (TobinMUD Client project, 2026-08-05) -- unlike GMCP's JSON,
 * MSDP is a flat binary VAR/VAL framing (one control byte per
 * variable name, one per value, repeated): no TABLE_OPEN/CLOSE or
 * ARRAY_OPEN/CLOSE nesting support yet (v1 scope matches GMCP's own
 * "small fixed vocabulary, flat values only" cut -- add nesting if a
 * real variable actually needs it later). Same "known small shape,
 * hand-built rather than a general encoder" reasoning as gmcp.h.
 *
 * Every msdp_build_* function writes raw (non-printable) bytes into
 * `buf` and returns the byte count written (0 if it didn't fit) --
 * NOT a C string; pass straight to descriptor_send_subneg(), never
 * strlen()/printf() it. */

/* VAR "HEALTH" VAL "<hp>" VAR "HEALTH_MAX" VAL "<maxhp>"
 * VAR "VITALITY" VAL "<vit>" VAR "VITALITY_MAX" VAL "<maxvit>"
 * (values sent as their decimal ASCII text, MSDP's own convention --
 * there is no numeric type on the wire, everything is a string).
 * Second resource is Vitality, not MSDP's semi-conventional "MANA" --
 * Tobin's being.h progress_t has no mana stat at all, only hp/max_hp
 * and vit/max_vit (the real HP-parallel movement-cost resource), so
 * this reports what actually exists rather than lying to the client. */
size_t msdp_build_vitals(unsigned char *buf, size_t bufsz, int hp, int maxhp, int vit, int maxvit);

#endif
