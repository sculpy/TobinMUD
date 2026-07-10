/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"

/* Body positions: sit / stand / rest / sleep / wake. Each sets being.position
 * and announces to the room. Movement requires standing (cmd_move.c), you
 * can't see while asleep (cmd_look.c), and you can't change position while
 * fighting. Resting/sleeping heal faster (regen.c). "Fighting" itself is
 * derived from the `fighting` pointer, never stored. */

static void set_position(descriptor_t *d, position_t pos, const char *self,
                         const char *room_fmt) {
    being_t *ch = d->character;
    ch->position = pos;
    descriptor_send(d, self);
    if (ch->base.roomp) {
        char msg[160];
        /* room_fmt gets both the name and the gender-specific possessive
         * pronoun (user 2026-07-09: no blanket "their"); formats with only
         * one %s (sit/rest/sleep/wake) simply ignore the extra vararg. */
        snprintf(msg, sizeof(msg), room_fmt, ch->base.name, gender_possess(ch->gender));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
}

static bool busy_fighting(descriptor_t *d) {
    if (d->character->fighting) {
        descriptor_send(d, "Maybe you should finish this fight first!\r\n");
        return true;
    }
    return false;
}

bool cmd_stand(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (ch->fighting) {
        descriptor_send(d, "You are already on your feet -- and fighting!\r\n");
        return true;
    }
    if (ch->position == POSITION_STANDING) {
        descriptor_send(d, "You are already standing.\r\n");
        return true;
    }
    set_position(d, POSITION_STANDING, "You clamber to your feet.\r\n",
                 "%s clambers to %s feet.\r\n");
    return true;
}

bool cmd_sit(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (busy_fighting(d))
        return true;
    if (ch->position == POSITION_SITTING) {
        descriptor_send(d, "You are already sitting.\r\n");
        return true;
    }
    set_position(d, POSITION_SITTING, "You sit down.\r\n", "%s sits down.\r\n");
    return true;
}

bool cmd_rest(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (busy_fighting(d))
        return true;
    if (ch->position == POSITION_RESTING) {
        descriptor_send(d, "You are already resting.\r\n");
        return true;
    }
    set_position(d, POSITION_RESTING, "You settle down and rest.\r\n",
                 "%s settles down to rest.\r\n");
    return true;
}

bool cmd_sleep(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (busy_fighting(d))
        return true;
    if (ch->position == POSITION_SLEEPING) {
        descriptor_send(d, "You are already fast asleep.\r\n");
        return true;
    }
    set_position(d, POSITION_SLEEPING, "You lie down and drift off to sleep.\r\n",
                 "%s lies down and falls asleep.\r\n");
    return true;
}

bool cmd_wake(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (ch->position != POSITION_SLEEPING) {
        descriptor_send(d, "You are already awake.\r\n");
        return true;
    }
    set_position(d, POSITION_RESTING, "You wake and sit up.\r\n",
                 "%s wakes and sits up.\r\n");
    return true;
}
