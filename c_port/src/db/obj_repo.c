/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "obj_repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "db.h"
#include "log.h"
#include "thing.h"

#define OBJ_PROTO_CACHE_BUCKETS 512

typedef struct obj_proto_cache_node {
    int vnum;
    obj_proto_t proto;
    struct obj_proto_cache_node *next;
} obj_proto_cache_node_t;

static obj_proto_cache_node_t *g_obj_proto_cache[OBJ_PROTO_CACHE_BUCKETS];
static bool g_obj_proto_cache_active = false;

void obj_proto_cache_begin(void) {
    g_obj_proto_cache_active = true;
}

void obj_proto_cache_end(void) {
    for (int i = 0; i < OBJ_PROTO_CACHE_BUCKETS; i++) {
        obj_proto_cache_node_t *n = g_obj_proto_cache[i];
        while (n) {
            obj_proto_cache_node_t *next = n->next;
            free(n);
            n = next;
        }
        g_obj_proto_cache[i] = NULL;
    }
    g_obj_proto_cache_active = false;
}

static const obj_proto_t *obj_proto_cache_find(int vnum) {
    for (obj_proto_cache_node_t *n = g_obj_proto_cache[(unsigned)vnum % OBJ_PROTO_CACHE_BUCKETS]; n; n = n->next)
        if (n->vnum == vnum)
            return &n->proto;
    return NULL;
}

static void obj_proto_cache_put(int vnum, const obj_proto_t *p) {
    obj_proto_cache_node_t *n = malloc(sizeof(*n));
    if (!n)
        return;
    n->vnum = vnum;
    n->proto = *p;
    unsigned bucket = (unsigned)vnum % OBJ_PROTO_CACHE_BUCKETS;
    n->next = g_obj_proto_cache[bucket];
    g_obj_proto_cache[bucket] = n;
}

/* Loads an object prototype's full field set from the obj table by vnum --
 * the counterpart to obj_proto_save() below, used to populate an
 * obj_proto_t for oedit and for instantiating objects in the world. */
bool obj_proto_load(int vnum, obj_proto_t *out) {
    if (g_obj_proto_cache_active) {
        const obj_proto_t *hit = obj_proto_cache_find(vnum);
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
            "select name, short_desc, long_desc, type, action_flag, anti_race_flag, "
            "wear_flag, val0, val1, val2, val3, weight, price, can_be_seen, max_struct, "
            "cur_struct, volume, material, max_exist, decay, spec_proc "
            "from obj where vnum=%i",
            vnum)
        && db_fetch_row(db)) {
        out->vnum = vnum;
        snprintf(out->name, sizeof(out->name), "%s", db_get(db, "name"));
        snprintf(out->short_descr, sizeof(out->short_descr), "%s", db_get(db, "short_desc"));
        snprintf(out->long_descr, sizeof(out->long_descr), "%s", db_get(db, "long_desc"));
        out->type = atoi(db_get(db, "type"));
        out->action_flag = atoi(db_get(db, "action_flag"));
        out->anti_race_flag = atoi(db_get(db, "anti_race_flag"));
        out->wear_flag = atoi(db_get(db, "wear_flag"));
        out->val[0] = atoi(db_get(db, "val0"));
        out->val[1] = atoi(db_get(db, "val1"));
        out->val[2] = atoi(db_get(db, "val2"));
        out->val[3] = atoi(db_get(db, "val3"));
        out->weight = atof(db_get(db, "weight"));
        out->price = atoi(db_get(db, "price"));
        out->can_be_seen = atoi(db_get(db, "can_be_seen")) != 0;
        out->max_struct = atoi(db_get(db, "max_struct"));
        out->cur_struct = atoi(db_get(db, "cur_struct"));
        out->volume = atoi(db_get(db, "volume"));
        out->material = atoi(db_get(db, "material"));
        out->max_exist = atoi(db_get(db, "max_exist"));
        out->decay_time = atoi(db_get(db, "decay"));
        out->spec_proc = atoi(db_get(db, "spec_proc"));
        found = true;
    }

    db_close(db);
    if (found && g_obj_proto_cache_active)
        obj_proto_cache_put(vnum, out);
    return found;
}

