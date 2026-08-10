/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "combat.h"
#include "skill.h"

/* `ride`/`mount` (alias) and `dismount` -- Mount / riding system
 * (Sneezy → Tobin feature audit). Checked Sneezy's own misc/riding.cc
 * first: the real system is a rich height-ratio/carry-weight/rider-slot
 * gauntlet plus a whole Deikhan "mounted knight" class with its own
 * discipline (SKILL_RIDE_DOMESTIC/WINGED/EXOTIC, SKILL_CHIVALRY, ...).
 * Tobin has no Deikhan class and no per-mob height/weight data to run a
 * ratio check against, so this is scoped WAY down: any class can attempt
 * to ride any HORSE-race mob (mob_race_is_rideable(), being.c) that
 * isn't already ridden or fighting, gated by a single "riding" skill
 * roll every class gets (skill.c, same learn-by-doing shape cast/pray/
 * peek already use). Success grants `POSITION_MOUNTED` to both rider and
 * mount (the mount itself stops wandering/aggroing while ridden, since
 * mob_ai.c's wander/aggro checks already gate on `position ==
 * POSITION_STANDING`) and links `ch->mount`/`target->rider`
 * (being.h) bidirectionally.
 *
 * Movement while mounted (cost discount, indoor auto-dismount, the mount
 * following the rider room-to-room) lives in cmd_move.c; the small
 * mounted attack/AC bonus lives in combat.c/being.c's being_total_ac();
 * teardown on death/quit lives in being_destroy() (being.c). */
bool cmd_ride(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Ride what?\r\n");
        return true;
    }
    if (ch->mount) {
        descriptor_send(d, "You're already mounted -- dismount first.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Maybe you should finish this fight first!\r\n");
        return true;
    }
    if (ch->position != POSITION_STANDING) {
        descriptor_send(d, "You need to be standing to mount up.\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(ch, args);
    if (!target || target->base.kind != THING_MOB || !mob_race_is_rideable(target->mob_race)) {
        descriptor_send(d, "You don't see anything here you could ride.\r\n");
        return true;
    }
    if (target->rider) {
        descriptor_send(d, "Someone's already riding that.\r\n");
        return true;
    }
    if (target->fighting) {
        descriptor_send(d, "Not while it's fighting!\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    const skill_def_t *sk = skill_find(ch->char_class, "riding", imm);
    int riding_prof = imm || !sk ? 100 : skill_learn_from_doing(ch, sk);
    /* `advanced riding` (Deikhan mounted-combat trio, missing-skill audit
     * batch C, 2026-08-09) -- real upstream's getRideMod() gives a flat
     * situational bonus to every riding check for a rider who knows it;
     * ported as a flat proficiency-scaled bonus added to the mount roll
     * itself, same disclosed-simplified shape the rest of this trio
     * uses. */
    if (!imm && being_knows_skill(ch, "advanced riding")) {
        const skill_def_t *adv_sk = skill_find(ch->char_class, "advanced riding", false);
        if (adv_sk)
            riding_prof += skill_learn_from_doing(ch, adv_sk) / 5;
    }
    bool success = imm || !sk || skill_roll_success(riding_prof);
    if (!success) {
        char msg[128];
        snprintf(msg, sizeof(msg), "You try to mount %s, but can't get settled -- you slide right back off.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }

    ch->mount = target;
    target->rider = ch;
    ch->position = POSITION_MOUNTED;
    target->position = POSITION_MOUNTED;

    char msg[128];
    snprintf(msg, sizeof(msg), "You mount %s.\r\n", being_display_name(target));
    descriptor_send(d, msg);

    char room_msg[160];
    snprintf(room_msg, sizeof(room_msg), "%s mounts %s.\r\n", ch->base.name, being_display_name(target));
    descriptor_room_echo(ch->base.roomp, ch, room_msg);
    return true;
}

/* `dismount` command: tears down the bidirectional ch->mount/mount->rider
 * link cmd_ride() set up, dropping both back to POSITION_STANDING.
 * Refused mid-fight since being thrown off is meant to be an involuntary
 * combat outcome, not something you can dodge by just dismounting. */
bool cmd_dismount(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;
    if (!ch->mount) {
        descriptor_send(d, "You're not mounted.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while you're in a fight -- you'd be thrown!\r\n");
        return true;
    }

    being_t *mount = ch->mount;
    ch->mount = NULL;
    mount->rider = NULL;
    ch->position = POSITION_STANDING;
    mount->position = POSITION_STANDING;

    char msg[128];
    snprintf(msg, sizeof(msg), "You dismount %s.\r\n", being_display_name(mount));
    descriptor_send(d, msg);

    if (ch->base.roomp) {
        char room_msg[160];
        snprintf(room_msg, sizeof(room_msg), "%s dismounts %s.\r\n", ch->base.name, being_display_name(mount));
        descriptor_room_echo(ch->base.roomp, ch, room_msg);
    }
    return true;
}
