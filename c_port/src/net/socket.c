/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "net.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

/* Puts fd into non-blocking mode so read()/write()/accept() on it never
 * stall the single-threaded select() loop; a would-block turns into
 * EAGAIN instead of hanging. */
void socket_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Toggles TCP keepalive on fd so a peer that vanishes without closing
 * cleanly (network drop, crashed client) is eventually detected instead
 * of leaving a dead connection open forever. */
void socket_set_keepalive(int fd, bool enabled) {
    int val = enabled ? 1 : 0;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
}

/* Best-effort write of len bytes to fd. A would-block or a peer that's
 * already gone (EPIPE) is treated as "wrote nothing" rather than an
 * error, since the caller's send buffer/retry logic handles a partial
 * or zero write; only a genuine hard error returns -1. Never raises
 * SIGPIPE (caller is expected to ignore it globally). */
int socket_write(int fd, const char *data, size_t len) {
    ssize_t n = write(fd, data, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE)
            return 0;
        return -1;
    }
    return (int)n;
}
