/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "player_repo.h"
#include "room.h"
#include "thing.h"

/* `follow`/`stop`/`group`/`split` (Sneezy → Tobin feature audit, "Group /
 * party system"). See being.h's `master`/`followers`/`grouped` field
 * comment for the real Sneezy behavior this is scoped down from (a
 * per-player money-share factor, leader succession on death) and why.
 * `follow` establishes the master/follower relationship alone -- it does
 * NOT grant group benefits, matching the original exactly: the leader
 * still has to `group <name>` each follower in. */

static being_t *find_target_in_room(being_t *ch, const char *tok) {
    const char *rest;
    int ordinal = thing_parse_ordinal(tok, &rest);
    size_t len = strlen(rest);
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if ((t->kind != THING_PC && t->kind != THING_MOB) || t == &ch->base)
            continue;
        if (thing_name_matches(t->name, rest, len)) {
            seen++;
            if (seen == ordinal)
                return (being_t *)t;
        }
    }
    return NULL;
}

bool cmd_follow(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char tok[64] = "";
    if (sscanf(args, "%63s", tok) != 1) {
        descriptor_send(d, "Follow whom?\r\n");
        return true;
    }

    being_t *target = find_target_in_room(ch, tok);
    if (!target) {
        descriptor_send(d, "You don't see them here.\r\n");
        return true;
    }
    if (target->master == ch) {
        descriptor_send(d, "They're following YOU -- that would be a circular chain.\r\n");
        return true;
    }

    int slot = -1;
    for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
        if (target->followers[i] == ch)
            slot = -2; /* already following them */
        else if (slot == -1 && !target->followers[i])
            slot = i;
    }
    if (slot == -2) {
        descriptor_send(d, "You're already following them.\r\n");
        return true;
    }
    if (slot == -1) {
        descriptor_send(d, "Their group is full.\r\n");
        return true;
    }

    if (ch->master)
        being_leave_group(ch); /* switching masters -- detach from the old one first */

    ch->master = target;
    target->followers[slot] = ch;

    char msg[128];
    snprintf(msg, sizeof(msg), "You now follow %s.\r\n", being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s starts following you.\r\n", being_display_name(ch));
        descriptor_notify(target->desc, msg);
    }
    return true;
}

bool cmd_stop(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (!ch->master) {
        descriptor_send(d, "You aren't following anyone.\r\n");
        return true;
    }

    being_t *master = ch->master;
    for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
        if (master->followers[i] == ch) {
            master->followers[i] = NULL;
            break;
        }
    }
    ch->master = NULL;
    ch->grouped = false;

    char msg[128];
    snprintf(msg, sizeof(msg), "You stop following %s.\r\n", being_display_name(master));
    descriptor_send(d, msg);
    if (master->desc) {
        snprintf(msg, sizeof(msg), "%s stops following you.\r\n", being_display_name(ch));
        descriptor_notify(master->desc, msg);
    }
    return true;
}

/* `dismiss` -- releases a charmed pet early (Pet/charm, Sneezy → Tobin
 * feature audit) rather than waiting out its AFFECT_CHARMED duration.
 * The MASTER's side of the relationship (`stop` above is the opposite:
 * a follower leaving THEIR OWN master) -- no real Sneezy command to port
 * (its charm affects just expire), a small Tobin-scale addition so a
 * player isn't stuck carrying an inconvenient pet (e.g. into a
 * ROOM_FLAG_NO_MOB area) until the timer runs out. */
bool cmd_dismiss(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    being_t *pet = being_find_charmed_pet(ch);
    if (!pet) {
        descriptor_send(d, "You have no charmed creature to dismiss.\r\n");
        return true;
    }

    char capbuf[128], msg[224];
    being_display_name_cap(pet, capbuf, sizeof(capbuf));
    snprintf(msg, sizeof(msg), "You release %s from your service, and it fades away.\r\n", capbuf);
    descriptor_send(d, msg);
    if (pet->base.roomp) {
        snprintf(msg, sizeof(msg), "%s fades away.\r\n", capbuf);
        descriptor_room_echo(pet->base.roomp, NULL, msg);
    }
    being_destroy(pet);
    return true;
}