/* Writes every obj_proto_t field back in one UPDATE, keyed by p->vnum --
 * how oedit persists changes to an object prototype. */
bool obj_proto_save(const obj_proto_t *p) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "update obj set name='%s', short_desc='%s', long_desc='%s', type=%i, "
        "action_flag=%i, anti_race_flag=%i, wear_flag=%i, val0=%i, val1=%i, val2=%i, val3=%i, "
        "weight=%f, price=%i, can_be_seen=%i, max_struct=%i, cur_struct=%i, "
        "volume=%i, material=%i, max_exist=%i, decay=%i, spec_proc=%i "
        "where vnum=%i",
        p->name, p->short_descr, p->long_descr, p->type,
        p->action_flag, p->anti_race_flag, p->wear_flag, p->val[0], p->val[1], p->val[2], p->val[3],
        p->weight, p->price, p->can_be_seen ? 1 : 0, p->max_struct, p->cur_struct,
        p->volume, p->material, p->max_exist, p->decay_time, p->spec_proc,
        p->vnum);
    db_close(db);
    return ok;
}

/* Inserts a brand-new, minimal `obj` row at `vnum` -- 2026-07-25, user:
 * "if one doesn't exist a blank one should be created" (edit mob 43 on a
 * missing vnum), then "objects and rooms should behave the same". Every
 * numeric column left off here has a real `0`/-`1` DB-level default
 * already (see the schema); only the four string columns genuinely need
 * an explicit value. Returns false if `vnum` already exists (a real
 * PRIMARY KEY collision, surfaced by the caller as a save/DB-error
 * message) or on any other DB error. */
bool obj_proto_create_blank(int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into obj (vnum, name, short_desc, long_desc, action_desc) "
        "values (%i, 'an unfinished object', 'an unfinished object', "
        "'An unfinished object is lying here.', '')",
        vnum);

    db_close(db);
    return ok;
}

/* Finds the lowest vnum of any object whose name contains the given
 * substring -- used for name-based object lookups where the caller
 * doesn't know the vnum. Returns -1 if no match. */
int obj_find_vnum_by_name(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int vnum = -1;
    if (db_query(db, "select vnum from obj where name like '%%%s%%' order by vnum limit 1", name)
        && db_fetch_row(db)) {
        vnum = atoi(db_get(db, "vnum"));
    }

    db_close(db);
    return vnum;
}

/* Sums an object's hitroll/damroll bonuses from its objaffect rows
 * (APPLY_HITROLL/APPLY_DAMROLL/APPLY_HITNDAM), for callers that just need
 * combat mods without loading full stat affects (see
 * obj_load_stat_affects() below for the broader version). */
void obj_load_combat_mods(int vnum, int *hitroll, int *damroll) {
    *hitroll = 0;
    *damroll = 0;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    if (db_query(db, "select type, mod1 from objaffect where vnum=%i and type in (15, 16, 17)", vnum)) {
        while (db_fetch_row(db)) {
            int type = atoi(db_get(db, "type"));
            int mod1 = atoi(db_get(db, "mod1"));
            if (type == 15 || type == 17) /* APPLY_HITROLL, APPLY_HITNDAM */
                *hitroll += mod1;
            if (type == 16 || type == 17) /* APPLY_DAMROLL, APPLY_HITNDAM */
                *damroll += mod1;
        }
    }

    db_close(db);
}

void obj_load_stat_affects(int vnum, int *str, int *dex, int *con, int *intel,
                           int *wis, int *cha, int *hit, int *move, int *ac) {
    *str = *dex = *con = *intel = *wis = *cha = *hit = *move = *ac = 0;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    if (db_query(db, "select type, mod1 from objaffect where vnum=%i "
                     "and type in (1, 2, 3, 4, 5, 31, 11, 12, 14)", vnum)) {
        while (db_fetch_row(db)) {
            int type = atoi(db_get(db, "type"));
            int mod1 = atoi(db_get(db, "mod1"));
            switch (type) {
                case 1:  *str += mod1; break;  /* APPLY_STR */
                case 2:  *intel += mod1; break; /* APPLY_INT */
                case 3:  *wis += mod1; break;   /* APPLY_WIS */
                case 4:  *dex += mod1; break;   /* APPLY_DEX */
                case 5:  *con += mod1; break;   /* APPLY_CON */
                case 31: *cha += mod1; break;   /* APPLY_CHA */
                case 12: *hit += mod1; break;   /* APPLY_HIT (max_hp) */
                case 14: *move += mod1; break;  /* APPLY_MOVE (max_vit) */
                case 11: *ac += -mod1; break;   /* APPLY_ARMOR -- sign-
                                                  * flipped, see obj_t's
                                                  * own doc comment */
            }
        }
    }

    db_close(db);
}

