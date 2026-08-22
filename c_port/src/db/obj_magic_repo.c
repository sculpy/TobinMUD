/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "obj_magic_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

/* Looks up a wand/staff-type object's associated spell name and max charge
 * count by vnum. Returns false if the object has no obj_magic row. */
bool obj_magic_repo_get(int vnum, char *spell_name, size_t spell_name_sz, int *max_charges) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select spell_name, max_charges from obj_magic where vnum=%i", vnum)
        && db_fetch_row(db)) {
        snprintf(spell_name, spell_name_sz, "%s", db_get(db, "spell_name"));
        *max_charges = atoi(db_get(db, "max_charges"));
        found = true;
    }

    db_close(db);
    return found;
}

/* `scribe` (Mage, level 25, level-25 audit batch) -- reverse lookup:
 * given a spell name, finds a real seeded SCROLL (obj.type=2) prototype
 * vnum bound to it in obj_magic, if one exists. Only scrolls (not wands/
 * staves) count -- "writes a spell onto a scroll" is scroll-specific by
 * definition. Tobin-original mechanic (no real SPELL_SCRIBE exists in
 * the upstream source to port), reusing the existing obj_magic table
 * rather than inventing a new one. */
bool obj_magic_repo_find_scroll_for_spell(const char *spell_name, int *vnum_out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select om.vnum from obj_magic om join obj o on o.vnum=om.vnum "
                 "where om.spell_name='%s' and o.type=2 limit 1", spell_name)
        && db_fetch_row(db)) {
        *vnum_out = atoi(db_get(db, "vnum"));
        found = true;
    }

    db_close(db);
    return found;
}
