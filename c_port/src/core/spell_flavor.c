/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "spell_flavor.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *self_line;
    /* %s = the caster's capitalized display name, %s = a gendered
     * possessive pronoun (gender_possess() -- "his"/"her"/"its", the
     * standing house convention over a blanket "their", see being.h).
     * Every room_line below uses EXACTLY these two placeholders, in
     * this order, even where only one is grammatically needed --
     * show_pair() always supplies both, so the format/argument count
     * must match on every line (a real bug caught before it shipped:
     * an earlier draft had some lines with a second bare %s meaning
     * the name again, called with only one argument -- undefined
     * behavior reading a nonexistent vararg). */
    const char *room_line;
} flavor_pair_t;

/* Gesture/verbal flavor text, adapted from real SneezyMUD's
 * TBeing::sendCastingMessages() (spelltask.cc) -- that function rolls
 * one of ~5 gesture lines and one of ~5 verbal lines per casting
 * ROUND; these are a representative handful of that same real text
 * (trimmed for a single one-shot use here rather than every round of
 * a multi-round task, see spell_flavor.h's own comment), kept
 * separate for Mage/Druid ("casting", gesture-and-word) vs Cleric
 * ("praying", symbol-and-word) to match the original's own
 * SPELL_CASTER/SPELL_PRAYER split. */
static const flavor_pair_t CAST_GESTURE[] = {
    { "You trace a complex pattern in the air with your hand.",
      "%s traces a complex magic pattern in the air." },
    { "You trace a line of a magic rune in the air.",
      "%s concentrates hard, making a gesture in the air." },
    { "You add another line to the magic rune you've been tracing.",
      "%s adds another line to the magic rune traced by %s hand." },
    { "The air shimmers around your hand as it follows a magic line.",
      "The air shimmers around %s's hand as it follows a magic line." },
};
static const flavor_pair_t CAST_VERBAL[] = {
    { "You lyrically chant the words of a gentle incantation.",
      "%s lyrically chants the words of a gentle incantation." },
    { "You utter the words needed to complete your spell.",
      "%s utters the words needed to complete %s spell." },
    { "Your words form the pattern calling on the magic to be unleashed.",
      "%s's words form a pattern calling on the magic to be released." },
    { "Your voice calls on the primordial forms and draws forth primal magic.",
      "%s's voice calls on the primordial forms, drawing forth primal magic." },
};
static const flavor_pair_t PRAY_GESTURE[] = {
    { "You raise your symbol towards the sky.",
      "%s raises %s symbol towards the sky." },
    { "Your symbol flares with heat as you raise it towards the heavens.",
      "%s's symbol flares as it is raised towards the heavens." },
    { "Your symbol glows with power as you call on its strength.",
      "%s's symbol glows with power, drawing on %s strength." },
    { "You raise your symbol towards the heavens looking for favor.",
      "%s holds %s symbol towards the heavens looking for favor." },
};
static const flavor_pair_t PRAY_VERBAL[] = {
    { "You softly say the familiar words of your prayer.",
      "%s softly says the familiar words of %s prayer." },
    { "You continue to utter the words needed to complete your prayer.",
      "%s continues to utter the words needed to complete %s prayer." },
    { "Your voice forms the historic words of supplication and entreaty.",
      "%s gives voice to the historic words of supplication and entreaty." },
    { "Your voice carries the culmination of your prayer.",
      "%s's voice is heard clearly, echoing the words of prayer." },
};

/* Shows one flavor_pair_t's self line to `ch` and its room line
 * (caster's capitalized display name + gendered possessive pronoun
 * substituted in, see flavor_pair_t's own doc comment) to everyone
 * else in the room. */
static void show_pair(descriptor_t *d, being_t *ch, const flavor_pair_t *p) {
    char selfmsg[192];
    snprintf(selfmsg, sizeof(selfmsg), "%s\r\n", p->self_line);
    descriptor_send(d, selfmsg);

    if (ch->base.roomp) {
        char capbuf[128], roommsg[256], roommsg_nl[280];
        snprintf(roommsg, sizeof(roommsg), p->room_line,
                 being_display_name_cap(ch, capbuf, sizeof(capbuf)), gender_possess(ch->gender));
        snprintf(roommsg_nl, sizeof(roommsg_nl), "%s\r\n", roommsg);
        descriptor_room_echo(ch->base.roomp, ch, roommsg_nl);
    }
}

void spell_flavor_show(descriptor_t *d, being_t *ch, bool is_prayer) {
    const flavor_pair_t *gestures = is_prayer ? PRAY_GESTURE : CAST_GESTURE;
    int gesture_count = is_prayer ? (int)(sizeof(PRAY_GESTURE) / sizeof(PRAY_GESTURE[0]))
                                   : (int)(sizeof(CAST_GESTURE) / sizeof(CAST_GESTURE[0]));
    const flavor_pair_t *verbals = is_prayer ? PRAY_VERBAL : CAST_VERBAL;
    int verbal_count = is_prayer ? (int)(sizeof(PRAY_VERBAL) / sizeof(PRAY_VERBAL[0]))
                                   : (int)(sizeof(CAST_VERBAL) / sizeof(CAST_VERBAL[0]));

    show_pair(d, ch, &gestures[rand() % gesture_count]);
    show_pair(d, ch, &verbals[rand() % verbal_count]);

    descriptor_send(d, is_prayer ? "You feel your prayer being answered.\r\n"
                                   : "You feel your spell taking form.\r\n");
}
