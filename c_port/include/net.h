/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_NET_H
#define TOBIN_NET_H

#include <stdbool.h>
#include <stddef.h>

/* C replacement for sys/socket.{h,cc}'s TMainSocket/TSocket -- both were
 * already thin wrappers over raw POSIX fds + select(), so this is a direct
 * structural port, not a redesign. */

typedef struct {
    int listen_fd;
} main_socket_t;

bool main_socket_open(main_socket_t *ms, int port);
void main_socket_close(main_socket_t *ms);

/* Accepts one pending connection, if any. Returns the new fd, or -1 if
 * none is pending / on error. Sets the new fd non-blocking with keepalive. */
/* Accepts one pending connection; writes the peer address into ip
 * (dotted quad) for logging. Returns the new fd or -1. */
int main_socket_accept(main_socket_t *ms, char *ip, size_t ip_size);

void socket_set_nonblocking(int fd);
void socket_set_keepalive(int fd, bool enabled);

/* Best-effort write; returns bytes written (may be < len), or -1 on a
 * hard error. Never raises SIGPIPE (caller should ignore it globally). */
int socket_write(int fd, const char *data, size_t len);

#endif
