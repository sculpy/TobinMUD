/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "obj.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "skill.h"
#include "thing.h"

/* `materialize <item>` (spell/skill functional-completeness audit
 * continued, level 6: skill.c's own Mage roster entry "Conjures a
 * named item out of thin air, for a price."). Checked the real
 * upstream first (disc/disc_alchemy.cc's `materialize()`/
 * `castMaterialize()`): pay a flat 100 gold (MATERIALIZE_PRICE),
 * search the world's object PROTOTYPE table for a name match cheap
 * enough to conjure (`value <= MATERIALIZE_PRICE`), then a skill roll
 * decides whether it actually manifests -- the gold is spent either
 * way, matching the roster's own "for a price" framing (a gamble, not
 * a guaranteed purchase). Reuses `obj_find_vnum_by_name()`/`obj_proto_
 * load()` (already backing `load obj <name>`, cmd_load.c) for the
 * prototype search instead of a new lookup.
 *
 * Deliberately simplified from the real version: the original can
 * conjure 1-10 copies scaled by how cheap the found item is relative
 * to the price paid, and tries to equip it into a free hand before
 * falling back to the room floor -- ported down to a flat ONE copy,
 * dropped straight into the caster's own inventory (same "conjured
 * items land in inventory, not the floor" precedent `load obj` already
 * established for immortals, 2026-07-22). No fighting/position gate --
 * the real `castMaterialize()`/`materialize()` don't check either. */
bool cmd_materialize(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "materialize")) {
        descriptor_send(d, "You don't know how to do that.\r\n");
        return true;
    }

    /* `args` is already trimmed of leading whitespace by cmd_dispatch()
     * -- used whole, not just its first word, since a real item name is
     * often multiple words ("a rabbit's foot") and obj_find_vnum_by_
     * name() already does its own substring match against the full
     * string. */
    if (!*args) {
        descriptor_send(d, "You need to specify an item.\r\n");
        return true;
    }
    if (strlen(args) < 3) {
        descriptor_send(d, "You must specify something more specific.\r\n");
        return true;
    }
    char name[64];
    snprintf(name, sizeof(name), "%s", args);

#define MATERIALIZE_PRICE 100
    if (ch->progress.gold < MATERIALIZE_PRICE) {
        descriptor_send(d, "You don't have the money for that!\r\n");
        return true;
    }

    int vnum = obj_find_vnum_by_name(name);
    obj_proto_t proto;
    bool found = vnum >= 0 && obj_proto_load(vnum, &proto) && proto.price <= MATERIALIZE_PRICE;

    descriptor_send(d, "You clap your hands together.\r\n");
    if (ch->base.roomp) {
        char capbuf[128], msg[128];
        snprintf(msg, sizeof(msg), "%s claps %s hands together.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)),
                 ch->gender == GENDER_FEMALE ? "her" : ch->gender == GENDER_MALE ? "his" : "their");
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }

    ch->progress.gold -= MATERIALIZE_PRICE;
    if (ch->base.kind == THING_PC)
        player_progress_save(ch->player_id, &ch->progress);

    if (!found) {
        descriptor_send(d, "You get the feeling that such an item cannot be created.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "materialize", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    if (!success) {
        descriptor_send(d, "Nothing happens.\r\n");
        return true;
    }

    obj_t *o = obj_create_from_proto(vnum);
    if (!o) {
        descriptor_send(d, "Nothing happens.\r\n");
        return true;
    }
    thing_move_to(&o->base, &ch->base);
    if (ch->base.kind == THING_PC)
        player_inventory_save(ch->player_id, ch);

    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char msg[192];
    snprintf(msg, sizeof(msg), "In a flash of light, %s appears!\r\n", label);
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        char capbuf[128];
        snprintf(msg, sizeof(msg), "In a flash of light, %s appears in %s's hands!\r\n",
                 label, being_display_name_cap(ch, capbuf, sizeof(capbuf)));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}
