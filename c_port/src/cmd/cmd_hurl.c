/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "being.h"
#include "cmd.h"
#include "combat.h"
#include "pulse.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"
#include "world.h"

/* `hurl` (Monk, level 25, level-25 audit batch: "Throw a victim bodily
 * out of the room."). Picks a random real, unblocked exit from the
 * caster's room and physically relocates the defeated-in-this-exchange
 * target through it -- same "physically relocate via thing_set_room()
 * if a real, unblocked exit exists, flavor-only bounce-off-the-wall
 * otherwise" shape cmd_shove.c already established for this kind of
 * forced-movement skill, just room-exit-random instead of caster-
 * chosen-direction. Deals a bonus hit on top (the roster's own "bodily"
 * phrasing implies real force, not a harmless toss), same shape as
 * `kneestrike`/`chop`. Mob targets only -- same PvP-consent precedent
 * as `whirlwind`/`taunt` elsewhere in this audit. */
bool cmd_hurl(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!ch->fighting) {
        descriptor_send(d, "Hurl whom? You're not fighting anyone.\r\n");
        return true;
    }
    if (ch->fighting->base.kind != THING_MOB) {
        descriptor_send(d, "You can only hurl a mob this way.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "hurl")) {
        descriptor_send(d, "You don't know how to hurl an opponent.\r\n");
        return true;
    }

    being_t *target = ch->fighting;
    const skill_def_t *sk = skill_find(ch->char_class, "hurl", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    being_set_wait(ch, 2 * COMBAT_ROUND_PULSES);

    char msg[160];
    if (!success) {
        snprintf(msg, sizeof(msg), "You try to hurl %s, but can't get the leverage!\r\n", being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }

    int dmg = 3 + ch->progress.level / 4 + (rand() % 6);
    limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
    int limb_hp_before = target->limbs[limb].hp;
    bool defeated = combat_apply_skill_damage(ch, target, dmg, limb);
    const char *intensity = describe_dam(dmg, limb_hp_before, NULL);

    room_t *from = ch->base.roomp;
    int dirs[ROOM_NUM_EXITS];
    int n_dirs = 0;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (from->exits[i] >= 0 && !(from->exit_door[i] != 0 && (from->exit_cond[i] & EXIT_COND_CLOSED)))
            dirs[n_dirs++] = i;

    if (defeated || n_dirs == 0) {
        snprintf(msg, sizeof(msg), "You hurl %s bodily %s!\r\n", being_display_name(target), intensity);
        descriptor_send(d, msg);
        return true;
    }

    int dest_vnum = from->exits[dirs[rand() % n_dirs]];
    room_t *dest = world_get_room(dest_vnum);
    if (!dest) {
        dest = room_repo_load(dest_vnum);
        if (dest)
            world_register_room(dest);
    }
    if (!dest) {
        snprintf(msg, sizeof(msg), "You hurl %s bodily %s!\r\n", being_display_name(target), intensity);
        descriptor_send(d, msg);
        return true;
    }

    target->fighting = NULL;
    ch->fighting = NULL;
    thing_set_room(&target->base, dest);

    snprintf(msg, sizeof(msg), "You hurl %s bodily out of the room, striking them %s!\r\n",
             being_display_name(target), intensity);
    descriptor_send(d, msg);
    if (target->desc) {
        descriptor_notify(target->desc, "You're hurled bodily out of the room!\r\n");
        cmd_dispatch(target->desc, "look");
    }
    return true;
}
