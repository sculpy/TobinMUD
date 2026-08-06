/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "affect.h"
#include "being.h"
#include "skill.h"

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

/* Shared guard for the position commands that can't be used mid-combat
 * (sit/rest/sleep) -- stand and wake have their own separate checks. */
static bool busy_fighting(descriptor_t *d) {
    if (d->character->fighting) {
        descriptor_send(d, "Maybe you should finish this fight first!\r\n");
        return true;
    }
    return false;
}

/* `stand` command: gets the character back on their feet. Fighting
 * characters are already considered standing (you can't sit/rest/sleep
 * while fighting), so that case gets its own message rather than
 * routing through busy_fighting(). */
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

/* `yoginsa` (Monk, and anyone else who knows it) used to require
 * manually typing the command to start the background meditation task
 * (meditate_tick_run(), meditate.c) -- user 2026-08-06: "yoginsa should
 * be automatic". Sitting or resting now starts it on its own for
 * anyone who knows the skill and isn't already meditating; `yoginsa`
 * itself still works too (as an explicit stop/restart toggle, or to
 * start meditating without changing position first -- see its own
 * header comment, cmd_yoginsa.c), it's just no longer the only way in. */
static void auto_start_meditating(descriptor_t *d, being_t *ch) {
    if (ch->meditating)
        return;
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "yoginsa"))
        return;
    ch->meditating = true;
    descriptor_send(d, "You begin meditating.\r\n");
}

/* `sit` command: drops the character to POSITION_SITTING. */
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
    auto_start_meditating(d, ch);
    return true;
}

/* `rest` command: settles the character into POSITION_RESTING, which
 * heals faster than standing/sitting (see regen.c). */
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
    auto_start_meditating(d, ch);
    return true;
}

/* `sleep` command: puts the character into POSITION_SLEEPING. Sleeping
 * characters can't see the room (cmd_look.c) but heal fastest of all
 * the positions (regen.c). */
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

/* `wake` command: rouses a sleeping character back to POSITION_RESTING
 * (not standing -- they still have to `stand` afterward). No-op with a
 * message if they weren't asleep to begin with. */
bool cmd_wake(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (ch->position != POSITION_SLEEPING) {
        descriptor_send(d, "You are already awake.\r\n");
        return true;
    }
    /* `slumber` (AFFECT_SLEEP, cmd_cast.c) is a forced sleep, not the
     * voluntary kind -- without this check `wake` would let a slumbered
     * player instantly stand back up, making the whole spell a no-op.
     * Ordinary sleep (no AFFECT_SLEEP) still wakes normally. */
    if (being_has_affect(ch, AFFECT_SLEEP)) {
        descriptor_send(d, "You can't fight off the magical urge to sleep!\r\n");
        return true;
    }
    set_position(d, POSITION_RESTING, "You wake and sit up.\r\n",
                 "%s wakes and sits up.\r\n");
    return true;
}
