/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
#include "telnet_client.h"

#include <stdlib.h>
#include <string.h>

enum {
    TN_IAC = 255, TN_WILL = 251, TN_WONT = 252, TN_DO = 253, TN_DONT = 254,
    TN_SB = 250, TN_SE = 240, TN_ECHO = 1, TN_SGA = 3,
    TN_GMCP = 201, TN_MSDP = 69, TN_MSP = 90,
    MSDP_VAR = 1, MSDP_VAL = 2,
};

#define SUBNEG_BUF_MAX 4096
#define TEXT_BUF_MAX 8192

struct telnet_client {
    telnet_client_callbacks_t cb;

    /* Same "resumable across feed() calls" shape as the server's own
     * parser (descriptor.c) -- a real TCP stream can hand us a telnet
     * sequence split across two recv()s. */
    int in_subneg;         /* 0 = no, 1 = yes, awaiting opt byte, 2 = yes, in payload */
    unsigned char subneg_opt;
    unsigned char subneg_buf[SUBNEG_BUF_MAX];
    size_t subneg_len;
    int pending_iac;       /* saw a bare IAC, waiting for the command byte */
    int pending_cmd;       /* saw IAC <cmd>, waiting for the option byte (WILL/WONT/DO/DONT) */
    unsigned char pending_cmd_byte;
    int subneg_prev_iac;   /* inside subneg payload: previous byte was IAC (escape/terminator lookahead) */

    /* Accumulates plain display text between telnet sequences, flushed
     * to on_text() whenever an IAC interrupts it or feed() ends. */
    char text_buf[TEXT_BUF_MAX];
    size_t text_len;
};

telnet_client_t *telnet_client_create(const telnet_client_callbacks_t *cb) {
    telnet_client_t *tc = calloc(1, sizeof(*tc));
    if (!tc)
        return NULL;
    tc->cb = *cb;
    return tc;
}

void telnet_client_destroy(telnet_client_t *tc) {
    free(tc);
}

static void flush_text(telnet_client_t *tc) {
    if (tc->text_len > 0 && tc->cb.on_text)
        tc->cb.on_text(tc->cb.ctx, tc->text_buf, tc->text_len);
    tc->text_len = 0;
}

static void reply(telnet_client_t *tc, unsigned char cmd, unsigned char opt) {
    unsigned char msg[3] = { TN_IAC, cmd, opt };
    if (tc->cb.send_bytes)
        tc->cb.send_bytes(tc->cb.ctx, msg, 3);
}

/* Splits a GMCP payload "Module.Name {json...}" into package + json and
 * fires on_gmcp(). Payload is NUL-terminated by the caller first. */
static void dispatch_gmcp(telnet_client_t *tc, const char *payload, size_t len) {
    char pkgbuf[128];
    const char *sp = memchr(payload, ' ', len);
    size_t pkglen = sp ? (size_t)(sp - payload) : len;
    if (pkglen >= sizeof(pkgbuf))
        pkglen = sizeof(pkgbuf) - 1;
    memcpy(pkgbuf, payload, pkglen);
    pkgbuf[pkglen] = '\0';
    const char *json = sp ? sp + 1 : "";
    if (tc->cb.on_gmcp)
        tc->cb.on_gmcp(tc->cb.ctx, pkgbuf, json);
}

/* Splits an MSDP VAR/VAL byte stream into name/value pairs and fires
 * on_msdp_var() per pair. Flat pairs only, matching the server's own
 * v1 scope (no TABLE/ARRAY nesting) -- see msdp.h's own comment. */
static void dispatch_msdp(telnet_client_t *tc, const unsigned char *payload, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (payload[i] != MSDP_VAR)
            break; /* malformed or a nested structure we don't support -- stop */
        i++;
        size_t name_start = i;
        while (i < len && payload[i] != MSDP_VAL)
            i++;
        size_t name_len = i - name_start;
        if (i >= len)
            break;
        i++; /* skip MSDP_VAL */
        size_t val_start = i;
        while (i < len && payload[i] != MSDP_VAR)
            i++;
        size_t val_len = i - val_start;

        char namebuf[64], valbuf[64];
        size_t nl = name_len < sizeof(namebuf) - 1 ? name_len : sizeof(namebuf) - 1;
        size_t vl = val_len < sizeof(valbuf) - 1 ? val_len : sizeof(valbuf) - 1;
        memcpy(namebuf, payload + name_start, nl);
        namebuf[nl] = '\0';
        memcpy(valbuf, payload + val_start, vl);
        valbuf[vl] = '\0';
        if (tc->cb.on_msdp_var)
            tc->cb.on_msdp_var(tc->cb.ctx, namebuf, valbuf);
    }
}

