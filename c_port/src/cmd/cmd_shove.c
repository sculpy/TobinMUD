/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "cmd.h"
#include "combat.h"
#include "pulse.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "world.h"

/* `shove <target> <direction>` (spell/skill functional-completeness
 * audit continued, level 6: skill.c's own Warrior roster entry "Push
 * an opponent, knocking them off balance."). Checked the real upstream
 * first (disc/disc_dueling.cc's `doShove()`/`shove()`): refuses while
 * either side is fighting, spends Move, rolls DEX/level-scaled success,
 * and on success physically shoves the victim through a real exit into
 * the adjacent room (`throwChar()`, spec/spec_mobs.cc) -- on FAILURE it
 * starts a fight instead of just fizzling, a deliberate real-game
 * design (a botched shove is provocative). Ported the same shape:
 * spends 8 Vitality (Move's Tobin equivalent, the middle of the real
 * `number(5,10)` roll rather than a random draw), DEX-difference +
 * level-difference to-hit-style roll (Tobin has no separate STR/AGI
 * "reaction" stats the real formula uses), physically relocates the
 * victim via `thing_set_room()` if a real, unblocked exit exists that
 * direction, "slams into the wall" flavor-only otherwise (same as the
 * real fallback). Deliberately NOT ported: the real version's entire
 * mount-vs-mount dismounting branch (shoving a rider off their mount,
 * or shoving while mounted) -- refused outright instead, simpler than
 * porting that whole separate code path faithfully; and the counter-
 * move skill interaction (Tobin's own "counter move" roster entry has
 * no handler yet either, so there's nothing for it to check against). */
bool cmd_shove(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    if (!imm && !being_knows_skill(ch, "shove")) {
        descriptor_send(d, "You can't go pushing people around like that.\r\n");
        return true;
    }
    if (ch->fighting) {
        descriptor_send(d, "Not while fighting.\r\n");
        return true;
    }
    if (ch->position == POSITION_MOUNTED || ch->mount) {
        descriptor_send(d, "You'd need to dismount before doing that.\r\n");
        return true;
    }

    char tok1[64], tok2[32];
    if (sscanf(args, "%63s %31s", tok1, tok2) != 2) {
        descriptor_send(d, "Shove whom, and which way?\r\n");
        return true;
    }

    being_t *target = combat_find_room_target(ch, tok1);
    if (!target) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (target == ch) {
        descriptor_send(d, "Aren't we funny today...\r\n");
        return true;
    }
    if (being_is_immortal(target)) {
        descriptor_send(d, "Oh no you don't!\r\n");
        return true;
    }
    if (target->fighting) {
        descriptor_send(d, "You can't shove them while they're fighting.\r\n");
        return true;
    }
    if (target->position == POSITION_MOUNTED || target->mount) {
        descriptor_send(d, "You can't shove someone off a mount that way.\r\n");
        return true;
    }

    int dir = -1;
    size_t len = strlen(tok2);
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        if (strncasecmp(DIR_NAMES[i], tok2, len) == 0) {
            dir = i;
            break;
        }
    }
    if (dir < 0) {
        descriptor_send(d, "You need to give a direction to shove.\r\n");
        return true;
    }

    if (!imm && ch->progress.vit < 8) {
        char msg[128];
        snprintf(msg, sizeof(msg), "You lack the vitality to shove %s.\r\n",
                 being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }
    if (!imm)
        being_spend_vit(ch, 8);

    const skill_def_t *sk = skill_find(ch->char_class, "shove", imm);
    int modifier = (ch->attrs.dexterity - target->attrs.dexterity) / 4
                  + (ch->progress.level - target->progress.level);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk) + modifier);

    /* `counter move` (Monk, level 25, level-25 audit batch: "Resist
     * being shoved or thrown out of position."). This function's own
     * doc comment flagged the interaction as unported since counter
     * move had no handler at all yet -- same passive-defensive-save
     * shape as `weapon retention`/`brawl avoidance` elsewhere in this
     * audit. */
    if (success && !being_is_immortal(target) && being_knows_skill(target, "counter move")) {
        const skill_def_t *counter_sk = skill_find(target->char_class, "counter move", false);
        if (counter_sk && skill_roll_success(skill_learn_from_doing(target, counter_sk)))
            success = false;
    }

    if (!success) {
        char msg[160], capbuf[128];
        snprintf(msg, sizeof(msg), "You try to shove %s to no avail!\r\n", being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "%s tries to shove you, but has no luck.\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        ch->fighting = target;
        target->fighting = ch;
        being_set_wait(ch, COMBAT_ROUND_PULSES);
        return true;
    }

    room_t *from = ch->base.roomp;
    bool blocked = from->exits[dir] < 0
        || (from->exit_door[dir] != 0 && (from->exit_cond[dir] & EXIT_COND_CLOSED));
    if (blocked) {
        char msg[160], capbuf[128];
        snprintf(msg, sizeof(msg), "You slam %s into the wall!\r\n", being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "%s slams you into the wall!\r\n",
                     being_display_name_cap(ch, capbuf, sizeof(capbuf)));
            descriptor_send(target->desc, msg);
        }
        return true;
    }

    room_t *to = world_get_room(from->exits[dir]);
    if (!to)
        to = room_repo_load(from->exits[dir]);
    if (!to) {
        descriptor_send(d, "Something goes wrong.\r\n");
        return true;
    }
    world_register_room(to);

    char msg[160], capbuf[128];
    snprintf(msg, sizeof(msg), "You push %s %s out of the room!\r\n", being_display_name(target), DIR_NAMES[dir]);
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s pushes you %s out of the room!\r\n",
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), DIR_NAMES[dir]);
        descriptor_send(target->desc, msg);
    }
    snprintf(msg, sizeof(msg), "%s is pushed %s out of the room by %s!\r\n",
             being_display_name(target), DIR_NAMES[dir], being_display_name(ch));
    descriptor_room_echo(from, ch, msg);

    thing_set_room(&target->base, to);
    being_set_wait(target, COMBAT_ROUND_PULSES);

    snprintf(msg, sizeof(msg), "%s is pushed into the room.\r\n", being_display_name_cap(target, capbuf, sizeof(capbuf)));
    descriptor_room_echo(to, target, msg);
    if (target->desc)
        cmd_dispatch(target->desc, "look");

    return true;
}
