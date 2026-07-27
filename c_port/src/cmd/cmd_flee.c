/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "cmd.h"
#include "mob_ai.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

/* `flee`: a chance to break off a fight and bolt through a random exit.
 * Mirrors the original doFlee -- you don't always get away, and if you do
 * you don't choose the direction. On success both sides stop fighting and
 * you're whisked to a neighbouring room; on failure you stay locked in
 * combat and eat the next round. */
bool cmd_flee(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!ch->fighting) {
        descriptor_send(d, "You aren't fighting anyone!\r\n");
        return true;
    }

    room_t *from = ch->base.roomp;

    /* Collect the room's real exits; you can only flee where there's a way. */
    int dirs[ROOM_NUM_EXITS];
    int ndirs = 0;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (from->exits[i] >= 0)
            dirs[ndirs++] = i;
    if (ndirs == 0) {
        descriptor_send(d, "<r>PANIC!<z> There's nowhere to run!\r\n");
        return true;
    }

    /* ~2-in-3 chance to escape (placeholder odds; the original weighs level
     * and speed, which Tobin doesn't model yet). */
    if (rand() % 3 == 0) {
        descriptor_send(d, "<r>PANIC!<z> You stumble and can't get away!\r\n");
        return true;
    }

    int dir = dirs[rand() % ndirs];
    int dest = from->exits[dir];
    room_t *to = world_get_room(dest);
    if (!to) {
        to = room_repo_load(dest);
        if (to)
            world_register_room(to);
    }
    if (!to) {
        descriptor_send(d, "<r>PANIC!<z> You stumble and can't get away!\r\n");
        return true;
    }

    /* Break off combat on both sides. */
    being_t *foe = ch->fighting;
    ch->fighting = NULL;
    if (foe) {
        if (foe->fighting == ch)
            foe->fighting = NULL;
        if (foe->desc) {
            char fm[128];
            snprintf(fm, sizeof(fm), "<y>%s flees from the fight!<z>\r\n", ch->base.name);
            descriptor_notify(foe->desc, fm);
        }
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "%s panics and flees!\r\n", ch->base.name);
    descriptor_room_echo(from, ch, msg);

    thing_set_room(&ch->base, to);
    descriptor_send(d, "<y>You flee head over heels!<z>\r\n");

    snprintf(msg, sizeof(msg), "%s arrives, panting and out of breath.\r\n", ch->base.name);
    descriptor_room_echo(to, ch, msg);

    /* Pursuit (Sneezy → Tobin feature audit, "Monster AI & behavior
     * (pursuit)"): an aggressive mob gets one immediate chance to follow
     * into `to` and resume the fight before the fleeing player's own
     * `look` renders -- so if it succeeds, the chasing mob is already
     * standing there in the room description, not a surprise arrival
     * after the fact. Only for a mob foe (foe->base.kind == THING_MOB
     * inside mob_ai_try_pursue()'s own guard) -- a PC opponent from PK
     * combat never gives chase this way. */
    if (foe)
        mob_ai_try_pursue(foe, ch, to);

    return cmd_dispatch(d, "look");
}
