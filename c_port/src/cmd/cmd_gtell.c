/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "being.h"
#include "descriptor.h"

/* `gtell`/`gt <message>` (TODO.md priority item, user 2026-07-30: "Group
 * tell command with `gt` alias"). Broadcasts to every GROUPED member of
 * `ch`'s group (being_group_members() -- the leader plus every follower
 * with `grouped` set, see cmd_group.c's own file-top comment on why
 * that's the right membership test here rather than plain `follow`:
 * `group`'s whole point is opting a follower INTO shared benefits/
 * communication, same reasoning `split`'s gold-sharing already uses).
 * No ignore-list/PLR_NOTELL gating -- unlike a direct `tell`, you're
 * already in this conversation by mutual group consent, so there's
 * nothing to opt out of short of leaving the group itself. Reaches
 * everyone regardless of room (a real party-wide channel, not
 * room-scoped like `say`). */
bool cmd_gtell(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Group-tell what?\r\n");
        return true;
    }
    if (!ch->grouped) {
        descriptor_send(d, "You aren't in a group.\r\n");
        return true;
    }

    being_t *members[GROUP_MAX_FOLLOWERS + 1];
    int n = being_group_members(ch, members, GROUP_MAX_FOLLOWERS + 1);

    char out[400];
    snprintf(out, sizeof(out), "<p>You group-tell, \"<z>%s<p>\"<z>\r\n", args);
    descriptor_send(d, out);

    snprintf(out, sizeof(out), "<p>%s group-tells, \"<z>%s<p>\"<z>\r\n", ch->base.name, args);
    for (int i = 0; i < n; i++) {
        if (members[i] == ch || !members[i]->desc)
            continue;
        descriptor_notify(members[i]->desc, out);
    }
    return true;
}
