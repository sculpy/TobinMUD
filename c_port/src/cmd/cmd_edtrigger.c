/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "room_repo.h"
#include "trigger_repo.h"
#include "zone.h"

#define TRIGGER_DEFAULT_CHANCE_PCT 25

/* `edit trigger <target_type> <vnum> <trigger_type> [match_text|chance]`
 * (user, 2026-07-11: "implement mob object and room scripting ...
 * interaction with mobs objs and room via scripts") -- the in-game-
 * authorable alternative to SneezyMUD's hardcoded spec procs, see
 * db/sneezy/trigger.sql's header comment. Captures the header fields,
 * then arms the shared line editor for the script body itself (same
 * .../s save shape as `edit news`/`edit rules`) -- see trigger.h for the
 * fixed action vocabulary and descriptor.c's EDIT_TRIGGER save case.
 *
 * Two read-only sub-forms share this entry point:
 *   edit trigger list <target_type> <vnum>  -- shows what's already there
 *   edit trigger delete <id>                -- removes one
 */
bool cmd_edtrigger(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char usage[] =
        "Usage: edit trigger <room|mob|obj> <vnum> <trigger_type> [match_text|chance]\r\n"
        "       edit trigger list <room|mob|obj> <vnum>\r\n"
        "       edit trigger delete <id>\r\n"
        "Trigger types: room: enter, random -- mob: greet, speech, death, random -- "
        "obj: get, wear\r\n";

    char a[32], b[64], c[32], rest[TRIGGER_MATCH_LEN];
    rest[0] = '\0';
    int got = sscanf(args, "%31s %63s %31s %63s", a, b, c, rest);

    if (got >= 2 && strcasecmp(a, "delete") == 0) {
        long id = atol(b);
        if (trigger_repo_delete(id))
            descriptor_send(d, "Trigger deleted.\r\n");
        else
            descriptor_send(d, "No trigger has that id.\r\n");
        return true;
    }

    if (got >= 3 && strcasecmp(a, "list") == 0) {
        if (strcasecmp(b, "room") != 0 && strcasecmp(b, "mob") != 0 && strcasecmp(b, "obj") != 0) {
            descriptor_send(d, usage);
            return true;
        }
        int vnum = atoi(c);
        trigger_t trigs[32];
        int n = trigger_repo_list_for(b, vnum, trigs, 32);
        if (n == 0) {
            descriptor_send(d, "No triggers on that target.\r\n");
            return true;
        }
        char out[4096];
        size_t len = 0;
        for (int i = 0; i < n; i++) {
            len += (size_t)snprintf(out + len, sizeof(out) - len,
                "#%ld %s %d %s%s%s%s\r\n",
                trigs[i].id, trigs[i].target_type, trigs[i].target_vnum, trigs[i].trigger_type,
                trigs[i].match_text[0] ? " match=\"" : "",
                trigs[i].match_text[0] ? trigs[i].match_text : "",
                trigs[i].match_text[0] ? "\"" : "");
            if (len >= sizeof(out))
                break;
        }
        descriptor_page_start(d, out, 0);
        return true;
    }

    if (got < 3) {
        descriptor_send(d, usage);
        return true;
    }

    if (strcasecmp(a, "room") != 0 && strcasecmp(a, "mob") != 0 && strcasecmp(a, "obj") != 0) {
        descriptor_send(d, usage);
        return true;
    }
    int vnum = atoi(b);
    if (!isdigit((unsigned char)b[0]) || vnum <= 0) {
        descriptor_send(d, usage);
        return true;
    }

    bool valid_type;
    if (strcasecmp(a, "room") == 0)
        valid_type = strcasecmp(c, "enter") == 0 || strcasecmp(c, "random") == 0;
    else if (strcasecmp(a, "mob") == 0)
        valid_type = strcasecmp(c, "greet") == 0 || strcasecmp(c, "speech") == 0
                     || strcasecmp(c, "death") == 0 || strcasecmp(c, "random") == 0;
    else
        valid_type = strcasecmp(c, "get") == 0 || strcasecmp(c, "wear") == 0;

    if (!valid_type) {
        descriptor_send(d, usage);
        return true;
    }

    if (strcasecmp(c, "speech") == 0 && !rest[0]) {
        descriptor_send(d, "A speech trigger needs a keyword: "
                           "edit trigger mob <vnum> speech <keyword>\r\n");
        return true;
    }

    /* Builders (51-54) are confined to their assigned zone for a room
     * target, same rule `edit room` enforces -- mob/obj prototypes have no
     * zone_repo lookup wired yet (TODO.md), so that check is scoped to
     * rooms only for now. */
    if (strcasecmp(a, "room") == 0 && !zone_can_edit(d->character, room_repo_get_zone(vnum))) {
        descriptor_send(d, "You aren't assigned to that zone.\r\n");
        return true;
    }

    /* Already validated above to be one of a handful of short fixed
     * words ("room"/"mob"/"obj", "enter"/"random"/"greet"/...) -- the
     * explicit width still caps it so the compiler can see it's bounded. */
    snprintf(d->trig_target_type, sizeof(d->trig_target_type), "%.7s", a);
    d->trig_target_vnum = vnum;
    snprintf(d->trig_trigger_type, sizeof(d->trig_trigger_type), "%.15s", c);
    d->trig_chance_pct = TRIGGER_DEFAULT_CHANCE_PCT;
    d->trig_match_text[0] = '\0';
    if (strcasecmp(c, "speech") == 0)
        snprintf(d->trig_match_text, sizeof(d->trig_match_text), "%s", rest);
    else if (strcasecmp(c, "random") == 0 && rest[0] && isdigit((unsigned char)rest[0]))
        d->trig_chance_pct = atoi(rest);

    d->edit_buf[0] = '\0';
    d->edit_len = 0;

    char head[320];
    snprintf(head, sizeof(head),
        "\r\n-- Writing trigger: %s %d %s%s%s --\r\n"
        "Type the script, one action per line (echo/echoroom/emote/teleport/give/"
        "damage/log). /s saves, /a aborts, /b blanks, /f reflows to width.\r\n] ",
        a, vnum, c,
        d->trig_match_text[0] ? " keyword=" : "", d->trig_match_text);
    descriptor_send(d, head);
    d->edit_kind = EDIT_TRIGGER;
    return true;
}