/* Which player_inventory `slot` a currently-attached instance `o` occupies
 * on `b` -- the inverse of player_inventory_load()'s placement below. */
static int slot_for_obj(const being_t *b, const obj_t *o) {
    if (b->held[0] == o)
        return INV_SLOT_HELD_PRIMARY;
    if (b->held[1] == o)
        return INV_SLOT_HELD_OFFHAND;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (b->equipment[i] == o)
            return i;
    return INV_SLOT_CARRIED;
}

/* Rebuilds a player's held/equipped/carried objects from their saved
 * player_inventory rows: spawns each object from its prototype, restores
 * per-instance state (cur_struct, depreciation, monogram), and places it
 * into the right slot on b. Missing/removed vnums are skipped with a log
 * rather than failing the whole load. */
void player_inventory_load(long player_id, being_t *b) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    if (db_query(db, "select vnum, slot, cur_struct, depreciation, monogram "
                      "from player_inventory where player_id=%i order by id",
                 (int)player_id)) {
        /* Collect rows first -- obj_create_from_proto() below opens its own
         * connection via obj_proto_load(), and this connection must stay
         * open across db_fetch_row() calls. */
        typedef struct {
            int vnum, slot;
            bool has_cur_struct;
            int cur_struct, depreciation;
            char monogram[64];
        } row_t;
        row_t rows[256];
        int n = 0;
        while (n < 256 && db_fetch_row(db)) {
            rows[n].vnum = atoi(db_get(db, "vnum"));
            rows[n].slot = atoi(db_get(db, "slot"));
            const char *cs = db_get(db, "cur_struct");
            rows[n].has_cur_struct = cs && *cs;
            rows[n].cur_struct = rows[n].has_cur_struct ? atoi(cs) : 0;
            rows[n].depreciation = atoi(db_get(db, "depreciation"));
            snprintf(rows[n].monogram, sizeof(rows[n].monogram), "%s", db_get(db, "monogram"));
            n++;
        }
        db_close(db);

        for (int i = 0; i < n; i++) {
            obj_t *o = obj_create_from_proto(rows[i].vnum);
            if (!o) {
                log_error("player_inventory_load: player %ld vnum %d no longer exists, skipping",
                          player_id, rows[i].vnum);
                continue;
            }
            /* Object maintenance (Session 55/56) -- per-instance state a
             * fresh prototype spawn doesn't have. cur_struct is NULL
             * (has_cur_struct false) for every row saved before this
             * column existed, or an item that was never damaged -- leave
             * the prototype's own full cur_struct alone in that case. */
            if (rows[i].has_cur_struct)
                o->cur_struct = rows[i].cur_struct;
            o->depreciation = rows[i].depreciation;
            snprintf(o->monogram, sizeof(o->monogram), "%s", rows[i].monogram);
            thing_move_to(&o->base, &b->base);
            if (rows[i].slot == INV_SLOT_HELD_PRIMARY)
                b->held[0] = o;
            else if (rows[i].slot == INV_SLOT_HELD_OFFHAND)
                b->held[1] = o;
            else if (rows[i].slot >= 0 && rows[i].slot < LIMB_COUNT) {
                b->equipment[rows[i].slot] = o;
                obj_apply_equip_load_affects(b, o);
            }
            /* WEAR_PAIRED item: saved under one slot, but occupies both
             * members of its pair (or both hands) -- restore the partner. */
            if (obj_is_paired(o)) {
                if (rows[i].slot == INV_SLOT_HELD_PRIMARY
                    || rows[i].slot == INV_SLOT_HELD_OFFHAND) {
                    b->held[0] = o;
                    b->held[1] = o;
                } else if (rows[i].slot >= 0 && rows[i].slot < LIMB_COUNT) {
                    int pp = limb_pair_partner(rows[i].slot);
                    if (pp >= 0)
                        b->equipment[pp] = o;
                }
            }
            /* else INV_SLOT_CARRIED (or garbage) -- carried loose, already
             * attached above. */
        }
    } else {
        db_close(db);
    }
}

