/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
#include "ansi_client.h"

#include <stdlib.h>

#define RUN_BUF_MAX 4096
#define PARAM_BUF_MAX 16

struct ansi_client {
    ansi_client_callbacks_t cb;

    int state; /* 0 = normal, 1 = saw ESC, 2 = in CSI params */
    char params[PARAM_BUF_MAX];
    size_t param_len;

    int color_index; /* current -1..7 */
    int bold;

    char run_buf[RUN_BUF_MAX];
    size_t run_len;
};

ansi_client_t *ansi_client_create(const ansi_client_callbacks_t *cb) {
    ansi_client_t *ac = calloc(1, sizeof(*ac));
    if (!ac)
        return NULL;
    ac->cb = *cb;
    ac->color_index = -1;
    return ac;
}

void ansi_client_destroy(ansi_client_t *ac) {
    free(ac);
}

static void flush_run(ansi_client_t *ac) {
    if (ac->run_len > 0 && ac->cb.emit)
        ac->cb.emit(ac->cb.ctx, ac->run_buf, ac->run_len, ac->color_index, ac->bold);
    ac->run_len = 0;
}

/* Applies one semicolon-separated SGR parameter list (already
 * collected in ac->params) to ac->color_index/ac->bold. */
static void apply_sgr(ansi_client_t *ac) {
    if (ac->param_len == 0) { /* bare ESC[m == reset */
        ac->color_index = -1;
        ac->bold = 0;
        return;
    }
    int val = 0;
    int have_digit = 0;
    for (size_t i = 0; i <= ac->param_len; i++) {
        char c = (i < ac->param_len) ? ac->params[i] : ';';
        if (c >= '0' && c <= '9') {
            val = val * 10 + (c - '0');
            have_digit = 1;
            continue;
        }
        /* separator (';' or end) -- apply `val` if we saw one */
        if (have_digit) {
            if (val == 0) {
                ac->color_index = -1;
                ac->bold = 0;
            } else if (val == 1) {
                ac->bold = 1;
            } else if (val >= 30 && val <= 37) {
                ac->color_index = val - 30;
            } else if (val >= 90 && val <= 97) {
                ac->color_index = val - 90;
                ac->bold = 1;
            }
            /* background (40-47), underline, etc: not tracked -- Tobin's
             * own colorstring.c doesn't emit them either (see its own
             * "dropped background/flash codes" scope-cut). */
        }
        val = 0;
        have_digit = 0;
    }
}

void ansi_client_feed(ansi_client_t *ac, const char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char b = (unsigned char)data[i];

        if (ac->state == 2) { /* collecting CSI params until a letter terminates */
            if (b >= '0' && b <= '9') {
                if (ac->param_len < PARAM_BUF_MAX - 1)
                    ac->params[ac->param_len++] = (char)b;
                continue;
            }
            if (b == ';') {
                if (ac->param_len < PARAM_BUF_MAX - 1)
                    ac->params[ac->param_len++] = (char)b;
                continue;
            }
            /* Any other byte terminates the sequence -- 'm' (SGR) is
             * the only one Tobin's server emits, but harmlessly consume
             * (with no color change) any other CSI terminator too
             * rather than leaking it into visible text. */
            if (b == 'm')
                apply_sgr(ac);
            ac->state = 0;
            ac->param_len = 0;
            continue;
        }
        if (ac->state == 1) { /* saw ESC, expect '[' */
            if (b == '[') {
                flush_run(ac);
                ac->state = 2;
                ac->param_len = 0;
            } else {
                ac->state = 0; /* not a CSI sequence -- drop the lone ESC, resume normally */
            }
            continue;
        }
        if (b == 0x1b) {
            ac->state = 1;
            continue;
        }

        if (ac->run_len < RUN_BUF_MAX) {
            ac->run_buf[ac->run_len++] = (char)b;
        } else {
            flush_run(ac);
            ac->run_buf[ac->run_len++] = (char)b;
        }
    }
    flush_run(ac);
}