static void finish_subneg(telnet_client_t *tc) {
    if (tc->subneg_opt == TN_GMCP) {
        char nulbuf[SUBNEG_BUF_MAX + 1];
        memcpy(nulbuf, tc->subneg_buf, tc->subneg_len);
        nulbuf[tc->subneg_len] = '\0';
        dispatch_gmcp(tc, nulbuf, tc->subneg_len);
    } else if (tc->subneg_opt == TN_MSDP) {
        dispatch_msdp(tc, tc->subneg_buf, tc->subneg_len);
    }
    /* MSP has no subnegotiation payload (see msp.h's own comment on the
     * server side -- it's an in-band text marker, arrives via on_text()
     * and is scanned for separately, e.g. by the Win32 layer). */
    tc->in_subneg = 0;
    tc->subneg_len = 0;
}

void telnet_client_feed(telnet_client_t *tc, const unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char b = data[i];

        if (tc->in_subneg == 1) { /* awaiting the option-id byte right after IAC SB */
            tc->subneg_opt = b;
            tc->in_subneg = 2;
            tc->subneg_prev_iac = 0;
            continue;
        }
        if (tc->in_subneg == 2) {
            if (tc->subneg_prev_iac) {
                tc->subneg_prev_iac = 0;
                if (b == TN_SE) {
                    finish_subneg(tc);
                    continue;
                }
                if (b == TN_IAC) {
                    if (tc->subneg_len < SUBNEG_BUF_MAX)
                        tc->subneg_buf[tc->subneg_len++] = TN_IAC;
                    continue;
                }
                if (tc->subneg_len < SUBNEG_BUF_MAX)
                    tc->subneg_buf[tc->subneg_len++] = b;
                continue;
            }
            if (b == TN_IAC) {
                tc->subneg_prev_iac = 1;
                continue;
            }
            if (tc->subneg_len < SUBNEG_BUF_MAX)
                tc->subneg_buf[tc->subneg_len++] = b;
            continue;
        }

        if (tc->pending_cmd) {
            tc->pending_cmd = 0;
            unsigned char cmd = tc->pending_cmd_byte;
            unsigned char opt = b;
            if (cmd == TN_WILL) {
                if (opt == TN_ECHO || opt == TN_SGA || opt == TN_GMCP
                    || opt == TN_MSDP || opt == TN_MSP)
                    reply(tc, TN_DO, opt);
                else
                    reply(tc, TN_DONT, opt);
            } else if (cmd == TN_DO) {
                reply(tc, TN_WONT, opt); /* we offer nothing back */
            }
            /* WONT/DONT from the server: nothing to react to (no option
             * state kept client-side beyond the auto-reply above). */
            continue;
        }
        if (tc->pending_iac) {
            tc->pending_iac = 0;
            if (b == TN_WILL || b == TN_WONT || b == TN_DO || b == TN_DONT) {
                tc->pending_cmd = 1;
                tc->pending_cmd_byte = b;
            } else if (b == TN_SB) {
                flush_text(tc);
                tc->in_subneg = 1;
            }
            /* Any other IAC <cmd> (NOP, etc) needs no further bytes and
             * no reaction -- just consumed. */
            continue;
        }
        if (b == TN_IAC) {
            tc->pending_iac = 1;
            continue;
        }

        if (tc->text_len < TEXT_BUF_MAX) {
            tc->text_buf[tc->text_len++] = (char)b;
        } else {
            flush_text(tc);
            tc->text_buf[tc->text_len++] = (char)b;
        }
    }
    flush_text(tc);
}
