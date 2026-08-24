/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "affect.h"
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

/* `follow <name>` -- attaches `ch` as a follower of `target` (master/
 * follower link only, see the file's top comment on why this alone
 * doesn't grant group benefits). Refuses a circular chain and switches
 * cleanly off any existing master first. */
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

/* `stop` -- the follower's own side of ending a follow relationship
 * (detaches from `ch->master` and clears `grouped`); the opposite
 * direction from cmd_dismiss(), which is the master releasing a pet. */
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

/* Visibility gate for grouping candidates -- mirrors the immortal-viewer/
 * detect-invisible exceptions cmd_look.c's room listing already uses for
 * AFFECT_INVISIBLE and `hide` (cmd_hide.c's `hiding` flag): the group
 * leader can't grab someone into their group that they can't actually
 * see, matching SneezyMUD's doGroup() `canSee(victim)` guard. */
static bool group_can_see(const being_t *viewer, const being_t *target) {
    if (being_has_affect(target, AFFECT_INVISIBLE) && !being_is_immortal(viewer)
        && !being_has_affect(viewer, AFFECT_DETECT_INVISIBLE))
        return false;
    if (target->hiding && !being_is_immortal(viewer)
        && !being_has_affect(viewer, AFFECT_DETECT_INVISIBLE))
        return false;
    return true;
}
/* True if `candidate` is an immortal-level NPC follower of `leader` --
 * SneezyMUD's doGroup() refuses to group these in ("is immortal and has
 * no need of you"); an immortal-level mob following someone is a
 * builder/staff tool (a summoned helper, a test pet), not a real group
 * member. */
static bool group_is_immortal_npc_follower(const being_t *leader, const being_t *candidate) {
    return candidate->base.kind == THING_MOB && being_is_immortal(candidate)
        && candidate->master == leader && leader != candidate;
}
/* `group` -- with no argument, shows the leader/follower listing and who's
 * actually grouped in yet; otherwise the leader grants group benefits to
 * one follower (or `group all`) by setting their `grouped` flag. Only the
 * leader (someone with no master of their own) can grant it. */
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
            being_t *cand = ch->followers[i];
            if (!cand)
                continue;
            if (cand->grouped)
                continue; /* already group-flagged -- nothing to do */
            if (!group_can_see(ch, cand))
                continue; /* can't grab someone you can't see into the group */
            if (cand == ch->mount)
                continue; /* your own mount is never a group candidate */
            if (group_is_immortal_npc_follower(ch, cand)) {
                char imsg[128];
                snprintf(imsg, sizeof(imsg), "%s is immortal and has no need of you. %s does not join your group.\r\n",
                        being_display_name(cand), being_display_name(cand));
                descriptor_send(d, imsg);
                continue;
            }
            cand->grouped = true;
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

    /* Self-target (leader typing their own name) toggles the leader
     * flag itself; SneezyMUD lets `group <own name>` resolve to `this`
     * via get_char_room_vis() the same way. */
    bool self_target = strncasecmp(ch->base.name, tok, strlen(tok)) == 0;
    being_t *target = self_target ? ch : NULL;
    if (!target) {
        for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
            if (ch->followers[i] && strncasecmp(ch->followers[i]->base.name, tok, strlen(tok)) == 0) {
                target = ch->followers[i];
                break;
            }
        }
    }
    if (!target) {
        descriptor_send(d, "They aren't following you -- `follow` only works the other way around.\r\n");
        return true;
    }

    char msg[128];
    if (target == ch) {
        /* `group <name>` is a toggle -- ungrouping the leader (self)
         * disbands the whole group, clearing `grouped` on every
         * follower too, matching SneezyMUD's doGroup(). */
        if (ch->grouped) {
            ch->grouped = false;
            for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
                being_t *f = ch->followers[i];
                if (!f || !f->grouped)
                    continue;
                f->grouped = false;
                if (f->desc)
                    descriptor_notify(f->desc, "The group has been disbanded.\r\n");
            }
            descriptor_send(d, "You ungroup yourself, causing the rest of the group to be ungrouped.\r\n");
        } else {
            ch->grouped = true;
            descriptor_send(d, "You group yourself.\r\n");
        }
        return true;
    }
    if (target->grouped) {
        /* Toggle: already grouped in, so `group <name>` ungroups them.
         * Blocked while they're fighting, matching SneezyMUD's
         * doGroup() `victim->fight()` guard. */
        if (target->fighting) {
            descriptor_send(d, "You can't ungroup them while they're fighting.\r\n");
            return true;
        }
        target->grouped = false;
        snprintf(msg, sizeof(msg), "You ungroup %s.\r\n", being_display_name(target));
        descriptor_send(d, msg);
        if (target->desc) {
            snprintf(msg, sizeof(msg), "You are no longer a member of %s's group!\r\n", being_display_name(ch));
            descriptor_notify(target->desc, msg);
        }
        return true;
    }
    if (!group_can_see(ch, target)) {
        descriptor_send(d, "You don't see them here.\r\n");
        return true;
    }
    if (target == ch->mount) {
        descriptor_send(d, "You can't group your own mount.\r\n");
        return true;
    }
    if (group_is_immortal_npc_follower(ch, target)) {
        snprintf(msg, sizeof(msg), "%s is immortal and has no need of you. %s does not join your group.\r\n",
                being_display_name(target), being_display_name(target));
        descriptor_send(d, msg);
        return true;
    }
    target->grouped = true;
    ch->grouped = true;
    snprintf(msg, sizeof(msg), "You group in %s.\r\n", being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s groups you in.\r\n", being_display_name(ch));
        descriptor_notify(target->desc, msg);
    }
    return true;
}

/* `split <amount>` -- the group leader divides gold evenly among every
 * grouped member physically present in the room (Sneezy's same-room rule
 * for shared rewards, applied to money here). Rounds down; leftover gold
 * from an uneven split stays with the leader. */
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
