/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "rules_repo.h"

/* `rules`: list the numbered game rules. `rules <n>`: read rule n in full.
 * `edrules <n> <title>` (59+) opens the line editor to (re)write a rule. */
bool cmd_rules(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    while (*args == ' ')
        args++;

    if (!*args) {
        char out[4096];
        if (rules_repo_list(out, sizeof(out))) {
            descriptor_send(d, "\r\n<c>-- Game Rules --<z>\r\n");
            descriptor_send(d, out);
            descriptor_send(d, "\r\nType 'rules <number>' to read a rule in full.\r\n");
        } else {
            descriptor_send(d, "No rules have been posted yet.\r\n");
        }
        return true;
    }

    if (!isdigit((unsigned char)args[0])) {
        descriptor_send(d, "Usage: rules [number]\r\n");
        return true;
    }
    int num = atoi(args);
    char out[4608];
    if (rules_repo_get(num, out, sizeof(out))) {
        descriptor_send(d, out);
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "There is no rule number %d.\r\n", num);
        descriptor_send(d, msg);
    }
    return true;
}

/* `edrules <n> <title>` (59+): (re)write rule n. The number and a title are
 * required on the command line; the rule text is then typed into the shared
 * line editor ('.' saves, '~' aborts), saved via EDIT_RULES in descriptor.c. */
bool cmd_edrules(descriptor_t *d, const char *args) {
    if (!d->character)
        return true;

    int num = 0;
    if (sscanf(args, "%d", &num) != 1 || num <= 0) {
        descriptor_send(d,
            "Usage: edit rules <number> <title>\r\n"
            "Then type the rule text; /s saves, /a aborts, /b blanks, "
            "/f reflows to width.\r\n");
        return true;
    }

    /* Title = everything after the number. */
    const char *p = args;
    while (*p == ' ')
        p++;
    while (*p && isdigit((unsigned char)*p))
        p++;
    while (*p == ' ')
        p++;
    if (!*p) {
        descriptor_send(d, "You must give the rule a title: edrules <number> <title>\r\n");
        return true;
    }

    d->rule_num = num;
    snprintf(d->news_title, sizeof(d->news_title), "%s", p); /* reuse the news title scratch */
    d->edit_buf[0] = '\0';
    d->edit_len = 0;

    char head[320];
    snprintf(head, sizeof(head),
        "\r\n-- Writing rule %d: \"%s\" --\r\n"
        "Type the rule text. /s saves, /a aborts, /b blanks, "
        "/f reflows to width.\r\n] ",
        num, d->news_title);
    descriptor_send(d, head);
    d->edit_kind = EDIT_RULES;
    return true;
}
