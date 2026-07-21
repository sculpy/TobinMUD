/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_OBJ_MAGIC_REPO_H
#define TOBIN_OBJ_MAGIC_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* DB access for magic items (Sneezy -> Tobin feature audit, "Magic
 * items") -- see db/sneezy/obj_magic.sql for the full rationale (why
 * this is a fresh Tobin table rather than reinterpreting messy upstream
 * val[] data). One row per scroll/wand/staff VNUM, naming the spell it
 * invokes and how many charges it starts with. */

#define OBJ_MAGIC_SPELL_NAME_LEN 64

/* Looks up `vnum` in `obj_magic`. False (leaves *out untouched) if this
 * vnum isn't a magic item at all. */
bool obj_magic_repo_get(int vnum, char *spell_name, size_t spell_name_sz, int *max_charges);

#endif
