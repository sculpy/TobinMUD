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

#include "combat.h"
#include "mob_ai.h"
#include "room.h"
#include "socials.h"
#include "suit.h"
#include "suit_repo.h"
#include "thing.h"
#include "trigger.h"

/* `say <message>` (and the `'` one-character shorthand -- see the special
 * case in cmd_table.c's cmd_dispatch()) broadcasts to everyone else in the
 * speaker's room: the speaker sees `You say, "<message>"`, everyone else
 * sees `<Name> says, "<message>"`. Mirrors the original's TBeing::doSay()
 * (misc/talk.cc): same message format, same empty-message guard, and no
 * auto-added punctuation -- whatever the player typed is used verbatim.
 * Not replicated: the original's garble() (drunk/language distortion) and
 * color-coding of the name/message. */
/* Case-insensitive "does haystack contain needle" (strcasestr is GNU-only,
 * same duplicated-helper precedent as cmd_scan.c/cmd_who.c/combat.c/...). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0)
        return true;
    for (; *haystack; haystack++)
        if (strncasecmp(haystack, needle, nl) == 0)
            return true;
    return false;
}

/* Fires every mob-in-`r`'s "speech" trigger whose match_text is a
 * substring of what was just said (user, 2026-07-11: "interaction with
 * mobs objs and room via scripts"). */
static void run_speech_triggers(being_t *speaker, room_t *r, const char *said) {
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *mob = (being_t *)t;
        trigger_t trigs[8];
        int n = trigger_repo_load_for("mob", mob->base.id, "speech", trigs, 8);
        if (n == 0)
            continue;
        /* short_descr may start with a color tag -- skip it before
         * capitalizing, same bug class fixed elsewhere (cmd_look.c/
         * cmd_object.c/cmd_scan.c/mob_ai.c/trigger.c/combat.c/cmd_move.c). */
        char capbuf[128];
        snprintf(capbuf, sizeof(capbuf), "%s", mob->base.short_descr);
        size_t ci = 0;
        while (capbuf[ci] == '<' && capbuf[ci + 1] != '\0' && capbuf[ci + 2] == '>')
            ci += 3;
        if (capbuf[ci])
            capbuf[ci] = (char)toupper((unsigned char)capbuf[ci]);
        for (int i = 0; i < n; i++) {
            if (!trigs[i].match_text[0] || !ci_contains(said, trigs[i].match_text))
                continue;
            trigger_run(&trigs[i], speaker, r, capbuf[0] ? capbuf : NULL);
        }
    }
}

/* Pet/charm (Sneezy → Tobin feature audit), user 2026-07-25: "add a
 * trigger for the pet once following the master to react and do whatever
 * he says to do, like Master says, 'attack guard' then pet attacks
 * guard" -- and the follow-up, "master says dance pet dances etc". A
 * charmed pet in the SAME room as whoever just spoke listens for its
 * first word: "attack"/"kill <target>" sets the pet's own one-sided
 * fighting pointer (same mechanism combat.c's pet-assist pass already
 * resolves each round -- see its own comment); "stop"/"stay"/"guard"
 * disengages; anything else is tried as a social verb (social_perform_
 * for(), socials.c) so "dance"/"bow"/"laugh"/... make the pet perform
 * that social too, same 155-verb roster a player has. Not gated on the
 * speaker actually BEING the pet's master -- Sneezy has no analogous
 * mechanic to check against, and restricting it to "your own pet only
 * obeys you" is a reasonable-but-arbitrary call this doesn't need to
 * make, since only the pet's own master can plausibly benefit from
 * ordering it around (a stranger's "attack" would just help the pet's
 * real owner). No feedback line to the speaker beyond the room echo
 * itself (attack/stop) or the social's own others_no_arg text (dance/...) --
 * unrecognized speech is simply not a command, same silent fallthrough
 * `say` always had.
 *
 * PET_CONFUSION_CHANCE_PCT (user follow-up, "add a chance of failure,
 * confused pet"): a charmed creature isn't a fully obedient tool -- one
 * roll per spoken command, checked AFTER confirming there's a pet in
 * earshot and a real word to react to (so silence or an empty room never
 * "wastes" a confusion), but BEFORE interpreting what was said, so a
 * confused pet visibly does nothing at all rather than doing the WRONG
 * thing (simpler than picking a plausible wrong action, and reads fine
 * either way -- "looks confused" doesn't promise it almost obeyed). */
#define PET_CONFUSION_CHANCE_PCT 20

