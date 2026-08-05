/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
#ifndef TOBINCLIENT_TELNET_CLIENT_H
#define TOBINCLIENT_TELNET_CLIENT_H

#include <stddef.h>

/* Client-side mirror of the server's own telnet/GMCP/MSDP/MSP state
 * machine (c_port/src/net/descriptor.c, gmcp.c, msdp.c) -- see that
 * code's own comments for the wire format this parses. Pure data
 * processing, no socket/GUI calls of its own: fed raw bytes via
 * telnet_client_feed(), and reports back through the callback struct,
 * including a send_bytes() callback for outbound replies (WILL/DO
 * negotiation acks) so this file has zero OS dependency and can be
 * unit-tested/reused outside Win32. */

typedef struct telnet_client telnet_client_t;

typedef struct {
    void *ctx;
    /* Outbound bytes this parser needs written back to the server
     * (telnet negotiation replies). Never called with plain typed
     * player input -- that's the caller's own job to send. */
    void (*send_bytes)(void *ctx, const unsigned char *data, size_t len);
    /* Plain display text, telnet-framing-stripped but NOT ansi-
     * stripped -- still contains `\x1b[...m` SGR sequences and CRLF,
     * for ansi_client.c to process next. May be called multiple times
     * per telnet_client_feed() call (text interrupted by an IAC
     * sequence flushes what came before it). */
    void (*on_text)(void *ctx, const char *data, size_t len);
    /* One GMCP message: `package` is "Char.Vitals"/"Room.Info"/etc,
     * `json` is the raw JSON payload text (null-terminated, valid
     * until this callback returns). */
    void (*on_gmcp)(void *ctx, const char *package, const char *json);
    /* One MSDP variable: name/value both null-terminated. */
    void (*on_msdp_var)(void *ctx, const char *name, const char *value);
} telnet_client_callbacks_t;

telnet_client_t *telnet_client_create(const telnet_client_callbacks_t *cb);
void telnet_client_destroy(telnet_client_t *tc);

/* Feed raw bytes just read() from the socket. May trigger any number
 * of the callbacks above, synchronously, before returning. */
void telnet_client_feed(telnet_client_t *tc, const unsigned char *data, size_t len);

#endif
