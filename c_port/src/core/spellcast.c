/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "spellcast.h"

#include <stdio.h>
#include <stdlib.h>

#include "pulse.h"

/* Gesture/verbal flavor pairs, shown 2 per casting-tick round (plus one
 * closing "building"/"complete" line -- 3 lines/round, matching the
 * user's "2-3 lines per casting tick" ask), same self-line/room-line
 * shape as spell_flavor.c's flavor_pair_t (deliberately duplicated
 * rather than shared -- that file's one-shot single-flourish use is now
 * dead code from `cast`'s perspective, see cmd_cast.c's own note on
 * where its call was removed from). `room_line` may use `%s` (capitalized
 * display name) and/or a second `%s` (gender_possess() pronoun) -- every
 * call below supplies both regardless of whether a given line uses one
 * or two, so the arg count always matches the format. */
typedef struct {
    const char *self_line;
    const char *room_line;
} cast_line_t;

/* Mage -- purple (<p>/<P>). */
static const cast_line_t MAGE_GESTURE[] = {
    { "<p>You trace a burning sigil in the air with your fingertip.<z>",
      "<p>%s traces a burning sigil in the air with %s fingertip.<z>" },
    { "<p>Arcane light gathers between your outstretched hands.<z>",
      "<p>Arcane light gathers between %s's outstretched hands.<z>" },
    { "<p>You weave your fingers through unseen threads of power.<z>",
      "<p>%s weaves %s fingers through unseen threads of power.<z>" },
};
static const cast_line_t MAGE_VERBAL[] = {
    { "<p>You chant a string of harsh, guttural syllables.<z>",
      "<p>%s chants a string of harsh, guttural syllables.<z>" },
    { "<p>Your voice rises and falls with the cadence of an old incantation.<z>",
      "<p>%s's voice rises and falls with the cadence of an old incantation.<z>" },
    { "<p>You speak a word of power that makes the air itself shiver.<z>",
      "<p>%s speaks a word of power that makes the air itself shiver.<z>" },
};
static const char *const MAGE_BUILDING = "<p>Arcane energy crackles and gathers around you.<z>";
static const char *const MAGE_COMPLETE = "<p>The gathered magic strains at its limits, ready to be unleashed!<z>";

/* Druid -- yellow (<y>/<Y>, user follow-up correction), a forest-flavor
 * reskin of the SAME three gesture/verbal/closing beats as Mage above,
 * not a separately-structured set (user: "modified messages that mages
 * have except... forest flavor"). */
static const cast_line_t DRUID_GESTURE[] = {
    { "<y>You trace a living sigil of vine and leaf in the air.<z>",
      "<y>%s traces a living sigil of vine and leaf in the air.<z>" },
    { "<y>Sunlight gathers, warm and green, between your outstretched hands.<z>",
      "<y>Sunlight gathers, warm and green, between %s's outstretched hands.<z>" },
    { "<y>You weave your fingers through unseen roots and growing things.<z>",
      "<y>%s weaves %s fingers through unseen roots and growing things.<z>" },
};
static const cast_line_t DRUID_VERBAL[] = {
    { "<y>You murmur words as old as the deep forest itself.<z>",
      "<y>%s murmurs words as old as the deep forest itself.<z>" },
    { "<y>Your voice rises and falls like wind moving through high branches.<z>",
      "<y>%s's voice rises and falls like wind moving through high branches.<z>" },
    { "<y>You speak a word of growing that makes the ground itself stir.<z>",
      "<y>%s speaks a word of growing that makes the ground itself stir.<z>" },
};
static const char *const DRUID_BUILDING = "<y>The scent of moss and rain grows thick around you.<z>";
static const char *const DRUID_COMPLETE = "<y>The wild magic strains at its limits, ready to answer your call!<z>";

/* MSP casting sound (same pool cmd_cast.c's old one-shot flourish played
 * once from) -- played once per round now, one per casting tick. */
static const char *const CASTING_SOUND_POOL[] = {
    "casting1.wav", "casting2.wav", "casting3.wav",
    "casting4.wav", "casting5.wav", "casting6.wav",
};

/* Shows one cast_line_t's self line to `ch` and its room line (caster's
 * capitalized display name + gendered possessive pronoun substituted in)
 * to everyone else in the room -- see spell_flavor.c's identical
 * show_pair() for the precedent this duplicates. */
static void show_cast_line(descriptor_t *d, being_t *ch, const cast_line_t *p) {
    char selfmsg[224];
    snprintf(selfmsg, sizeof(selfmsg), "%s\r\n", p->self_line);
    descriptor_send(d, selfmsg);

    if (ch->base.roomp) {
        char capbuf[128], roommsg[288], roommsg_nl[300];
        snprintf(roommsg, sizeof(roommsg), p->room_line,
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), gender_possess(ch->gender));
        snprintf(roommsg_nl, sizeof(roommsg_nl), "%s\r\n", roommsg);
        descriptor_room_echo(ch->base.roomp, ch, roommsg_nl);
    }
}

/* Shows one casting-tick round's worth of flavor (a random gesture line,
 * a random verbal line, then a fixed building/completion line -- the
 * completion wording only on the FINAL round) plus its casting sound. */