/* Persists every THING_OBJ under `parent`, recursing into carried containers.
 * Direct children get their real equip/held/carried slot; anything nested
 * inside a container is saved loose (INV_SLOT_CARRIED) since the flat
 * player_inventory table has no per-instance parent reference yet -- so a
 * container's contents survive a relog but reload loose (nesting resets), and
 * are never silently lost. See STATUS.md's containers decision row. */
static bool inv_save_tree(db_conn_t *db, long player_id, const being_t *b,
                          const thing_t *parent, bool top) {
    bool ok = true;
    for (const thing_t *t = parent->stuff_head; t && ok; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        const obj_t *o = (const obj_t *)t;
        int slot = top ? slot_for_obj(b, o) : INV_SLOT_CARRIED;
        ok = db_query(db, "insert into player_inventory (player_id, vnum, slot, cur_struct, "
                          "depreciation, monogram) values (%i, %i, %i, %i, %i, '%s')",
                      (int)player_id, o->vnum, slot, o->cur_struct, o->depreciation, o->monogram);
        if (ok && obj_is_container(o))
            ok = inv_save_tree(db, player_id, b, &o->base, false);
    }
    return ok;
}

/* Replaces a player's saved inventory wholesale: deletes the old rows and
 * re-saves the current in-memory object tree (via inv_save_tree()) inside
 * one transaction so a mid-save failure can't leave a half-written
 * inventory. */
bool player_inventory_save(long player_id, const being_t *b) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    if (!db_begin(db)) {
        db_close(db);
        return false;
    }

    if (!db_query(db, "delete from player_inventory where player_id=%i", (int)player_id)) {
        db_rollback(db);
        db_close(db);
        return false;
    }

    bool ok = inv_save_tree(db, player_id, b, &b->base, true);

    if (!ok) {
        db_rollback(db);
        db_close(db);
        return false;
    }

    ok = db_commit(db);
    db_close(db);
    return ok;
}

int obj_repo_delete_range(int low, int high) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    db_query(db, "delete from objaffect where vnum between %i and %i", low, high);
    db_query(db, "delete from objextra where vnum between %i and %i", low, high);
    db_query(db, "delete from obj_magic where vnum between %i and %i", low, high);
    db_query(db, "delete from obj where vnum between %i and %i", low, high);
    long n = db_row_count(db);

    db_close(db);
    return (int)n;
}

/* Case-insensitive per-word prefix match against a space-separated
 * keyword list -- `tok` matches if it's a prefix of ANY individual
 * word in it. Local copy, same precedent room_repo.c's own
 * extra_desc_name_matches() documents (every file in this codebase
 * that needs this keeps its own small copy rather than sharing one). */
static bool obj_extra_desc_name_matches(const char *keywords, const char *tok) {
    size_t tok_len = strlen(tok);
    if (tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}
/* `look <keyword>` on an object with a matching hand-authored `objextra`
 * row shows THAT instead of the object's generic long_descr -- same
 * "real seeded data, no code reading it until this" gap room_repo_
 * extra_desc() closed for roomextra (missing-skill audit follow-up,
 * 2026-08-22, user bug report: "look sign doesnt read the extra
 * description"). 6,731 real seeded objextra rows existed with no
 * Tobin code querying the table for display purposes (only
 * obj_repo_delete_range() above touched it, and only to delete). Same
 * shape as room_repo_extra_desc(): writes into `buf` (size `bufsz`),
 * returns false if object `vnum` has no extra description matching
 * `keyword`. */
bool obj_repo_extra_desc(int vnum, const char *keyword, char *buf, size_t bufsz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select name, description from objextra where vnum=%i", vnum)) {
        while (db_fetch_row(db)) {
            if (obj_extra_desc_name_matches(db_get(db, "name"), keyword)) {
                snprintf(buf, bufsz, "%s", db_get(db, "description"));
                found = true;
                break;
            }
        }
    }

    db_close(db);
    return found;
}
