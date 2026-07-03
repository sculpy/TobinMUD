#include "net.h"

#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"

bool main_socket_open(main_socket_t *ms, int port) {
    ms->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ms->listen_fd < 0) {
        log_error("socket() failed");
        return false;
    }

    int opt = 1;
    setsockopt(ms->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);

    if (bind(ms->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("bind() failed on port %d", port);
        close(ms->listen_fd);
        ms->listen_fd = -1;
        return false;
    }

    if (listen(ms->listen_fd, 16) < 0) {
        log_error("listen() failed on port %d", port);
        close(ms->listen_fd);
        ms->listen_fd = -1;
        return false;
    }

    socket_set_nonblocking(ms->listen_fd);
    return true;
}

void main_socket_close(main_socket_t *ms) {
    if (ms->listen_fd >= 0) {
        close(ms->listen_fd);
        ms->listen_fd = -1;
    }
}

int main_socket_accept(main_socket_t *ms) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int fd = accept(ms->listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (fd < 0)
        return -1;

    socket_set_nonblocking(fd);
    socket_set_keepalive(fd, true);
    return fd;
}
