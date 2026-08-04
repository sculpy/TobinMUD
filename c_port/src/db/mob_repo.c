/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "mob_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

#define MOB_PROTO_CACHE_BUCKETS 512

typedef struct mob_proto_cache_node {
    int vnum;
    mob_proto_t proto;
    struct mob_proto_cache_node *next;
} mob_proto_cache_node_t;

static mob_proto_cache_node_t *g_mob_proto_cache[MOB_PROTO_CACHE_BUCKETS];
static bool g_mob_proto_cache_active = false;

void mob_proto_cache_begin(void) {
    g_mob_proto_cache_active = true;
}

void mob_proto_cache_end(void) {
    for (int i = 0; i < MOB_PROTO_CACHE_BUCKETS; i++) {
        mob_proto_cache_node_t *n = g_mob_proto_cache[i];
        while (n) {
            mob_proto_cache_node_t *next = n->next;
            free(n);
            n = next;
        }
        g_mob_proto_cache[i] = NULL;
    }
    g_mob_proto_cache_active = false;
}

static const mob_proto_t *mob_proto_cache_find(int vnum) {
    for (mob_proto_cache_node_t *n = g_mob_proto_cache[(unsigned)vnum % MOB_PROTO_CACHE_BUCKETS]; n; n = n->next)
        if (n->vnum == vnum)
            return &n->proto;
    return NULL;
}

static void mob_proto_cache_put(int vnum, const mob_proto_t *p) {
    mob_proto_cache_node_t *n = malloc(sizeof(*n));
    if (!n)
        return;
    n->vnum = vnum;
    n->proto = *p;
    unsigned bucket = (unsigned)vnum % MOB_PROTO_CACHE_BUCKETS;
    n->next = g_mob_proto_cache[bucket];
    g_mob_proto_cache[bucket] = n;
}

/* Loads a mob prototype's full field set from the mob table by vnum --
 * the counterpart to mob_proto_save() below, used to populate a mob_proto_t
 * for medit and for instantiating mobs in the world. */
bool mob_proto_load(int vnum, mob_proto_t *out) {
    if (g_mob_proto_cache_active) {
        const mob_proto_t *hit = mob_proto_cache_find(vnum);
        if (hit) {
            *out = *hit;
            return true;
        }
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db,
            "select name, short_desc, long_desc, description, actions, affects, faction, "
            "fact_perc, attacks, level, tohit, ac, hpbonus, damage_level, damage_precision, "
            "gold, race, body_type, weight, height, str, bra, con, dex, agi, intel, wis, foc, per, cha, "
            "kar, spe, def_position, sex, spec_proc, skin, vision, can_be_seen, max_exist, "
            "local_sound, adjacent_sound, align, class "
            "from mob where vnum=%i", vnum)
        && db_fetch_row(db)) {
        snprintf(out->name, sizeof(out->name), "%s", db_get(db, "name"));
        snprintf(out->short_descr, sizeof(out->short_descr), "%s", db_get(db, "short_desc"));
        snprintf(out->long_descr, sizeof(out->long_descr), "%s", db_get(db, "long_desc"));
        snprintf(out->description, sizeof(out->description), "%s", db_get(db, "description"));
        out->actions = atoi(db_get(db, "actions"));
        out->affects = atoi(db_get(db, "affects"));
        out->faction = atoi(db_get(db, "faction"));
        out->fact_perc = atoi(db_get(db, "fact_perc"));
        out->attacks = atof(db_get(db, "attacks"));
        out->level = atoi(db_get(db, "level"));
        out->tohit = atoi(db_get(db, "tohit"));
        out->ac = atof(db_get(db, "ac"));
        out->hpbonus = atof(db_get(db, "hpbonus"));
        out->damage_level = atof(db_get(db, "damage_level"));
        out->damage_precision = atoi(db_get(db, "damage_precision"));
        out->gold = atoi(db_get(db, "gold"));
        out->race = atoi(db_get(db, "race"));
        out->body_type = atoi(db_get(db, "body_type"));
        out->weight = atoi(db_get(db, "weight"));
        out->height = atoi(db_get(db, "height"));
        out->str = atoi(db_get(db, "str"));
        out->bra = atoi(db_get(db, "bra"));
        out->con = atoi(db_get(db, "con"));
        out->dex = atoi(db_get(db, "dex"));
        out->agi = atoi(db_get(db, "agi"));
        out->intel = atoi(db_get(db, "intel"));
        out->wis = atoi(db_get(db, "wis"));
        out->foc = atoi(db_get(db, "foc"));
        out->per = atoi(db_get(db, "per"));
        out->cha = atoi(db_get(db, "cha"));
        out->kar = atoi(db_get(db, "kar"));
        out->spe = atoi(db_get(db, "spe"));
        out->def_position = atoi(db_get(db, "def_position"));
        out->sex = atoi(db_get(db, "sex"));
        out->spec_proc = atoi(db_get(db, "spec_proc"));
        out->skin = atoi(db_get(db, "skin"));
        out->vision = atoi(db_get(db, "vision"));
        out->can_be_seen = atoi(db_get(db, "can_be_seen"));
        out->max_exist = atoi(db_get(db, "max_exist"));
        snprintf(out->local_sound, sizeof(out->local_sound), "%s", db_get(db, "local_sound"));
        snprintf(out->adjacent_sound, sizeof(out->adjacent_sound), "%s", db_get(db, "adjacent_sound"));
        out->align = atoi(db_get(db, "align"));
        out->class_mask = atoi(db_get(db, "class"));
        found = true;
    }

    db_close(db);
    if (found && g_mob_proto_cache_active)
        mob_proto_cache_put(vnum, out);
    return found;
}

