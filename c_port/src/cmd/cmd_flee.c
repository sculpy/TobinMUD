/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "cmd.h"
#include "mob_ai.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"
#include "world.h"

/* `flee`: a chance to break off a fight and bolt through a random exit.
 * Mirrors the original doFlee -- you don't always get away, and if you do
 * you don't choose the direction. On success both sides stop fighting and
 * you're whisked to a neighbouring room; on failure you stay locked in
 * combat and eat the next round. */
bool cmd_flee(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!ch->fighting) {
        descriptor_send(d, "You aren't fighting anyone!\r\n");
        return true;
    }

    /* ROOM_FLAG_NO_FLEE (room-flag effects port): some rooms magically
     * pin you in place -- upstream offense.cc refuses the flee outright
     * with "a strange power prevents you from escaping" (distinct from
     * NO-ESCAPE, which only blocks teleport/recall). Immortals ignore it,
     * same as every other gate here. ~77 live rooms carry this bit. */
    if (!being_is_immortal(ch) && (ch->base.roomp->room_flag & ROOM_FLAG_NO_FLEE)) {
        descriptor_send(d, "<r>A strange power prevents you from escaping!<z>\r\n");
        char pin[128];
        snprintf(pin, sizeof(pin),
                 "<y>%s tries to flee, but a strange power holds them fast!<z>\r\n",
                 ch->base.name);
        descriptor_room_echo(ch->base.roomp, ch, pin);
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

    /* `retreat` (Warrior/Thief/Monk, missing-skill audit, 2026-08-09):
     * real upstream help text -- "utilized during a flee from combat to
     * maintain a fighting withdrawal while escaping. A successful
     * retreat will not invoke panic... give greater control over which
     * direction they are able to escape in and minimize the losses...
     * Retreating is handled automatically anytime you flee." Ported as:
     * (1) a better escape chance (~2-in-3 -> ~5-in-6, proficiency-scaled
     * on top of that floor); (2) if the fleeing player named a direction
     * that's actually a real exit, retreat lets them choose it instead
     * of the usual random pick ("greater control over which direction");
     * (3) calmer, non-panicked flavor text instead of the PANIC!
     * messaging below. Tobin has no flee-XP-loss mechanic to "minimize"
     * in the first place (checked cmd_flee.c/combat.c -- fleeing never
     * deducts XP here), so that part of the roster text is moot, not
     * skipped. */
    bool imm = being_is_immortal(ch);
    bool retreating = false;
    int flee_fail_pct = 33;
    if (!imm && being_knows_skill(ch, "retreat")) {
        const skill_def_t *retreat_sk = skill_find(ch->char_class, "retreat", false);
        if (retreat_sk) {
            int prof = skill_learn_from_doing(ch, retreat_sk);
            retreating = true;
            flee_fail_pct = 15 - prof / 10;
            if (flee_fail_pct < 2)
                flee_fail_pct = 2;
        }
    }

    if (!imm && rand() % 100 < flee_fail_pct) {
        descriptor_send(d, retreating
            ? "<y>You try to withdraw, but can't find an opening!<z>\r\n"
            : "<r>PANIC!<z> You stumble and can't get away!\r\n");
        return true;
    }

    int dir = dirs[rand() % ndirs];
    if (retreating) {
        char tok[16];
        if (sscanf(args, "%15s", tok) == 1) {
            int req = -1;
            size_t len = strlen(tok);
            for (int i = 0; i < ROOM_NUM_EXITS; i++)
                if (strncasecmp(DIR_NAMES[i], tok, len) == 0) {
                    req = i;
                    break;
                }
            for (int i = 0; req >= 0 && i < ndirs; i++) {
                if (dirs[i] == req) {
                    dir = req;
                    break;
                }
            }
        }
    }
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
    snprintf(msg, sizeof(msg), retreating ? "%s makes a fighting withdrawal!\r\n" : "%s panics and flees!\r\n", ch->base.name);
    descriptor_room_echo(from, ch, msg);

    thing_set_room(&ch->base, to);
    /* Names the actual direction fled (user 2026-08-06: "when you flee
     * you should see what direction you fled") -- `dir` was already
     * picked at random above; DIR_NAMES (room.h) is the same lookup
     * `exits`/cmd_move.c use for direction names elsewhere. */
    char fleemsg[64];
    if (retreating)
        snprintf(fleemsg, sizeof(fleemsg), "<y>You retreat %s, keeping your guard up.<z>\r\n", DIR_NAMES[dir]);
    else
        snprintf(fleemsg, sizeof(fleemsg), "<y>You flee %s, head over heels!<z>\r\n", DIR_NAMES[dir]);
    descriptor_send(d, fleemsg);

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