static void spellcast_show_round(descriptor_t *d, being_t *ch, int round_num, int total_rounds) {
    bool druid = ch->char_class == CLASS_DRUID;
    const cast_line_t *gestures = druid ? DRUID_GESTURE : MAGE_GESTURE;
    const cast_line_t *verbals = druid ? DRUID_VERBAL : MAGE_VERBAL;
    int gcount = druid ? (int)(sizeof(DRUID_GESTURE) / sizeof(DRUID_GESTURE[0]))
                        : (int)(sizeof(MAGE_GESTURE) / sizeof(MAGE_GESTURE[0]));
    int vcount = druid ? (int)(sizeof(DRUID_VERBAL) / sizeof(DRUID_VERBAL[0]))
                        : (int)(sizeof(MAGE_VERBAL) / sizeof(MAGE_VERBAL[0]));

    show_cast_line(d, ch, &gestures[rand() % gcount]);
    show_cast_line(d, ch, &verbals[rand() % vcount]);

    const char *closing = (round_num >= total_rounds) ? (druid ? DRUID_COMPLETE : MAGE_COMPLETE)
                                                        : (druid ? DRUID_BUILDING : MAGE_BUILDING);
    char msg[224];
    snprintf(msg, sizeof(msg), "%s\r\n", closing);
    descriptor_send(d, msg);

    if (d)
        descriptor_send_msp_sound(d, CASTING_SOUND_POOL[rand() % 6], 100);
}

void spellcast_start(descriptor_t *d, being_t *ch, const skill_def_t *sk, being_t *target) {
    /* 2 or 3 rounds (user: "2-3 rounds... druid casting should take
     * about the same amount of time") -- randomized once per cast
     * attempt rather than fixed, same class-neutral length for both
     * Mage and Druid. */
    ch->cast_rounds_total = 2 + (rand() % 2);
    ch->cast_rounds_left = ch->cast_rounds_total;
    snprintf(ch->cast_spell_name, sizeof(ch->cast_spell_name), "%s", sk->name);
    ch->cast_target = target;
    ch->is_casting = true;

    /* Locks out further commands for the whole delay, same lag/lockout
     * convention every other action already uses (being_set_wait()'s own
     * doc comment) -- a no-op for an immortal (being_get_wait() always
     * reads 0 for them). */
    being_set_wait(ch, ch->cast_rounds_total * COMBAT_ROUND_PULSES);

    spellcast_show_round(d, ch, 1, ch->cast_rounds_total);
}

/* Periodic hook that advances every connected, currently-casting
 * character by one round -- see being.h's `is_casting` doc comment and
 * meditate.c's meditate_tick_run() for the pattern this mirrors. */
void spellcast_tick_run(long pulse_num) {
    (void)pulse_num;

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *ch = d->character;
        if (!ch || !ch->is_casting)
            continue;

        /* Interruption -- deliberately narrow (user's spec: "a simple
         * still connected and alive check is enough, mirroring
         * meditate's own scope"). Does NOT break on ch->fighting: most
         * offensive spells are cast WHILE already fighting (that's the
         * whole point of an in-combat cast) -- treating "fighting" alone
         * as a break condition would abort the overwhelming majority of
         * combat casts the instant they started, unlike `meditating`
         * (which is only ever entered from a fight-free resting/sitting
         * state to begin with). */
        if (!ch->desc || ch->progress.hp <= 0
            || ch->position == POSITION_DEAD || ch->position == POSITION_MORTALLYW
            || ch->position == POSITION_INCAP || ch->position == POSITION_STUNNED
            || ch->position == POSITION_SLEEPING) {
            ch->is_casting = false;
            ch->cast_target = NULL;
            descriptor_send(d, "Your concentration is broken -- the spell fizzles!\r\n");
            continue;
        }
        if (!ch->cast_target) {
            /* being_destroy() already cleared this and sent its own
             * fizzle message (being.c) -- just finish the cancellation. */
            ch->is_casting = false;
            continue;
        }

        /* Round 1's flavor was already shown synchronously at cast time
         * (spellcast_start()) so the caster gets immediate feedback
         * instead of ~1.2s of silence -- this tick advances to round 2,
         * 3, ..., cast_rounds_total, one tick apart. The FINAL round's
         * flavor (the "<completion line>" closing) is shown here too,
         * then the effect resolves the same tick, right after it. */
        ch->cast_rounds_left--;
        int round_num = ch->cast_rounds_total - ch->cast_rounds_left;
        spellcast_show_round(d, ch, round_num, ch->cast_rounds_total);
        if (ch->cast_rounds_left > 0)
            continue;

        /* Countdown complete -- resolve the real effect now, through the
         * SAME per-spell dispatch chain `cast` always used
         * (cmd_cast_resolve_effect(), cmd_cast.c). Re-looked-up by name/
         * class (being.h can't cache a skill_def_t* directly -- would
         * need to #include skill.h, which itself includes being.h). */
        being_t *target = ch->cast_target;
        char spell_name[64];
        snprintf(spell_name, sizeof(spell_name), "%s", ch->cast_spell_name);
        ch->is_casting = false;
        ch->cast_target = NULL;

        const skill_def_t *sk = skill_find(ch->char_class, spell_name, being_is_immortal(ch));
        if (!sk) {
            descriptor_send(d, "Your spell falters -- something about it feels wrong now.\r\n");
            continue;
        }
        cmd_cast_resolve_effect(d, ch, target, sk);
    }
}
