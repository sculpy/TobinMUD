/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MOB_REPO_H
#define TOBIN_MOB_REPO_H

#include <stdbool.h>

#include "being.h"

/* DB access for mob prototypes: the upstream-seeded `mob` table
 * (db/sneezy/mob.sql) is read directly -- no new Tobin table needed, same
 * "prototypes already exist" precedent as obj_repo.h. Only the fields
 * Tobin's simplified mob model actually uses are loaded; the rest of the
 * mob table's ~40 columns (class/race/tohit/ac/damage_level/actions/
 * affects/spec_proc/...) are explicitly deferred to a future edmobile/AI
 * session -- see STATUS.md's Mobiles decision row.
 *
 * Unlike objects, there is no mob-instance persistence table: a mob has no
 * owning player to persist against, and there's no zone-reset system yet
 * (2E, still future) to respawn one at boot -- an `mload`ed mob is lost on
 * a server restart, same documented gap as room-floor `oload`ed objects. */

typedef struct {
    char name[64];              /* matches thing_t.name */
    char short_descr[128];      /* matches thing_t.short_descr */
    char description[BEING_APPEARANCE_LEN]; /* the "look <mob>" closer text */
    int level;
    double hpbonus;             /* the original's real HP-scaling parameter */
    int sex;
    int actions;                /* original's ACT_* bitmask (misc/defs.h) --
                                  * verbatim upstream bit layout, e.g.
                                  * ACT_SENTINEL=2, ACT_SCAVENGER=4,
                                  * ACT_STAY_ZONE=64. First field of this
                                  * struct actually read for mob AI
                                  * (mob_ai.c) rather than just carried
                                  * inertly -- see that file's doc comment. */
    int align;                  /* Tobin-added `mob.align` column (NOT the
                                  * upstream mob.class/mob.race columns this
                                  * struct otherwise defers) -- -1 evil, 0
                                  * unaligned, 1 good. See mob_ai.c's
                                  * mob_try_aggress() (user 2026-07-11:
                                  * "good will attack evil and evil will
                                  * attack good randomly ... neutral should
                                  * be taunted by evil and supported by
                                  * good"). */
    int class_mask;              /* Upstream `mob.class` -- a BITMASK (1
                                   * mage, 2 cleric, 4 warrior, 8 thief, 16
                                   * shaman, 32 deikhan, 64 monk, 128
                                   * ranger, 256 other), confirmed against
                                   * the seeded guildmaster mobs (vnum
                                   * 200-229). No longer wholly deferred as
                                   * of user 2026-07-12's practice/
                                   * guildmaster request -- being_create_mob()
                                   * maps the recognizable single-class bits
                                   * to a Tobin player_class_t (see
                                   * being.c). race/tohit/ac/damage_level/
                                   * actions(non-AI-relevant bits)/spec_proc
                                   * remain deferred. */
} mob_proto_t;

/* Loads the prototype row for `vnum` from the `mob` table into *out.
 * Returns false if no such vnum exists. */
bool mob_proto_load(int vnum, mob_proto_t *out);

/* Finds the lowest vnum whose `name` column contains `name` (case-
 * insensitive substring, e.g. "demon" matches "a vrock demon"), or -1 if
 * none match. Backs `mload <name>` (cmd_mload.c) as an alternative to a
 * bare vnum. */
int mob_find_vnum_by_name(const char *name);

#endif
