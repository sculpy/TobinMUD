/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "descriptor.h"
#include "mob_ai.h"
#include "room.h"
#include "thing.h"

/* `egotrip <subcommand>` (Sneezy port, user 2026-07-12, expanded
 * 2026-08-08). The original (cmd_egotrip.cc) is a 13-subcommand
 * immortal toy-box (deity/bless/blast/damn/hate/cleanse/wander/
 * stupidity/crit/portal/teleport/disease/garble). This port covers
 * every subcommand that maps onto a REAL Tobin system:
 *
 *   blast     -- halves target's current HP (2026-07-12, unchanged).
 *   damn      -- instant, no-consequence kill (2026-08-08 follow-up,
 *                user: "bypass xp loss on an egotrip hit"). Sneezy's own
 *                `damn` is just three curse debuffs, never lethal -- this
 *                is a deliberate Tobin-original reinterpretation, not a
 *                literal port, since the user specifically wanted a
 *                LETHAL toy-box tool. Routes through the real
 *                combat_defeat() "slain" pipeline (same corpse/respawn/
 *                broadcast path any other kill uses) rather than a fake
 *                kill, but the XP-loss branch inside combat_defeat() is
 *                already conditioned on `winner->base.kind != THING_PC`
 *                (2026-08-04's "Player PK should neither gain nor lose
 *                experience" rule) -- since the egotrip caller (an
 *                immortal) IS a THING_PC, that condition is false and
 *                the loss is skipped automatically, with zero new code
 *                needed in combat.c itself. Refuses an immortal target,
 *                same guard the real Sneezy `crit` subcommand uses
 *                ("Bad god, no bone!").
 *   disease   -- inflicts a named disease on target (Tobin's own
 *                AFFECT_DISEASE_* range, affect.c).
 *   cleanse   -- cures every disease + poison affect on every connected
 *                being, world-wide.
 *   stupidity -- casts stupidity (AFFECT_STUPIDITY, cmd_cast.c's own
 *                penalty formula) on every connected mortal.
 *   wander    -- forces every eligible mob in the caller's room to
 *                attempt its wander move right now (mob_ai_force_wander()).
 *   crit      -- forces a random MINOR limb sever on target, reusing
 *                Tobin's own limb-crit mechanic (combat_egotrip_crit())
 *                instead of Sneezy's missing numbered crit-effect table.
 *
 * Still disclosed as NOT ported -- no reusable Tobin system to hang them
 * on without building real new infrastructure first:
 *   deity   -- Sneezy forces a global MOB_ALIGN_PULSE spec-proc sweep;
 *              Tobin's spec-proc architecture has no equivalent hook.
 *   bless   -- Sneezy's version is 17 different named-god flavor
 *              blessings, each its own affect type/stat combo; Tobin has
 *              no generic "flat blessing" affect to reuse at all.
 *   portal  -- Sneezy creates a persistent, walk-through two-way portal
 *              OBJECT pair; Tobin has no portal object type (only the
 *              instant, one-shot `ethereal gate` cast spell).
 *   hate    -- Sneezy adds target to every room mob's per-mob hated-list;
 *              Tobin's aggro model is alignment-based, no per-mob
 *              targeted-hate list exists.
 *   teleport -- already fully covered by the separate, existing
 *              `transfer <name> [vnum]` command (cmd_transfer.c) --
 *              not duplicated here.
 *   garble  -- no speech-distortion system exists in Tobin at all
 *              (cmd_say.c's own header comment already discloses this).
 *
 * Target lookup (2026-08-08, user: "make egotrip usable on mobs too"):
 * every targeted subcommand (blast/damn/crit/disease) now uses
 * egotrip_find_target() below -- a PC is still found world-wide (any
 * connected player, same g_descriptors scan as always), but a name that
 * doesn't match any PC now falls back to a mob in the CALLER'S OWN room
 * (combat_find_room_target(), same helper `kill`/`disarm`/`force`
 * already share). Tobin has no world-wide mob-by-name index, so a mob
 * target's reach is disclosed as room-only, same narrower scope `force`
 * already established for exactly this reason. */

/* Shared target resolver for every egotrip subcommand that takes a
 * <target> argument -- PC world-wide, mob fallback in the caller's own
 * room. See the header comment above for the full rationale. */
static being_t *egotrip_find_target(being_t *ch, const char *tok) {
    size_t len = strlen(tok);
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->character && strncasecmp(it->character->base.name, tok, len) == 0)
            return it->character;
    }
    if (ch->base.roomp)
        return combat_find_room_target(ch, tok);
    return NULL;
}

