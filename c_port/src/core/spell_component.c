/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "spell_component.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "db.h"
#include "log.h"
#include "obj.h"
#include "skill.h"
#include "thing.h"

/* COMP_FILEMAP[] / COMP_FILEMAP_COUNT -- generated filenum -> spell-name
 * table (see its own header). */
#include "spell_component_data.h"

/* Raw upstream itemTypeT for ITEM_COMPONENT (obj.type in the DB, stored
 * verbatim in obj_t.raw_type). Every reagent row is this type; val[2] holds
 * the file-spell-number it binds to, val[3] its usage flags. */
#define COMP_RAW_TYPE 30

/* spell name -> component vnum index, built once at boot from the live obj
 * table. Ordered by vnum, so the FIRST entry matching a given spell is that
 * spell's lowest-vnum representative (real Sneezy's own comp_num pick). */
typedef struct {
    char spell[64];
    int vnum;
    char name[96];
} comp_bind_t;

static comp_bind_t g_binds[512];
static int g_bind_count = 0;

/* Decode a component's file-spell-number to its Tobin spell name, or NULL
 * if the number isn't in the map (a removed/unknown Sneezy spell slot). */
static const char *spell_for_filenum(int fn) {
    for (int i = 0; i < COMP_FILEMAP_COUNT; i++)
        if (COMP_FILEMAP[i].filenum == fn)
            return COMP_FILEMAP[i].spell;
    return NULL;
}

bool obj_is_spell_component(const obj_t *o) {
    return o && o->raw_type == COMP_RAW_TYPE;
}

const char *spell_for_component(const obj_t *o) {
    if (!obj_is_spell_component(o))
        return NULL;
    return spell_for_filenum(o->val[2]);
}

void spell_component_init(void) {
    g_bind_count = 0;
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        log_error("spell_component_init: could not open DB; per-spell "
                  "components disabled (generic fallback still works).");
        return;
    }
    if (db_query(db, "select vnum, val2, short_desc from obj where type=%i "
                     "order by vnum",
                 COMP_RAW_TYPE)) {
        while (db_fetch_row(db)) {
            if (g_bind_count >= (int)(sizeof(g_binds) / sizeof(g_binds[0])))
                break;
            int fn = atoi(db_get(db, "val2"));
            const char *sp = spell_for_filenum(fn);
            if (!sp)
                continue; /* reagent for a spell Tobin's map doesn't name */
            comp_bind_t *b = &g_binds[g_bind_count++];
            snprintf(b->spell, sizeof(b->spell), "%s", sp);
            b->vnum = atoi(db_get(db, "vnum"));
            const char *nm = db_get(db, "short_desc");
            snprintf(b->name, sizeof(b->name), "%s", nm ? nm : "");
        }
    }
    db_close(db);
    log_info("spell_component_init: indexed %d spell-component bindings.",
             g_bind_count);
}

int spell_bound_component_vnum(const char *spell) {
    if (!spell)
        return 0;
    for (int i = 0; i < g_bind_count; i++)
        if (strcasecmp(g_binds[i].spell, spell) == 0)
            return g_binds[i].vnum; /* first == lowest vnum (query ordered) */
    return 0;
}

const char *spell_bound_component_name(const char *spell) {
    if (!spell)
        return NULL;
    for (int i = 0; i < g_bind_count; i++)
        if (strcasecmp(g_binds[i].spell, spell) == 0)
            return g_binds[i].name[0] ? g_binds[i].name : NULL;
    return NULL;
}

/* Depth-limited search of a stuff_head chain for a usable component bound to
 * `spell`. depth>0 descends one level per step into containers (spellbags). */
static obj_t *find_comp_in_stuff(thing_t *head, const char *spell, int depth) {
    for (thing_t *t = head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ) {
            obj_t *o = (obj_t *)t;
            const char *s = spell_for_component(o);
            if (s && strcasecmp(s, spell) == 0)
                return o;
        }
        if (depth > 0 && t->stuff_head) {
            obj_t *r = find_comp_in_stuff(t->stuff_head, spell, depth - 1);
            if (r)
                return r;
        }
    }
    return NULL;
}

obj_t *spell_component_find_for(being_t *ch, const char *spell) {
    if (!ch || !spell)
        return NULL;
    return find_comp_in_stuff(ch->base.stuff_head, spell, 1);
}

int spell_component_merge_siblings(obj_t *o) {
    if (!obj_is_spell_component(o) || !o->base.parent)
        return 0;
    int merged = 0;
    thing_t *t = o->base.parent->stuff_head;
    while (t) {
        thing_t *next = t->stuff_next; /* obj_destroy() relinks the chain */
        if (t != &o->base && t->kind == THING_OBJ) {
            obj_t *s = (obj_t *)t;
            if (obj_is_spell_component(s) && s->vnum == o->vnum) {
                int a = o->val[0] > 0 ? o->val[0] : 1;
                int b = s->val[0] > 0 ? s->val[0] : 1;
                if (a + b <= SPELL_COMP_MERGE_CAP) {
                    /* Charge-weighted-average decay, but only when BOTH
                     * stacks actually decay; a never-decaying (-1) member
                     * keeps the surviving stack never-decaying, so merging
                     * can't accidentally make persistent gear rot. */
                    if (o->decay_time >= 0 && s->decay_time >= 0)
                        o->decay_time =
                            (o->decay_time * a + s->decay_time * b) / (a + b);
                    else if (o->decay_time >= 0 && s->decay_time < 0)
                        o->decay_time = -1;
                    o->val[0] = a + b;
                    obj_destroy(s);
                    merged++;
                }
            }
        }
        t = next;
    }
    return merged;
}

int spell_component_grant_caster(being_t *mob, thing_t *dest, int max_items) {
    if (!mob || !dest || max_items <= 0)
        return 0;

    /* "as if they're saving up for future spells" (user 2026-08-10): a mob
     * can be loaded with reagents for spells up to 2 levels above what it
     * can currently cast. */
    int lvlcap = mob->progress.level + 2;

    int cand[512];
    int nc = 0;
    for (int i = 0; i < g_bind_count && nc < (int)(sizeof(cand) / sizeof(cand[0])); i++) {
        const skill_def_t *sk = skill_find(mob->char_class, g_binds[i].spell, false);
        if (!sk || sk->min_level > lvlcap)
            continue;
        bool dup = false;
        for (int j = 0; j < nc; j++)
            if (cand[j] == g_binds[i].vnum) {
                dup = true;
                break;
            }
        if (!dup)
            cand[nc++] = g_binds[i].vnum;
    }
    if (nc == 0)
        return 0;

    int placed = 0;
    for (int k = 0; k < max_items && nc > 0; k++) {
        int pick = rand() % nc;
        int vnum = cand[pick];
        cand[pick] = cand[--nc]; /* swap-remove so we don't repeat a reagent */

        obj_t *c = obj_create_from_proto(vnum);
        if (!c)
            continue;
        c->decay_time = -1; /* starting/reset gear doesn't decay (see
                               being.c's own supply-loading precedent) */
        thing_move_to(&c->base, dest);
        spell_component_merge_siblings(c);
        placed++;
    }
    return placed;
}
