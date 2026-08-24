/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "combat.h"
#include "pulse.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"
#include "world.h"

/* `doorbash <direction>` (Unimplemented skills/spells backlog, Session
 * 158 audit: Warrior "doorbash", skill.c level 1). Real upstream
 * (cmd/cmd_doorbash.cc) is a heavy affair -- a door weight vs.
 * maxWieldWeight gauntlet, a lock_difficulty roll, self-collision
 * damage with an AFF_STUNNED daze, and moving you through on a burst.
 * Tobin exits carry no weight/lock_difficulty data (only the CLOSED/
 * LOCKED/SECRET/TRAPPED condition bits, room.h) and there is no stun
 * affect, so this is scoped down to the mechanic that actually matters:
 * a Warrior can FORCE a closed -- even LOCKED -- door open by brute
 * strength, no key needed. One skill_roll_success() roll (same learn-
 * by-doing shape as bash/kick/berserk). On success the door bursts open
 * (CLOSED and LOCKED both cleared, synced to the far side same as
 * cmd_open.c does); you then walk through normally (a disclosed scope-
 * cut from upstream auto-moving you). On failure you bounce off and
 * take a little self-damage (the "OUCH! that REALLY hurt" flavor, minus
 * the stun Tobin has no affect for). Always costs a heavy combat-lag
 * round, win or lose (the anti-spam brake, same as bash). */

static int parse_dir(const char *tok) {
    size_t len = strlen(tok);
    if (len == 0)
        return -1;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    return -1;
}

/* Mirror a burst-open onto the far side of the door, same "treat both
 * sides as the same physical door" logic cmd_open.c's sync_reverse_door()
 * uses -- but clearing LOCKED too (a bashed door is broken off, not
 * merely unlatched). Only touches a reverse exit that genuinely points
 * back here and has its own door. */
static void sync_reverse_bash(room_t *r, int dir) {
    int dest = r->exits[dir];
    if (dest < 0)
        return;
    room_t *far = world_get_room(dest);
    if (!far) {
        far = room_repo_load(dest);
        if (far)
            world_register_room(far);
    }
    if (!far)
        return;
    int rev = REV_DIR[dir];
    if (far->exits[rev] != r->vnum || far->exit_door[rev] == 0)
        return;
    far->exit_cond[rev] &= ~(EXIT_COND_CLOSED | EXIT_COND_LOCKED);
    room_repo_save_exit(far->vnum, rev, far->exits[rev], far->exit_door[rev], far->exit_cond[rev]);
    char echo[128];
    snprintf(echo, sizeof(echo), "The door to the %s bursts open from the far side!\r\n", DIR_NAMES[rev]);
    descriptor_room_echo(far, NULL, echo);
}

bool cmd_doorbash(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "doorbash")) {
        descriptor_send(d, "You know nothing about door bashing.\r\n");
        return true;
    }
    if (ch->position == POSITION_MOUNTED) {
        descriptor_send(d, "Yeah... right... while mounted.\r\n");
        return true;
    }

    char tok[64];
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Bash down the door in which direction? (doorbash <direction>)\r\n");
        return true;
    }
    int dir = parse_dir(tok);
    room_t *r = ch->base.roomp;
    if (dir < 0 || r->exits[dir] < 0) {
        descriptor_send(d, "There's no exit that way to bash through.\r\n");
        return true;
    }
    if (r->exit_door[dir] == 0) {
        descriptor_send(d, "There's no door that way.\r\n");
        return true;
    }
    if (!(r->exit_cond[dir] & EXIT_COND_CLOSED)) {
        descriptor_send(d, "That door is already open.\r\n");
        return true;
    }

    /* door_type_name() is capitalized ("Door"/"Gate"); lowercase for mid-
     * sentence use, same as cmd_open.c. */
    char door[16];
    snprintf(door, sizeof(door), "%s", door_type_name(r->exit_door[dir]));
    for (char *p = door; *p; p++)
        *p = (char)tolower((unsigned char)*p);

    const skill_def_t *sk = skill_find(ch->char_class, "doorbash", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    if (!success) {
        int dmg = 2 + rand() % 8;
        limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
        int hp_before = ch->limbs[limb].hp;
        being_hurt_limb(ch, limb, dmg);
        char msg[220];
        snprintf(msg, sizeof(msg),
                 "You hurl yourself at the %s to the %s -- it holds fast, and you "
                 "bounce off, your %s %s\r\n",
                 door, DIR_NAMES[dir], limb_name(limb), describe_dam(dmg, hp_before, NULL));
        descriptor_send(d, msg);
        char capbuf[128], echo[220];
        snprintf(echo, sizeof(echo), "%s slams into the %s to the %s with no effect.\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), door, DIR_NAMES[dir]);
        descriptor_room_echo(r, ch, echo);
        return true;
    }

    r->exit_cond[dir] &= ~(EXIT_COND_CLOSED | EXIT_COND_LOCKED);
    room_repo_save_exit(r->vnum, dir, r->exits[dir], r->exit_door[dir], r->exit_cond[dir]);
    sync_reverse_bash(r, dir);

    char msg[220];
    snprintf(msg, sizeof(msg), "You charge the %s to the %s and it bursts off its hinges!\r\n",
             door, DIR_NAMES[dir]);
    descriptor_send(d, msg);
    char capbuf[128], echo[220];
    snprintf(echo, sizeof(echo), "%s smashes the %s to the %s clean off its hinges!\r\n",
             being_display_name_cap(ch, capbuf, sizeof(capbuf)), door, DIR_NAMES[dir]);
    descriptor_room_echo(r, ch, echo);
    return true;
}
