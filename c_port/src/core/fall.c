/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "fall.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "descriptor.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"
#include "world.h"

/* Best-effort message to b's connection, if any -- mirrors combat.c's own
 * static tell() helper (not shared across files, small enough to just
 * duplicate rather than export). */
static void tell(being_t *b, const char *fmt, ...) {
    if (!b || !b->desc)
        return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    descriptor_notify(b->desc, buf);
}

/* Down-exit index -- matches cmd_move.c's own do_move(d, 5) for `down`
 * (no named DIR_DOWN constant exists there either; raw index is the
 * established convention). */
#define DIR_DOWN 5

void fall_check(being_t *b) {
    if (!b || !b->base.roomp)
        return;
    if (being_is_immortal(b) || being_has_affect(b, AFFECT_FLYING) || b->position == POSITION_MOUNTED)
        return;
    if (!sector_is_fall(b->base.roomp->sector))
        return;

    bool catfall = being_knows_skill(b, "catfall");
    int num1 = catfall ? 10 : 5;
    int num2 = num1 - 2;

    tell(b, "The world spins, as you plunge downwards, out of control!\r\n");
    if (b->base.roomp) {
        char capbuf[128], msg[160];
        snprintf(msg, sizeof(msg), "%s plunges downwards, out of control!\r\n",
                 being_display_name_cap(b, capbuf, sizeof(capbuf)));
        descriptor_room_echo(b->base.roomp, b, msg);
    }

    int count = 0;
    while (count < 100 && sector_is_fall(b->base.roomp->sector)) {
        room_t *from = b->base.roomp;
        int dest = from->exits[DIR_DOWN];
        room_t *next = dest >= 0 ? world_get_room(dest) : NULL;
        if (!next && dest >= 0) {
            next = room_repo_load(dest);
            if (next)
                world_register_room(next);
        }
        if (!next)
            break; /* no floor below at all -- resolve the landing right here */

        thing_set_room(&b->base, next);
        count++;
        if (count > 1) {
            if (catfall)
                tell(b, "You twist around like a cat as you fall, in order to lessen the damage when you land.\r\n");
            else
                tell(b, "You continue to plunge downwards, towards your doom.\r\n");
        }
    }

    if (count == 0)
        return; /* landed the moment it started -- no real fall, no damage */

    room_t *landing = b->base.roomp;
    bool water = sector_is_water(landing->sector);

    /* Agility-style reflex save (Tobin has no isAgile() equivalent --
     * DEX-scaled percentage roll instead, same spirit: a more agile
     * character is likelier to land clean). */
    int agility_chance = 40 + (b->attrs.dexterity - ATTR_BASE) / 2;
    bool agile_save = (rand() % 100) < agility_chance;

    if (count > num1) {
        /* Beyond survivable depth -- unconditionally fatal, same as the
         * real upstream's own fallKill(). */
        if (b->base.kind == THING_PC)
            combat_fall_kill_pc(b);
        return;
    }

    if (count > num2) {
        /* Crush-roll band (Tobin has no getConShock() equivalent --
         * CON-scaled percentage roll instead). A higher-CON character
         * more often survives this band as a bad landing instead of a
         * death. */
        int crush_chance = 50 - (b->attrs.constitution - ATTR_BASE) / 2;
        if (crush_chance < 5)
            crush_chance = 5;
        if ((rand() % 100) < crush_chance) {
            if (b->base.kind == THING_PC)
                combat_fall_kill_pc(b);
            return;
        }

        tell(b, "You are CRUSHED as you impact with the ground!\r\n");
        if (b->base.roomp) {
            char capbuf[128], msg[160];
            snprintf(msg, sizeof(msg), "%s SLAMS into the ground, looking rather pancake-like!\r\n",
                     being_display_name_cap(b, capbuf, sizeof(capbuf)));
            descriptor_room_echo(b->base.roomp, b, msg);
        }
        int dam = count * (40 + rand() % 41);
        if (catfall)
            dam /= 2;
        /* being_hurt_limb() deducts from overall HP too (limb and
         * overall HP aren't separate pools in Tobin's model) -- so a
         * "legs shatter" flourish on top of `dam` would double-count
         * the same damage rather than adding real extra harm. Instead,
         * split `dam` across both legs (worse landing, matches the
         * real "legs splinter" flavor) instead of applying it whole to
         * LIMB_BODY, only when the agility save above also failed. */
        if (!agile_save) {
            tell(b, "You feel the bones in your legs splinter into a million pieces!\r\n");
            being_hurt_limb(b, LIMB_RIGHT_LEG, dam / 2);
            being_hurt_limb(b, LIMB_LEFT_LEG, dam - dam / 2);
        } else {
            being_hurt_limb(b, LIMB_BODY, dam);
        }
        return;
    }

    /* count > 0 && count <= num2: a shorter fall -- a clean agility
     * save avoids damage entirely, otherwise a real but survivable hit,
     * softened by a water landing. */
    if (agile_save) {
        if (water)
            tell(b, "You dive gracefully into the water!\r\n");
        else
            tell(b, "You land safely on your feet!\r\n");
        return;
    }

    int dam;
    if (water) {
        tell(b, "You belly-flop into the water -- that had to have hurt!\r\n");
        dam = count * (5 + rand() % 26);
    } else {
        tell(b, "You scream in pain as you tumble to the ground!\r\n");
        dam = count * (15 + rand() % 41);
    }
    if (catfall)
        dam /= 2;
    being_hurt_limb(b, LIMB_BODY, dam);
}
