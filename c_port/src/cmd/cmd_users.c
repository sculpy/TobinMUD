#include "cmd_internal.h"

#include <stdio.h>

/* `users` (58+): every live connection -- who (account/character), where
 * from (IP), and what connection state they're in (the classic Diku
 * admin roster; user request, Session 21). Mid-editor players are marked,
 * since that explains an otherwise unresponsive-looking character. */

static const char *state_name(const descriptor_t *d) {
    if (d->state == CONN_PLAYING && d->edit_kind != EDIT_NONE)
        return "playing (in editor)";
    switch (d->state) {
        case CONN_GET_ACCOUNT_NAME:   return "at login (account name)";
        case CONN_GET_PASSWORD:       return "at login (password)";
        case CONN_GET_NEW_PASSWORD:   return "at login (new password)";
        case CONN_CONFIRM_PASSWORD:  return "at login (confirming password)";
        case CONN_ACCOUNT_MENU:       return "at the account menu";
        case CONN_CHAR_CREATE_NAME:   return "creating (name)";
        case CONN_CHAR_CREATE_ATTRS:  return "creating (attributes)";
        case CONN_CHAR_DELETE_CONFIRM:return "confirming a delete";
        case CONN_PLAYING:            return "playing";
        default:                      return "closing";
    }
}

bool cmd_users(descriptor_t *d, const char *args) {
    (void)args;

    char out[2048];
    int n = snprintf(out, sizeof(out),
                     "\r\n%-15s %-15s %-16s %s\r\n",
                     "Character", "Account", "IP", "State");
    int count = 0;
    for (descriptor_t *it = g_descriptors; it && (size_t)n < sizeof(out) - 96; it = it->next) {
        count++;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "%-15s %-15s %-16s %s\r\n",
                      it->character ? it->character->base.name : "-",
                      it->account.name[0] ? it->account.name : "-",
                      it->ip[0] ? it->ip : "?",
                      state_name(it));
    }
    if ((size_t)n < sizeof(out))
        snprintf(out + n, sizeof(out) - (size_t)n, "%d connection%s.\r\n",
                 count, count == 1 ? "" : "s");
    descriptor_send(d, out);
    return true;
}
