/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "player_repo.h"
#include "shop_repo.h"
#include "thing.h"
#include "treasury.h"
#include "treasury_repo.h"

/* Money system v2 (Sneezy → Tobin feature audit, "Money system v2
 * (banking/taxes)"). Checked the real upstream first
 * (spec/spec_mobs_banker.cc, misc/shopowned.cc/shopaccounting.cc,
 * docs/systems/critical/17-economy-system.md): per-shop bank accounts, a
 * fractional-reserve central bank, and player-owned-shop sales tax
 * posted through a full double-entry ledger. All entangled with a
 * player-owned-shop economy Tobin doesn't have. Scoped Tobin-scale
 * instead, confirmed with the user via AskUserQuestion 2026-07-21: ONE
 * global bank (`player_progress.bank_gold`, not per-shop accounts) and
 * tax revenue collects into a single visible treasury (see
 * tobin_migrations.sql for the full writeup and cmd_shop.c's cmd_buy()
 * for where the tax itself is charged). */

/* Same "find the shop operating in this room" pattern cmd_shop.c's own
 * (file-local) find_active_shop() and cmd_repair.c's find_repair_shop()
 * already duplicate -- extended with a shop_repo_is_bank() check. */
static being_t *find_bank(room_t *room, shop_t *shop) {
    if (!room || !shop_repo_find_by_room(room->vnum, shop) || !shop_repo_is_bank(shop->shop_nr))
        return NULL;
    for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_MOB && t->id == shop->keeper)
            return (being_t *)t;
    }
    return NULL;
}

/* The `bank` command: balance/deposit/withdraw against the player's single
 * global bank_gold balance, routed through whatever bank keeper is standing
 * in the room (see find_bank() above). No bank in the room bounces the
 * player before any of the sub-verbs are parsed. */
bool cmd_bank(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp)
        return true;

    shop_t shop;
    being_t *keeper = find_bank(ch->base.roomp, &shop);
    if (!keeper) {
        descriptor_send(d, "You don't see a bank here.\r\n");
        return true;
    }

    char verb[32] = "";
    char rest[32] = "";
    sscanf(args, "%31s %31s", verb, rest);

    if (!*verb || strncasecmp(verb, "balance", strlen(verb)) == 0) {
        char msg[160];
        snprintf(msg, sizeof(msg), "You have %d gold in your wallet and %d gold in the bank.\r\n",
                 ch->progress.gold, ch->progress.bank_gold);
        descriptor_send(d, msg);
        return true;
    }

    bool depositing;
    if (strncasecmp(verb, "deposit", strlen(verb)) == 0)
        depositing = true;
    else if (strncasecmp(verb, "withdraw", strlen(verb)) == 0)
        depositing = false;
    else {
        descriptor_send(d, "Usage: bank [balance | deposit <amount> | withdraw <amount>]\r\n");
        return true;
    }

    if (!*rest || !isdigit((unsigned char)rest[0])) {
        descriptor_send(d, "How much?\r\n");
        return true;
    }
    int amount = atoi(rest);
    if (amount <= 0) {
        descriptor_send(d, "That's not a sensible amount.\r\n");
        return true;
    }

    char capbuf[128];
    char msg[160];
    if (depositing) {
        if (ch->progress.gold < amount) {
            descriptor_send(d, "You don't have that much gold to deposit.\r\n");
            return true;
        }
        ch->progress.gold -= amount;
        ch->progress.bank_gold += amount;
        snprintf(msg, sizeof(msg), "%s counts %d gold into your account.\r\n",
                 being_display_name_cap(keeper, capbuf, sizeof(capbuf)), amount);
    } else {
        if (ch->progress.bank_gold < amount) {
            descriptor_send(d, "You don't have that much in the bank.\r\n");
            return true;
        }
        ch->progress.bank_gold -= amount;
        ch->progress.gold += amount;
        snprintf(msg, sizeof(msg), "%s counts %d gold out for you.\r\n",
                 being_display_name_cap(keeper, capbuf, sizeof(capbuf)), amount);
    }
    descriptor_send(d, msg);

    player_progress_save(ch->player_id, &ch->progress);
    return true;
}

/* The `treasury` command: reports the crown's single visible tax-revenue
 * pot (see the money-system-v2 note at the top of this file for why it's
 * one shared treasury rather than per-shop ledgers). */
bool cmd_treasury(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    char first[16] = "";
    sscanf(args, "%15s", first);
    /* `treasury allocate` (immortal): force the monthly improvement-projects
     * spend now, rather than waiting for the game-month rollover. Same spend
     * + world announcement the automatic tick does (treasury.c). */
    if (first[0] && strncasecmp(first, "allocate", strlen(first)) == 0) {
        if (!ch || !being_is_immortal(ch)) {
            descriptor_send(d, "Only an immortal may allocate the treasury.\r\n");
            return true;
        }
        int spent = treasury_spend_monthly_improvements();
        char msg[160];
        if (spent > 0)
            snprintf(msg, sizeof(msg),
                     "You allocate the improvement-projects budget: %d gold spent, %d gold remains.\r\n",
                     spent, treasury_repo_get_gold());
        else
            snprintf(msg, sizeof(msg), "The treasury is empty -- nothing to allocate.\r\n");
        descriptor_send(d, msg);
        return true;
    }
    char msg[96];
    snprintf(msg, sizeof(msg), "The crown's treasury holds %d gold in collected taxes.\r\n",
             treasury_repo_get_gold());
    descriptor_send(d, msg);
    return true;
}
