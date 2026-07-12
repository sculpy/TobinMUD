/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <strings.h>

#include "being.h"
#include "combat.h"

/* `consider <target>` (Sneezy port, user 2026-07-12). Scoped down from
 * the original's `doConsider()` (cmd_consider.cc): that version also
 * breaks down trophy-tracked kill counts, per-lore-skill creature-type
 * identification (SKILL_CONS_ANIMAL/VEGGIE/UNDEAD/...), and estimated
 * HP/AC/attack-count ranges -- none of which Tobin has built yet
 * (trophy tracking, those specific lore sub-skills, or a "learnedness"
 * estimate system). What's kept, because the infrastructure already
 * exists: `consider self` reads your own armor via `being_total_ac()`
 * (same AC system as combat's to-hit formula); considering an
 * immortal or another mortal PC gets the same flavor-only refusals as
 * the original; considering a mob gives the same plain level-
 * difference ladder wording. */
bool cmd_consider(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Consider killing whom?\r\n");
        return true;
    }

    if (strcasecmp(args, "self") == 0) {
        descriptor_send(d, "You consider yourself...\r\n");
        int ac = being_total_ac(ch);
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Your equipment would seem %s for your class and level.\r\n",
                 ac >= 40 ? "fantastic" : ac >= 25 ? "very good" : ac >= 12 ? "good"
                 : ac >= 5 ? "o.k." : ac > 0 ? "poor" : ac == 0 ? "unarmored"
                 : "laughably pathetic");
        descriptor_send(d, msg);
        return true;
    }

    being_t *victim = combat_find_room_target(ch, args);
    if (!victim) {
        descriptor_send(d, "Consider killing whom?\r\n");
        return true;
    }

    if (victim->base.kind == THING_PC) {
        if (being_is_immortal(victim)) {
            descriptor_send(d, "You must sure have a big ego to contemplate fighting gods.\r\n");
            if (victim->desc) {
                char msg[128];
                snprintf(msg, sizeof(msg), "%s just considered fighting you.\r\n", ch->base.name);
                descriptor_notify(victim->desc, msg);
            }
        } else {
            descriptor_send(d, "Would you like to borrow a cross and a shovel?\r\n");
        }
        return true;
    }

    int diff = victim->progress.level - ch->progress.level;
    const char *verdict;
    if (diff <= -15)      verdict = "Shall I tie both hands behind your back, or just one?";
    else if (diff <= -10) verdict = "Why bother???";
    else if (diff <= -6)  verdict = "Don't strain yourself.";
    else if (diff <= -3)  verdict = "Piece of cake.";
    else if (diff <= -2)  verdict = "Odds are in your favor.";
    else if (diff <= -1)  verdict = "You have a slight advantage.";
    else if (diff == 0)   verdict = "A fair fight.";
    else if (diff <= 1)   verdict = "It doesn't look that tough...";
    else if (diff <= 2)   verdict = "Cross your fingers.";
    else if (diff <= 3)   verdict = "Cross your fingers and hope they don't get broken.";
    else if (diff <= 6)   verdict = "I hope you have a good plan!";
    else if (diff <= 10)  verdict = "Bring friends.";
    else if (diff <= 15)  verdict = "You and what army??";
    else if (diff <= 30)  verdict = "You'll win if it never hits you.";
    else                  verdict = "There are better ways to suicide.";

    char msg[160];
    snprintf(msg, sizeof(msg), "%s\r\n", verdict);
    descriptor_send(d, msg);
    return true;
}
