/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "affect.h"
#include "being.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* `scribe <spell>` (Mage, level 25 advanced spec-proc) -- inscribe a spell
 * you know onto a fresh scroll anyone can later `use`. Faithful to upstream
 * doScribe()'s core gates ("you don't know that spell" / "lack any
 * knowledge of how to scribe scrolls"), with two disclosed divergences
 * forced by Tobin's model:
 *   - No parchment/spell/scribe component-charge trio: Tobin does not model
 *     upstream's three-component charge economy, so scribing is gated by
 *     spell knowledge plus a short recast cooldown (AFFECT_SCRIBE_COOLDOWN)
 *     rather than consuming charges -- one scroll per invocation.
 *   - The scroll is ephemeral (obj_create_ephemeral, vnum 0): usable for
 *     the rest of the session but not across a server restart, there being
 *     no blank-scroll prototype or per-instance inventory column to persist
 *     it. The spell rides on obj_t.scribed_spell, which cmd_use.c honours
 *     ahead of the vnum-keyed obj_magic table. */
bool cmd_scribe(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "scribe")) {
        descriptor_send(d, "You lack any knowledge of how to scribe scrolls.\r\n");
        return true;
    }
    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "You need to specify a spell to scribe.\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_SCRIBE_COOLDOWN)) {
        descriptor_send(d, "Your hand is still cramped from the last scroll -- rest a moment.\r\n");
        return true;
    }
    const skill_def_t *spell = skill_find(CLASS_MAGE, args, true);
    if (!spell) {
        descriptor_send(d, "You can't scribe a scroll of that type.\r\n");
        return true;
    }
    if (!imm && !being_knows_skill(ch, args)) {
        descriptor_send(d, "You don't know that spell.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "scribe", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_apply_affect(ch, AFFECT_SCRIBE_COOLDOWN, SCRIBE_COOLDOWN_ROUNDS);

    char msg[256];
    if (!success) {
        snprintf(msg, sizeof(msg), "Your hand slips and the scroll of %s is ruined.\r\n", spell->name);
        descriptor_send(d, msg);
        return true;
    }

    char nm[128], sd[160], ld[192];
    snprintf(nm, sizeof(nm), "scroll handwritten %s", spell->name);
    snprintf(sd, sizeof(sd), "a handwritten scroll of %s", spell->name);
    snprintf(ld, sizeof(ld), "A handwritten scroll of %s lies here.", spell->name);
    obj_t *scroll = obj_create_ephemeral(nm, sd, ld, OBJ_CAT_MAGIC_DEVICE);
    if (!scroll) {
        descriptor_send(d, "Something goes wrong and the scroll crumbles to dust.\r\n");
        return true;
    }
    scroll->raw_type = 2; /* ITEM_SCROLL, so cmd_use accepts it */
    snprintf(scroll->scribed_spell, sizeof(scroll->scribed_spell), "%s", spell->name);
    thing_move_to(&scroll->base, &ch->base);

    snprintf(msg, sizeof(msg), "You carefully scribe %s.\r\n", sd);
    descriptor_send(d, msg);
    return true;
}
