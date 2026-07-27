/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "account.h"
#include "being.h"
#include "config.h"
#include "log.h"
#include "player_repo.h"
#include "thing.h"

/* `wipe <name> <password>` / `wipe account <name> <password>` --
 * Administrator-tier (59+), the most destructive command there is short
 * of `shutdown`: permanently erases a character or an entire account.
 * Ported from the original's TBeing::doWipe() (misc/immortal.cc), with
 * one deliberate departure -- the original checked the typed password
 * against a literal hardcoded string ("ole'chicken"). User, 2026-07-17:
 * "`wipe` command + a real (non-hardcoded) master password". Tobin reads
 * it from TOBIN_WIPE_PASSWORD at process start (config.c) and refuses
 * outright if that's unset -- no fallback value lives in this source at
 * all. Same level-hierarchy rule as the original ("You can only banish
 * players less than your level"): the target (or, for an account, its
 * HIGHEST-level character) must be strictly below the caller's own
 * level, which also naturally blocks wiping yourself. Every table hung
 * off `player`/`account` (player_progress, player_attrs,
 * player_inventory, and a couple dozen more, see the FK survey behind
 * this) cascades on delete, so a single player_delete()/account_delete()
 * call is genuinely everything -- no manual per-table cleanup. */

/* Fully removes an online victim from the live world: drops their
 * carried/worn/held items loose on the room floor (matching the
 * original's dropItemsToRoom, not destroying them the way a corpse
 * would), announces it, then tears down the being and its connection.
 * Nulling victim_d->character before descriptor_destroy() skips its
 * link-drop path entirely (no "has lost his link" message, no leaving a
 * linkdead body standing around waiting for a reconnect that will never
 * come -- there's no row left to reconnect to). */
static void wipe_online_victim(descriptor_t *victim_d) {
    being_t *v = victim_d->character;

    descriptor_send(victim_d, "\r\n<r>*** You have been wiped from existence. ***<z>\r\n");
    descriptor_flush_output(victim_d);

    if (v->base.roomp) {
        thing_t *t = v->base.stuff_head;
        while (t) {
            thing_t *next = t->stuff_next;
            if (t->kind == THING_OBJ)
                thing_move_to(t, &v->base.roomp->base);
            t = next;
        }
        char msg[200];
        snprintf(msg, sizeof(msg), "%s vanishes in a puff of smoke, %s belongings crashing to the ground!\r\n",
                 being_display_name(v), gender_possess(v->gender));
        descriptor_room_echo(v->base.roomp, v, msg);
    }

    being_destroy(v); /* stuff_head is empty now -- just detaches + frees */
    victim_d->character = NULL;
    descriptor_destroy(victim_d);
}

/* `wipe <name> <password>` / `wipe account <name> <password>` command --
 * see file-top comment for the full destructive-power rationale.
 * Validates the master wipe password from config, enforces the "target
 * must be strictly below your level" rule (checking the account's
 * highest-level character in account mode), tears down any online
 * victim(s) via wipe_online_victim(), then deletes the player/account
 * row outright (cascading through every dependent table). */
bool cmd_wipe(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok1[64] = "", tok2[64] = "", tok3[64] = "";
    int n = sscanf(args, "%63s %63s %63s", tok1, tok2, tok3);

    bool is_account;
    const char *target_name, *password;
    if (n == 3 && strcasecmp(tok1, "account") == 0) {
        is_account = true;
        target_name = tok2;
        password = tok3;
    } else if (n == 2) {
        is_account = false;
        target_name = tok1;
        password = tok2;
    } else {
        descriptor_send(d, "Usage: wipe <name> <password>\r\n       wipe account <name> <password>\r\n");
        return true;
    }

    const config_t *cfg = config_get();
    if (!cfg->wipe_password || !*cfg->wipe_password) {
        descriptor_send(d, "wipe is disabled -- no master password is configured.\r\n");
        return true;
    }
    if (strcmp(password, cfg->wipe_password) != 0) {
        descriptor_send(d, "Wrong wipe password.\r\n");
        return true;
    }

    if (is_account) {
        account_t acct;
        if (!account_load(target_name, &acct)) {
            descriptor_send(d, "No such account.\r\n");
            return true;
        }

        char names[MAX_CHARS_PER_ACCOUNT][PLAYER_NAME_LEN];
        int levels[MAX_CHARS_PER_ACCOUNT];
        int count = 0;
        player_list_by_account(acct.account_id, names, levels, MAX_CHARS_PER_ACCOUNT, &count);

        int highest = 0;
        for (int i = 0; i < count; i++) {
            if (levels[i] > highest)
                highest = levels[i];
        }
        if (highest >= ch->progress.level) {
            descriptor_send(d, "You can only wipe accounts whose characters are all below your level.\r\n");
            return true;
        }

        descriptor_t *it = g_descriptors;
        while (it) {
            descriptor_t *next = it->next;
            if (it->character && it->account.account_id == acct.account_id)
                wipe_online_victim(it);
            it = next;
        }

        if (!account_delete(acct.account_id)) {
            descriptor_send(d, "Database delete failed; aborting wipe.\r\n");
            return true;
        }

        char msg[160];
        snprintf(msg, sizeof(msg), "The account '%s' (%d character%s) has been wiped from existence.\r\n",
                 target_name, count, count == 1 ? "" : "s");
        descriptor_send(d, msg);
        game_log(LOG_GAME, "%s wiped account '%s' (%d character(s)).",
                 ch->base.name, target_name, count);
    } else {
        long target_pid = player_id_for_name(target_name);
        long target_account_id = player_account_id_for_name(target_name);
        if (target_pid < 0 || target_account_id < 0) {
            descriptor_send(d, "No such player.\r\n");
            return true;
        }

        progress_t prog;
        int target_level = player_progress_load(target_pid, &prog) ? prog.level : 0;
        if (target_level >= ch->progress.level) {
            descriptor_send(d, "You can only wipe players below your level.\r\n");
            return true;
        }

        descriptor_t *victim_d = NULL;
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (it->character && strcasecmp(it->character->base.name, target_name) == 0) {
                victim_d = it;
                break;
            }
        }
        if (victim_d)
            wipe_online_victim(victim_d);

        if (!player_delete(target_name, target_account_id)) {
            descriptor_send(d, "Database delete failed; aborting wipe.\r\n");
            return true;
        }

        char msg[160];
        snprintf(msg, sizeof(msg), "%s has been wiped from existence.\r\n", target_name);
        descriptor_send(d, msg);
        game_log(LOG_GAME, "%s wiped player '%s'.", ch->base.name, target_name);
    }

    return true;
}
