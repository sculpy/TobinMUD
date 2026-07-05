/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "net.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

void socket_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void socket_set_keepalive(int fd, bool enabled) {
    int val = enabled ? 1 : 0;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val));
}

int socket_write(int fd, const char *data, size_t len) {
    ssize_t n = write(fd, data, len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EPIPE)
            return 0;
        return -1;
    }
    return (int)n;
}
