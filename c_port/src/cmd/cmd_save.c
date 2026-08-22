/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include "player_repo.h"

/* `save`: persists everything about your character on demand (user
 * request, 2026-07-07: "add a save command to manually save your
 * character"). A thin wrapper around player_save() (player_repo.c),
 * which is also called automatically on quit/death
 * (descriptor_leave_to_menu(), descriptor.c) -- this command exists for
 * a player who wants the reassurance of doing it themselves mid-session,
 * mirroring the original's real `save` command. */
bool cmd_save(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (player_save(ch->player_id, ch))
        descriptor_send(d, "Saved.\r\n");
    else
        descriptor_send(d, "Save failed -- the database is unavailable.\r\n");
    return true;
}
