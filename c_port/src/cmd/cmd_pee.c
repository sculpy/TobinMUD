/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "log.h"
#include "obj.h"

/* Liquid types `pee <type>` can leave a puddle of, beyond plain pee itself
 * -- prefix-matched, abbreviations welcome (same convention as every other
 * command). Extend this list freely; obj.c's pool_noun_color() already
 * falls back to plain white for any noun it doesn't specifically color, so
 * a new entry here needs no other code changes unless it wants its own
 * color too. */
typedef struct {
    const char *name;
    const char *keywords;
} pee_liquid_t;

static const pee_liquid_t PEE_LIQUIDS[] = {
    { "pee",   "puddle pool pee urine" },
    { "water", "puddle pool water" },
    { "wine",  "puddle pool wine" },
    { "beer",  "puddle pool beer" },
    { "acid",  "puddle pool acid" },
};
#define PEE_LIQUID_COUNT (int)(sizeof(PEE_LIQUIDS) / sizeof(PEE_LIQUIDS[0]))

/* `pee` (user, 2026-07-11: "add pools and the pee command for 51"; user
 * 2026-07-17: "pee should be able to pee liquid types, pee defaults to
 * pee, pee <arg> tries to find a matching liquid type and leave a puddle
 * of that liquid type"). A flavor command, immortal-only
 * (IMMORTAL_LEVEL_MIN) like the other one-off fun/utility commands added
 * this session -- leaves a non-takeable puddle (obj_grow_pool(), obj.c) on
 * the floor of the caller's room. A second `pee` of the SAME liquid in the
 * same room grows the existing puddle into a bigger pool instead of adding
 * a separate object (user, 2026-07-11: "pools should grow in size if
 * multiple puddles of the same type are created in a room") -- a
 * DIFFERENT liquid starts its own puddle alongside it, since
 * obj_grow_pool() only merges into a puddle whose keywords already match
 * the requested type_tag. */
bool cmd_pee(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    const pee_liquid_t *liquid = &PEE_LIQUIDS[0]; /* bare pee defaults to pee */
    if (*args) {
        size_t alen = strlen(args);
        liquid = NULL;
        for (int i = 0; i < PEE_LIQUID_COUNT; i++) {
            if (strncasecmp(PEE_LIQUIDS[i].name, args, alen) == 0) {
                liquid = &PEE_LIQUIDS[i];
                break;
            }
        }
        if (!liquid) {
            char msg[192];
            int n = snprintf(msg, sizeof(msg), "You don't know how to pee '%s'. Try: ", args);
            for (int i = 0; i < PEE_LIQUID_COUNT && n < (int)sizeof(msg); i++)
                n += snprintf(msg + n, sizeof(msg) - (size_t)n, "%s%s",
                              PEE_LIQUIDS[i].name, i + 1 < PEE_LIQUID_COUNT ? ", " : ".\r\n");
            descriptor_send(d, msg);
            return true;
        }
    }

    obj_grow_pool(d->character->base.roomp, liquid->name, liquid->keywords, liquid->name);

    bool plain_pee = strcmp(liquid->name, "pee") == 0;
    char you_msg[80];
    if (plain_pee)
        snprintf(you_msg, sizeof(you_msg), "You relieve yourself. Ahh, much better.\r\n");
    else
        snprintf(you_msg, sizeof(you_msg), "You concentrate and produce a puddle of %s.\r\n", liquid->name);
    descriptor_send(d, you_msg);

    char msg[128];
    if (plain_pee)
        snprintf(msg, sizeof(msg), "%s relieves %s on the floor.\r\n",
                 d->character->base.name, "themselves");
    else
        snprintf(msg, sizeof(msg), "%s produces a puddle of %s on the floor.\r\n",
                 d->character->base.name, liquid->name);
    descriptor_room_echo(d->character->base.roomp, d->character, msg);

    game_log(LOG_EDIT, "%s left a puddle of %s in room %d. [%s]",
             d->character->base.name, liquid->name, d->character->base.roomp->vnum,
             descriptor_display_host(d));
    return true;
}