bool cmd_group(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    char tok[64] = "";
    sscanf(args, "%63s", tok);

    if (!tok[0]) {
        /* Not grouped: `ch` is a leaderless solo with no followers either.
         * Otherwise show the listing -- a leader with un-grouped followers
         * can still see who's following before `group`ing them in, and a
         * follower can see whether they've actually been grouped in yet. */
        being_t *leader = ch->master ? ch->master : ch;
        bool any_followers = false;
        for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++)
            if (leader->followers[i])
                any_followers = true;
        if (!ch->master && !leader->grouped && !any_followers) {
            descriptor_send(d, "You aren't in a group.\r\n");
            return true;
        }
        char out[1024];
        int len = snprintf(out, sizeof(out), "Group:\r\n");
        len += snprintf(out + len, sizeof(out) - (size_t)len, "  %s (leader)%s\r\n",
                        being_display_name(leader), leader->grouped ? "" : " -- not grouped in yet");
        for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
            being_t *f = leader->followers[i];
            if (!f)
                continue;
            len += snprintf(out + len, sizeof(out) - (size_t)len, "  %s%s\r\n",
                            being_display_name(f), f->grouped ? "" : " -- following, not grouped in yet");
        }
        descriptor_send(d, out);
        return true;
    }

    if (ch->master) {
        descriptor_send(d, "Only the group leader can do that -- you're following someone else.\r\n");
        return true;
    }

    if (strcasecmp(tok, "all") == 0) {
        int n = 0;
        for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
            if (!ch->followers[i])
                continue;
            ch->followers[i]->grouped = true;
            n++;
        }
        if (n == 0) {
            descriptor_send(d, "You have no followers to group in.\r\n");
            return true;
        }
        ch->grouped = true;
        char msg[64];
        snprintf(msg, sizeof(msg), "You group in all %d follower%s.\r\n", n, n == 1 ? "" : "s");
        descriptor_send(d, msg);
        return true;
    }

    being_t *target = NULL;
    for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
        if (ch->followers[i] && strncasecmp(ch->followers[i]->base.name, tok, strlen(tok)) == 0) {
            target = ch->followers[i];
            break;
        }
    }
    if (!target) {
        descriptor_send(d, "They aren't following you -- `follow` only works the other way around.\r\n");
        return true;
    }

    target->grouped = true;
    ch->grouped = true;
    char msg[128];
    snprintf(msg, sizeof(msg), "You group in %s.\r\n", being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s groups you in.\r\n", being_display_name(ch));
        descriptor_notify(target->desc, msg);
    }
    return true;
}

bool cmd_split(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (ch->master) {
        descriptor_send(d, "Only the group leader can split gold.\r\n");
        return true;
    }
    if (!ch->grouped) {
        descriptor_send(d, "You aren't leading a group.\r\n");
        return true;
    }

    int amount = 0;
    if (sscanf(args, "%d", &amount) != 1 || amount <= 0) {
        descriptor_send(d, "Usage: split <amount>\r\n");
        return true;
    }
    if (amount > ch->progress.gold) {
        descriptor_send(d, "You don't have that much gold.\r\n");
        return true;
    }

    /* Recipients: the leader plus every grouped member physically present
     * in this room (Sneezy's own spatial rule -- "Only characters in same
     * room as combat receive experience," applied here to money too). */
    being_t *members[GROUP_MAX_FOLLOWERS + 1];
    int total = being_group_members(ch, members, GROUP_MAX_FOLLOWERS + 1);
    being_t *present[GROUP_MAX_FOLLOWERS + 1];
    int n = 0;
    for (int i = 0; i < total; i++)
        if (members[i]->base.roomp == ch->base.roomp)
            present[n++] = members[i];
    if (n == 0) {
        descriptor_send(d, "There's no one here to split with.\r\n");
        return true;
    }

    int share = amount / n;
    if (share < 1) {
        descriptor_send(d, "That's not enough to split evenly.\r\n");
        return true;
    }
    int spent = share * n;
    ch->progress.gold -= spent;

    char out[96];
    for (int i = 0; i < n; i++) {
        present[i]->progress.gold += share;
        if (present[i]->base.kind == THING_PC)
            player_progress_save(present[i]->player_id, &present[i]->progress);
        if (present[i]->desc) {
            snprintf(out, sizeof(out), "You receive %d gold from the split.\r\n", share);
            descriptor_notify(present[i]->desc, out);
        }
    }
    snprintf(out, sizeof(out), "You split %d gold %d ways (%d each).\r\n", spent, n, share);
    descriptor_send(d, out);
    return true;
}
