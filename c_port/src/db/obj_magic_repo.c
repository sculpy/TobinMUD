/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