bool cmd_egotrip(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char sub[16] = "";
    int consumed = 0;
    sscanf(args, "%15s %n", sub, &consumed);
    const char *rest = args + consumed;
    while (*rest == ' ')
        rest++;

    if (!sub[0]) {
        descriptor_send(d,
            "Syntax: egotrip <subcommand>\r\n"
            "  blast <target>              -- halve a target's current HP\r\n"
            "  damn <target>               -- instant kill, no XP loss\r\n"
            "  disease <target> <disease>  -- cold/dysentery/flu/pneumonia/leprosy/\r\n"
            "                                 gangrene/plague/scurvy\r\n"
            "  cleanse                     -- cure all disease/poison, world-wide\r\n"
            "  stupidity                   -- casts stupidity on every mortal\r\n"
            "  wander                      -- forces every mob in your room to move\r\n"
            "  crit <target>               -- forces a random minor limb sever\r\n"
            "(deity/bless/portal/hate/teleport/garble are not ported -- see HELP EGOTRIP.)\r\n");
        return true;
    }

    if (strcasecmp(sub, "blast") == 0) {
        char tok[64];
        if (sscanf(rest, "%63s", tok) != 1) {
            descriptor_send(d, "Syntax: egotrip blast <target>\r\n");
            return true;
        }

        being_t *target = egotrip_find_target(ch, tok);
        if (!target) {
            char out[128];
            snprintf(out, sizeof(out), "No one named '%s' is in the game.\r\n", tok);
            descriptor_send(d, out);
            return true;
        }

        target->progress.hp /= 2;
        if (target->progress.hp < 1)
            target->progress.hp = 1;

        char out[300];
        snprintf(out, sizeof(out), "You blast %s with a bolt of lightning.\r\n", target->base.name);
        descriptor_send(d, out);

        if (target->desc) {
            descriptor_notify(target->desc,
                "A bolt of lightning streaks down from the heavens right at your feet!\r\n"
                "BZZZZZaaaaaappppp!!!!!\r\n");
        }
        if (target->base.roomp) {
            snprintf(out, sizeof(out),
                     "A bolt of lightning streaks down from the heavens right at %s's feet!\r\n",
                     target->base.name);
            descriptor_room_echo(target->base.roomp, target, out);
        }
        return true;
    }

    if (strcasecmp(sub, "damn") == 0) {
        char tok[64];
        if (sscanf(rest, "%63s", tok) != 1) {
            descriptor_send(d, "Syntax: egotrip damn <target>\r\n");
            return true;
        }

        being_t *target = egotrip_find_target(ch, tok);
        if (!target) {
            char out[128];
            snprintf(out, sizeof(out), "No one named '%s' is in the game.\r\n", tok);
            descriptor_send(d, out);
            return true;
        }
        if (target == ch) {
            descriptor_send(d, "Damn yourself? That seems unnecessary.\r\n");
            return true;
        }
        if (being_is_immortal(target)) {
            descriptor_send(d, "Do this to an immortal??? Bad god, no bone!\r\n");
            return true;
        }
        if (!target->base.roomp) {
            descriptor_send(d, "They're nowhere -- there's nothing to strike down.\r\n");
            return true;
        }

        char out[300];
        char namebuf[64];
        being_display_name_cap(target, namebuf, sizeof(namebuf));

        if (target->desc)
            descriptor_notify(target->desc, "You have been DAMNED! The world goes dark...\r\n");
        snprintf(out, sizeof(out), "%s is struck down by an unseen, wrathful force!\r\n", namebuf);
        descriptor_room_echo(target->base.roomp, target, out);

        snprintf(out, sizeof(out), "You point at %s and utter a word of damnation.\r\n", target->base.name);
        descriptor_send(d, out);

        /* Real kill, real corpse/respawn/broadcast pipeline -- see the
         * header comment for why this costs the target no XP even
         * though it routes through the exact same combat_defeat() every
         * other kill uses: the winner here is always a THING_PC
         * (the immortal), and combat_defeat()'s XP-loss branch is
         * already conditioned on the winner NOT being a PC. */
        combat_egotrip_damn(ch, target);
        return true;
    }

    if (strcasecmp(sub, "crit") == 0) {
        char tok[64];
        if (sscanf(rest, "%63s", tok) != 1) {
            descriptor_send(d, "Syntax: egotrip crit <target>\r\n");
            return true;
        }
        being_t *target = egotrip_find_target(ch, tok);
        if (!target) {
            char out[128];
            snprintf(out, sizeof(out), "No one named '%s' is in the game.\r\n", tok);
            descriptor_send(d, out);
            return true;
        }
        if (!combat_egotrip_crit(ch, target)) {
            descriptor_send(d, "Could not find an eligible limb -- they may have none left to sever.\r\n");
            return true;
        }
        char out[200];
        snprintf(out, sizeof(out), "It looks like some bad luck has befallen %s. Heh, heh, heh.\r\n",
                 target->base.name);
        descriptor_send(d, out);
        return true;
    }

    if (strcasecmp(sub, "disease") == 0) {
        char tok[64], dtok[32];
        if (sscanf(rest, "%63s %31s", tok, dtok) != 2) {
            descriptor_send(d,
                "Syntax: egotrip disease <target> <disease>\r\n"
                "Viable cruelties include: cold, dysentery, flu, pneumonia, leprosy,\r\n"
                "gangrene, plague, scurvy\r\n");
            return true;
        }
        being_t *target = egotrip_find_target(ch, tok);
        if (!target) {
            char out[128];
            snprintf(out, sizeof(out), "No one named '%s' is in the game.\r\n", tok);
            descriptor_send(d, out);
            return true;
        }
        if (being_is_immortal(target)) {
            descriptor_send(d, "Bummer, they're immune.\r\n");
            return true;
        }

        /* Names + durations (5x cmd_drink.c's own accidental-infection
         * durations -- a deliberate curse should last longer than bad
         * luck from a puddle) for the 8 diseases Sneezy's own egotrip
         * offers. Duration is in combat rounds (~1.2s each), same unit
         * being_apply_affect() everywhere else already uses. */
        static const struct {
            const char *name;
            affect_type_t type;
            int duration;
        } DISEASES[] = {
            {"cold", AFFECT_DISEASE_COLD, 150},
            {"dysentery", AFFECT_DISEASE_DYSENTERY, 150},
            {"flu", AFFECT_DISEASE_FLU, 250},
            {"pneumonia", AFFECT_DISEASE_PNEUMONIA, 250},
            {"leprosy", AFFECT_DISEASE_LEPROSY, 300},
            {"gangrene", AFFECT_DISEASE_GANGRENE, 350},
            {"plague", AFFECT_DISEASE_PLAGUE, 400},
            {"scurvy", AFFECT_DISEASE_SCURVY, 100},
        };
        affect_type_t chosen = AFFECT_NONE;
        int duration = 0;
        for (size_t i = 0; i < sizeof(DISEASES) / sizeof(DISEASES[0]); i++) {
            if (strncasecmp(DISEASES[i].name, dtok, strlen(dtok)) == 0) {
                chosen = DISEASES[i].type;
                duration = DISEASES[i].duration;
                break;
            }
        }
        if (chosen == AFFECT_NONE) {
            descriptor_send(d,
                "Syntax: egotrip disease <target> <disease>\r\n"
                "Viable cruelties include: cold, dysentery, flu, pneumonia, leprosy,\r\n"
                "gangrene, plague, scurvy\r\n");
            return true;
        }
        if (being_has_affect(target, chosen)) {
            descriptor_send(d, "Bummer, they already have that one.\r\n");
            return true;
        }

        being_apply_affect(target, chosen, duration);
        char out[200];
        snprintf(out, sizeof(out), "You breathe a fetid cloud into %s's body.\r\n", target->base.name);
        descriptor_send(d, out);
        if (target->desc)
            descriptor_notify(target->desc, "Someone around here doesn't like you.\r\n");
        if (target->base.roomp) {
            snprintf(out, sizeof(out), "Someone around here doesn't like %s.\r\n", target->base.name);
            descriptor_room_echo(target->base.roomp, target, out);
        }
        return true;
    }

    if (strcasecmp(sub, "cleanse") == 0) {
        int count = 0;
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            being_t *b = it->character;
            if (!b)
                continue;
            for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
                affect_type_t t = b->affects[i].type;
                if (t == AFFECT_NONE)
                    continue;
                if (!affect_is_disease(t) && t != AFFECT_POISON)
                    continue;
                being_remove_affect(b, t);
                count++;
                if (b->desc) {
                    char out[128];
                    snprintf(out, sizeof(out), "A wave of divine power cures your %s.\r\n", affect_name(t));
                    descriptor_notify(b->desc, out);
                }
            }
        }
        char out[128];
        snprintf(out, sizeof(out), "You cleanse the world of disease -- %d affliction%s cured.\r\n",
                 count, count == 1 ? "" : "s");
        descriptor_send(d, out);
        return true;
    }

    if (strcasecmp(sub, "stupidity") == 0) {
        int count = 0;
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            being_t *b = it->character;
            if (!b || being_is_immortal(b))
                continue;
            /* Same penalty formula as cmd_cast.c's stupidity branch
             * (sk->min_level / 4, floored at 1) -- min_level 1 there for
             * a generically-cast egotrip affliction, same 100-round
             * duration. */
            being_apply_stat_affect(b, AFFECT_STUPIDITY, 100, -1);
            count++;
            if (b->desc)
                descriptor_notify(b->desc, "Your mind fogs with stupidity!\r\n");
        }
        char out[128];
        snprintf(out, sizeof(out), "You reconfirm %d mortal%s suspicions.\r\n",
                 count, count == 1 ? "'s" : "s'");
        descriptor_send(d, out);
        return true;
    }

    if (strcasecmp(sub, "wander") == 0) {
        if (!ch->base.roomp) {
            descriptor_send(d, "You are nowhere.\r\n");
            return true;
        }
        int count = 0;
        thing_t *next;
        for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = next) {
            next = t->stuff_next; /* mob_ai_force_wander() may move `t` out of this room */
            if (t->kind != THING_MOB)
                continue;
            mob_ai_force_wander((being_t *)t);
            count++;
        }
        char out[128];
        snprintf(out, sizeof(out), "You force a wander pulse for %d mob%s in the room.\r\n",
                 count, count == 1 ? "" : "s");
        descriptor_send(d, out);
        return true;
    }

    descriptor_send(d,
        "Unrecognized egotrip subcommand. Syntax: egotrip <subcommand>\r\n"
        "See HELP EGOTRIP for the full list.\r\n");
    return true;
}
