/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
#ifndef TOBINCLIENT_ANSI_CLIENT_H
#define TOBINCLIENT_ANSI_CLIENT_H

#include <stddef.h>

/* Turns a byte stream containing ANSI SGR escapes (`\x1b[...m`, what
 * the server's own colorstring.c emits from its `<X>` tags) into
 * plain-text runs tagged with a color index, for the GUI layer to
 * render (Win32 RichEdit SetCharFormat, one run at a time). Resumable
 * across ansi_client_feed() calls -- an escape sequence split across
 * two socket reads is handled correctly, not lost or misrendered.
 *
 * color_index is the standard 8-color ANSI index (0=black, 1=red,
 * 2=green, 3=yellow, 4=blue, 5=magenta, 6=cyan, 7=white); `bold`
 * selects the bright variant, matching SGR 1 / 30-37 vs 90-97. -1 for
 * color_index means "default/reset" -- render in the normal
 * foreground color. */

typedef struct ansi_client ansi_client_t;

typedef struct {
    void *ctx;
    void (*emit)(void *ctx, const char *text, size_t len, int color_index, int bold);
} ansi_client_callbacks_t;

ansi_client_t *ansi_client_create(const ansi_client_callbacks_t *cb);
void ansi_client_destroy(ansi_client_t *ac);
void ansi_client_feed(ansi_client_t *ac, const char *data, size_t len);

#endif