static void try_pet_command(being_t *speaker, room_t *r, const char *said) {
    being_t *pet = being_find_charmed_pet(speaker);
    if (!pet || pet->base.roomp != r)
        return;

    char verb[32] = "", rest[192] = "";
    sscanf(said, "%31s %191[^\r\n]", verb, rest);
    if (!verb[0])
        return;

    char capbuf[128], msg[256];
    being_display_name_cap(pet, capbuf, sizeof(capbuf));

    if (rand() % 100 < PET_CONFUSION_CHANCE_PCT) {
        snprintf(msg, sizeof(msg), "%s tilts its head, looking confused.\r\n", capbuf);
        descriptor_room_echo(r, NULL, msg);
        return;
    }

    if (strcasecmp(verb, "attack") == 0 || strcasecmp(verb, "kill") == 0) {
        if (!rest[0])
            return;
        being_t *target = combat_find_room_target(pet, rest);
        if (!target || target == speaker)
            return;
        pet->fighting = target;
        snprintf(msg, sizeof(msg), "%s obeys, and turns to attack %s!\r\n",
                 capbuf, being_display_name(target));
        descriptor_room_echo(r, NULL, msg);
        return;
    }
    if (strcasecmp(verb, "stop") == 0 || strcasecmp(verb, "stay") == 0 || strcasecmp(verb, "guard") == 0) {
        pet->fighting = NULL;
        snprintf(msg, sizeof(msg), "%s obeys, and stands down.\r\n", capbuf);
        descriptor_room_echo(r, NULL, msg);
        return;
    }

    social_perform_for(pet, verb);
}

/* SPEC_PROC_NEWBIE_EQUIPPER (mob_ai.h has the full origin/scope
 * rationale) -- user 2026-07-26: "in room 570 (welfare) they could ask
 * the social worker to receive a new set of newbie gear." Any mob
 * carrying this spec-proc (seeded live only on vnum 90, "the Grimhaven
 * social worker") reissues the SPEAKER's own class's newbie suit on
 * request, same suit_grant() the automatic character-creation issue and
 * `loadsuit` already call. Keyword-gated the same substring way
 * run_speech_triggers() above matches a trigger's match_text, just a
 * fixed built-in list instead of a DB row -- no anti-farming limit
 * (nothing asked for one; this is explicitly "lost your gear, get a
 * replacement"). Stops at the first matching mob in the room. */
static bool is_newbie_gear_request(const char *said) {
    return ci_contains(said, "gear") || ci_contains(said, "equipment")
        || ci_contains(said, "newbie") || ci_contains(said, "supplies");
}

static void try_newbie_equipper(being_t *speaker, room_t *r, const char *said) {
    if (!speaker->desc || speaker->player_id <= 0 || !is_newbie_gear_request(said))
        return;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *mob = (being_t *)t;
        if (mob->mob_spec_proc != SPEC_PROC_NEWBIE_EQUIPPER)
            continue;

        char capbuf[128], msg[256];
        being_display_name_cap(mob, capbuf, sizeof(capbuf));
        int suit_id = suit_repo_find_for_class((int)speaker->char_class);
        int n = suit_id >= 0 ? suit_grant(speaker, suit_id) : 0;
        if (n > 0)
            snprintf(msg, sizeof(msg), "%s hands you a fresh set of gear.\r\n", capbuf);
        else
            snprintf(msg, sizeof(msg), "%s frowns -- \"I'm afraid I don't have anything for you right now.\"\r\n", capbuf);
        descriptor_send(speaker->desc, msg);
        return;
    }
}

bool cmd_say(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Yes, but WHAT do you want to say?\r\n");
        return true;
    }

    /* Colorized wrapper (user spec, Tier 3): the say framing -- name,
     * "say(s)," and the opening quote -- renders cyan, reset before the
     * message so the player's text shows as typed (including their own
     * color tags), and a final <z> before the closing quote so an
     * unterminated tag can never color the quote or bleed onward (the
     * Session 20 finding, preserved). Tags strip cleanly when color is
     * off. */
    char msg[336];
    snprintf(msg, sizeof(msg), "<c>You say, \"<z>%s<c>\"<z>\r\n", args);
    descriptor_send(d, msg);

    room_t *r = d->character->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t == &d->character->base || t->kind != THING_PC)
            continue;
        being_t *other = (being_t *)t;
        if (!other->desc)
            continue;
        snprintf(msg, sizeof(msg), "<c>%s says, \"<z>%s<c>\"<z>\r\n",
                 d->character->base.name, args);
        descriptor_notify_comm(other->desc, msg);
    }

    run_speech_triggers(d->character, r, args);
    try_pet_command(d->character, r, args);
    try_newbie_equipper(d->character, r, args);

    return true;
}