/* Writes every mob_proto_t field back in one UPDATE. Two real upstream
 * `send_mob_menu()` fields are disclosed as out of scope, not silently
 * dropped: "Immunities" (the real menu's slot 21 -- Tobin's `mob` table
 * has no immunity/resistance column at all to write to) and slot 15,
 * which the real upstream's own menu labels "unused" with no case in its
 * own dispatcher either -- genuinely not a real field there either, same
 * "not a Tobin omission" precedent oedit's own `action_desc` gap used. */
bool mob_proto_save(int vnum, const mob_proto_t *p) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "update mob set name='%s', short_desc='%s', long_desc='%s', description='%s', "
        "actions=%i, affects=%i, faction=%i, fact_perc=%i, attacks=%f, level=%i, tohit=%i, "
        "ac=%f, hpbonus=%f, damage_level=%f, damage_precision=%i, gold=%i, race=%i, "
        "body_type=%i, "
        "weight=%i, height=%i, str=%i, bra=%i, con=%i, dex=%i, agi=%i, intel=%i, wis=%i, "
        "foc=%i, per=%i, cha=%i, kar=%i, spe=%i, def_position=%i, sex=%i, spec_proc=%i, "
        "skin=%i, vision=%i, can_be_seen=%i, max_exist=%i, local_sound='%s', "
        "adjacent_sound='%s', align=%i, class=%i where vnum=%i",
        p->name, p->short_descr, p->long_descr, p->description,
        p->actions, p->affects, p->faction, p->fact_perc, p->attacks, p->level, p->tohit,
        p->ac, p->hpbonus, p->damage_level, p->damage_precision, p->gold, p->race,
        p->body_type,
        p->weight, p->height, p->str, p->bra, p->con, p->dex, p->agi, p->intel, p->wis,
        p->foc, p->per, p->cha, p->kar, p->spe, p->def_position, p->sex, p->spec_proc,
        p->skin, p->vision, p->can_be_seen, p->max_exist, p->local_sound,
        p->adjacent_sound, p->align, p->class_mask, vnum);

    db_close(db);
    return ok;
}

/* Inserts a new mob row at vnum with sane placeholder stats ("an
 * unfinished mob"), for medit's create-new-mob flow to then fill in. */
bool mob_proto_create_blank(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into mob (vnum, name, short_desc, long_desc, description, actions, "
        "affects, faction, fact_perc, letter, attacks, class, level, tohit, ac, "
        "hpbonus, damage_level, damage_precision, gold, race, weight, height, str, "
        "bra, con, dex, agi, intel, wis, foc, per, cha, kar, spe, pos, def_position, "
        "sex, spec_proc, skin, vision, can_be_seen, max_exist, align) values "
        "(%i, 'an unfinished mob', 'an unfinished mob', "
        "'An unfinished mob stands here.', '', 0, 0, 0, 0, 'A', 1.0, 0, 1, 0, 0.0, "
        "0.0, 0.0, 0, 0, 0, 0, 0, 120, 120, 120, 120, 120, 120, 120, 120, 120, 120, "
        "120, 120, 10, 10, 0, 0, 0, 0, 1, 0, 0)",
        vnum);

    db_close(db);
    return ok;
}

/* Finds the lowest vnum of any mob whose name contains the given substring
 * -- used for name-based mob lookups (e.g. "goto mob <name>") where the
 * caller doesn't know the vnum. Returns -1 if no match. */
int mob_find_vnum_by_name(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int vnum = -1;
    if (db_query(db, "select vnum from mob where name like '%%%s%%' order by vnum limit 1", name)
        && db_fetch_row(db)) {
        vnum = atoi(db_get(db, "vnum"));
    }

    db_close(db);
    return vnum;
}

/* Fetches just a mob's spec_proc id without loading the whole prototype --
 * used where a caller only needs to know which special procedure a mob
 * runs. Returns -1 if the mob doesn't exist. */
int mob_repo_get_spec_proc(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int spec_proc = -1;
    if (db_query(db, "select spec_proc from mob where vnum=%i", vnum) && db_fetch_row(db))
        spec_proc = atoi(db_get(db, "spec_proc"));

    db_close(db);
    return spec_proc;
}
