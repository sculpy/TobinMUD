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
#include "db.h"
#include "room.h"
#include "skill.h"
#include "thing.h"
#include "world.h"
/* `spy <direction>` (Thief, missing-skill audit, skill.c level 38,
 * SKILL_TIER_ADVANCED). Real upstream (disc_thief_stealth.cc's spy())
 * is a toggle affect (AFF_SCRYING) that hides the "$n looks at you"
 * notice when the thief later looks at someone in the SAME room --
 * Tobin's own `look <target>` never sends that notice to the victim in
 * the first place (no such message exists to suppress), so that exact
 * mechanic has no real gap to fill here. Ported instead against what
 * the skill.c/skill_help.sql roster description actually promises
 * ("Covertly watch a room from elsewhere") -- a single-hop remote
 * glimpse, same one-exit lookup cmd_scan.c's own scan_exit() already
 * does (reused verbatim below), gated by a proficiency roll like any
 * other Thief skill (cmd_peek.c's own shape) rather than scan's no-
 * roll cross-class version. Genuinely covert: unlike `scan` (which
 * announces "$n scans the surrounding area" to the room), `spy` prints
 * nothing to either room -- no one here or there has any way to know
 * it happened, success or failure. Deliberately simplified vs. real
 * upstream and vs. a full `look`: no item listing, no darkness gate
 * (Tobin's own room_is_dark_for() is display-only plumbing tied to the
 * viewer's own descriptor, not reusable at a distance without deeper
 * surgery) -- just the room's name, description, and who's standing in
 * it, same "genuinely useful, working, disclosed scope-cut" precedent
 * every other backlog item in this audit takes. */
static int spy_parse_dir(const char *tok) {
    size_t len = strlen(tok);
    if (len == 0)
        return -1;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    return -1;
}
/* Destination vnum of room `vnum`'s exit in `dir` (or -1), with *cond set
 * to the exit's condition bitmask. Identical shape to cmd_scan.c's own
 * scan_exit() (not shared -- that one's file-local there too): prefers
 * the in-memory (active) room, falls back to a roomexit query so an
 * adjacent-but-never-loaded room can still be spied on (occupants, as
 * ever, only exist in an active room). */
static int spy_exit(db_conn_t *db, int vnum, int dir, int *cond) {
    *cond = 0;
    room_t *r = world_get_room(vnum);
    if (r) {
        *cond = r->exit_cond[dir];
        return r->exits[dir];
    }
    int dest = -1;
    if (db && db_query(db, "select destination, condition_flag from roomexit "
                           "where vnum=%i and direction=%i", vnum, dir)
        && db_fetch_row(db)) {
        dest = atoi(db_get(db, "destination"));
        *cond = atoi(db_get(db, "condition_flag"));
    }
    return dest;
}
bool cmd_spy(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "spy")) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }
    char arg1[32] = "";
    sscanf(args, "%31s", arg1);
    int dir = spy_parse_dir(arg1);
    if (dir < 0) {
        descriptor_send(d, "Usage: spy <direction>\r\n");
        return true;
    }
    bool imm = being_is_immortal(ch);
    const skill_def_t *sk = skill_find(ch->char_class, "spy", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
    if (!success) {
        char failmsg[96];
        snprintf(failmsg, sizeof(failmsg),
                 "You try to get a covert look to the %s, but can't quite focus.\r\n",
                 DIR_NAMES[dir]);
        descriptor_send(d, failmsg);
        return true;
    }
    db_conn_t *db = db_open(DB_TOBIN);
    int cond = 0;
    int dest = spy_exit(db, ch->base.roomp->vnum, dir, &cond);
    if (db)
        db_close(db);
    if (dest < 0) {
        descriptor_send(d, "There's nothing that way to spy on.\r\n");
        return true;
    }
    if (cond & (EXIT_COND_CLOSED | EXIT_COND_SECRET)) {
        descriptor_send(d, "A closed door blocks your view that way.\r\n");
        return true;
    }
    room_t *dr = world_get_room(dest);
    if (!dr) {
        descriptor_send(d, "You catch only a formless glimpse -- too far to make out.\r\n");
        return true;
    }
    char out[2048];
    int n = snprintf(out, sizeof(out), "<c>You covertly spy %s, to the %s:<z>\r\n%s\r\n%s\r\n",
                      dr->base.name, DIR_NAMES[dir], dr->base.name, dr->description);
    bool any = false;
    for (thing_t *t = dr->base.stuff_head; t && (size_t)n < sizeof(out); t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (t->kind == THING_PC && !((const being_t *)t)->desc)
            continue; /* linkdead, not a real target -- same convention as scan */
        any = true;
        const char *label = t->short_descr[0] ? t->short_descr : t->name;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s is there.\r\n", label);
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  No one is there.\r\n");
    descriptor_send(d, out);
    return true;
}
