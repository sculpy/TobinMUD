/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_CMD_H
#define TOBIN_CMD_H

#include <stdbool.h>

struct descriptor;

/* Parses the first word of `line` as a command verb and dispatches to the
 * matching handler (see src/cmd/cmd_table.c), or sends an "unknown
 * command" reply if there's no match. Returns false if the command (e.g.
 * `quit`) requested the connection be closed, true otherwise. */
bool cmd_dispatch(struct descriptor *d, const char *line);

#endif
