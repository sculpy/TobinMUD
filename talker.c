/*
 * talker.c - simple multi-user chat/talker server (MUD-style)
 *
 * Clients connect over TCP (e.g. `telnet host 4000`), pick a name, and
 * broadcast chat with `say <message>` (or the `'<message>` shorthand).
 * Players can also fight: `attack <name> <limb>` targets a specific
 * body part (head, torso, larm, rarm, lleg, rleg); `status` shows your
 * own condition.
 *
 * Build (Fedora / Linux, needs gcc + pthreads):
 *   gcc -Wall -Wextra -O2 -o talker talker.c -lpthread
 *
 * Run:
 *   ./talker [port]        (defaults to 4000)
 *
 * Connect from another terminal:
 *   telnet localhost 4000
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT   4000
#define MAX_CLIENTS    64
#define NAME_LEN       32
#define BUF_LEN        512
#define BACKLOG        16

/* Minimal telnet protocol bytes (RFC 854) needed to negotiate
 * server-side echo and character-at-a-time mode, so a real telnet
 * client doesn't buffer/echo locally and send one line as N packets. */
#define TN_IAC  255
#define TN_WILL 251
#define TN_WONT 252
#define TN_DO   253
#define TN_DONT 254
#define TN_SB   250
#define TN_SE   240
#define TN_ECHO 1
#define TN_SGA  3

/* Limb-based combat: each player tracks HP per body part. A killing
 * blow lands on the head or torso; a limb at 0 HP is "shattered" and
 * arms are required to attack. */
#define HP_HEAD_MAX  15
#define HP_TORSO_MAX 25
#define HP_LIMB_MAX  10 /* arms and legs share this cap */

typedef struct {
    int  fd;
    char name[NAME_LEN];
    int  in_use;
    int  hp_head;
    int  hp_torso;
    int  hp_larm;
    int  hp_rarm;
    int  hp_lleg;
    int  hp_rleg;
} client_t;

static client_t clients[MAX_CLIENTS];
static pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t shutting_down = 0;

/* Send a line to a single client, ignoring errors (client may have left). */
static void send_line(int fd, const char *msg) {
    size_t len = strlen(msg);
    ssize_t n = write(fd, msg, len);
    (void)n; /* best-effort; disconnects are caught on the read side */
}

/* Broadcast a line to every connected client except `except_fd` (-1 = everyone). */
static void broadcast(const char *msg, int except_fd) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].fd != except_fd) {
            send_line(clients[i].fd, msg);
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

static void list_who(int fd) {
    char line[BUF_LEN];
    send_line(fd, "-- Who's online --\r\n");
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use) {
            snprintf(line, sizeof(line), "  %s\r\n", clients[i].name);
            send_line(fd, line);
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

/* Broadcast a chat line as "Name says, "text"". Leading spaces in `text`
 * are skipped; an empty message after that is rejected (sender-only). */
static void do_say(int fd, const char *name, const char *text) {
    while (*text == ' ') text++;
    if (*text == '\0') {
        send_line(fd, "Say what?\r\n");
        return;
    }
    char msg[BUF_LEN];
    snprintf(msg, sizeof(msg), "%.*s says, \"%.*s\"\r\n",
             NAME_LEN - 1, name,
             (int)(sizeof(msg) - NAME_LEN - 12), text);
    broadcast(msg, -1);
}

static void reset_limbs(client_t *c) {
    c->hp_head  = HP_HEAD_MAX;
    c->hp_torso = HP_TORSO_MAX;
    c->hp_larm  = HP_LIMB_MAX;
    c->hp_rarm  = HP_LIMB_MAX;
    c->hp_lleg  = HP_LIMB_MAX;
    c->hp_rleg  = HP_LIMB_MAX;
}

/* Map a limb token (from the "attack" command) to the target's HP field.
 * "arm"/"leg" pick a side at random. Returns NULL for an unknown token. */
static int *limb_field(client_t *c, const char *limb, unsigned int *seed) {
    if (strcasecmp(limb, "head") == 0) return &c->hp_head;
    if (strcasecmp(limb, "torso") == 0 || strcasecmp(limb, "chest") == 0) return &c->hp_torso;
    if (strcasecmp(limb, "larm") == 0 || strcasecmp(limb, "leftarm") == 0) return &c->hp_larm;
    if (strcasecmp(limb, "rarm") == 0 || strcasecmp(limb, "rightarm") == 0) return &c->hp_rarm;
    if (strcasecmp(limb, "lleg") == 0 || strcasecmp(limb, "leftleg") == 0) return &c->hp_lleg;
    if (strcasecmp(limb, "rleg") == 0 || strcasecmp(limb, "rightleg") == 0) return &c->hp_rleg;
    if (strcasecmp(limb, "arm") == 0) return (rand_r(seed) % 2) ? &c->hp_larm : &c->hp_rarm;
    if (strcasecmp(limb, "leg") == 0) return (rand_r(seed) % 2) ? &c->hp_lleg : &c->hp_rleg;
    return NULL;
}

static const char *limb_display(int *field, client_t *c) {
    if (field == &c->hp_head)  return "head";
    if (field == &c->hp_torso) return "torso";
    if (field == &c->hp_larm)  return "left arm";
    if (field == &c->hp_rarm)  return "right arm";
    if (field == &c->hp_lleg)  return "left leg";
    return "right leg";
}

/* Resolve "attack <name> <limb>" and apply damage. Limbs at 0 HP are
 * shattered; a head/torso kill respawns the victim at full health.
 * A player whose arms are both shattered can't attack. */
static void do_attack(int fd, const char *attacker_name, const char *args, unsigned int *seed) {
    while (*args == ' ') args++;

    char target_name[NAME_LEN];
    char limb_tok[16];
    int got = sscanf(args, "%31s %15s", target_name, limb_tok);
    if (got < 1) {
        send_line(fd, "Attack whom? Usage: attack <name> <limb>\r\n");
        return;
    }
    if (got < 2) snprintf(limb_tok, sizeof(limb_tok), "torso");

    pthread_mutex_lock(&clients_lock);

    client_t *attacker = NULL, *target = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].in_use) continue;
        if (clients[i].fd == fd) attacker = &clients[i];
        if (strcasecmp(clients[i].name, target_name) == 0) target = &clients[i];
    }

    if (!attacker) { pthread_mutex_unlock(&clients_lock); return; }

    if (!target) {
        pthread_mutex_unlock(&clients_lock);
        send_line(fd, "No one by that name is here.\r\n");
        return;
    }
    if (target == attacker) {
        pthread_mutex_unlock(&clients_lock);
        send_line(fd, "You can't attack yourself.\r\n");
        return;
    }
    if (attacker->hp_larm <= 0 && attacker->hp_rarm <= 0) {
        pthread_mutex_unlock(&clients_lock);
        send_line(fd, "Both your arms are shattered -- you can't attack!\r\n");
        return;
    }

    int *field = limb_field(target, limb_tok, seed);
    if (!field) {
        pthread_mutex_unlock(&clients_lock);
        send_line(fd, "Unknown limb. Try: head, torso, arm, larm, rarm, leg, lleg, rleg.\r\n");
        return;
    }

    int was_up = (*field > 0);
    int dmg = 3 + (int)(rand_r(seed) % 6); /* 3-8 */
    *field -= dmg;
    if (*field < 0) *field = 0;

    const char *limb_str = limb_display(field, target);
    int shattered_now = was_up && *field == 0;
    int remaining = *field;
    int died = (target->hp_head <= 0 || target->hp_torso <= 0);

    char attacker_copy[NAME_LEN];
    char target_copy[NAME_LEN];
    snprintf(attacker_copy, sizeof(attacker_copy), "%s", attacker_name);
    snprintf(target_copy, sizeof(target_copy), "%s", target->name);

    if (died) reset_limbs(target);

    pthread_mutex_unlock(&clients_lock);

    char out[BUF_LEN];
    if (died) {
        snprintf(out, sizeof(out),
                 "%.28s lands a brutal hit on %.28s's %s for %d damage -- %.28s collapses, "
                 "defeated! %.28s staggers back up, wounds mended.\r\n",
                 attacker_copy, target_copy, limb_str, dmg, target_copy, target_copy);
    } else if (shattered_now) {
        snprintf(out, sizeof(out),
                 "%.28s hits %.28s's %s for %d damage -- the %s is shattered!\r\n",
                 attacker_copy, target_copy, limb_str, dmg, limb_str);
    } else {
        snprintf(out, sizeof(out),
                 "%.28s hits %.28s's %s for %d damage. (%s HP: %d)\r\n",
                 attacker_copy, target_copy, limb_str, dmg, limb_str, remaining);
    }
    broadcast(out, -1);
}

static void do_status(int fd) {
    char msg[BUF_LEN];
    int found = 0;

    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].fd == fd) {
            found = 1;
            snprintf(msg, sizeof(msg),
                     "-- Your condition --\r\n"
                     "  head:      %2d/%2d\r\n"
                     "  torso:     %2d/%2d\r\n"
                     "  left arm:  %2d/%2d\r\n"
                     "  right arm: %2d/%2d\r\n"
                     "  left leg:  %2d/%2d\r\n"
                     "  right leg: %2d/%2d\r\n",
                     clients[i].hp_head,  HP_HEAD_MAX,
                     clients[i].hp_torso, HP_TORSO_MAX,
                     clients[i].hp_larm,  HP_LIMB_MAX,
                     clients[i].hp_rarm,  HP_LIMB_MAX,
                     clients[i].hp_lleg,  HP_LIMB_MAX,
                     clients[i].hp_rleg,  HP_LIMB_MAX);
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);

    if (found) send_line(fd, msg);
}

/* Per-connection raw-byte buffer, so telnet_read_line() can pull one
 * line at a time out of a socket that delivers arbitrary chunks. */
typedef struct {
    int fd;
    unsigned char raw[BUF_LEN];
    int raw_len;
    int raw_pos;
} conn_t;

/* Refill from the socket when the buffer is empty; hands back one byte.
 * Returns 0 on EOF/error, 1 on success. */
static int next_byte(conn_t *c, unsigned char *out) {
    if (c->raw_pos >= c->raw_len) {
        ssize_t n = read(c->fd, c->raw, sizeof(c->raw));
        if (n <= 0) return 0;
        c->raw_len = (int)n;
        c->raw_pos = 0;
    }
    *out = c->raw[c->raw_pos++];
    return 1;
}

/* Read one line of user input. Consumes and discards telnet IAC
 * negotiation replies, handles backspace/DEL, and echoes typed
 * characters back to the client (we negotiate WILL ECHO, so the
 * client itself won't echo locally). Returns line length (>= 0),
 * or -1 on disconnect. */
static int telnet_read_line(conn_t *c, char *out, size_t outsz) {
    size_t len = 0;
    unsigned char b;

    while (next_byte(c, &b)) {
        if (b == TN_IAC) {
            unsigned char cmd;
            if (!next_byte(c, &cmd)) return -1;
            if (cmd == TN_WILL || cmd == TN_WONT || cmd == TN_DO || cmd == TN_DONT) {
                unsigned char opt;
                if (!next_byte(c, &opt)) return -1;
            } else if (cmd == TN_SB) {
                unsigned char prev = 0, sb;
                do {
                    if (!next_byte(c, &sb)) return -1;
                    if (prev == TN_IAC && sb == TN_SE) break;
                    prev = sb;
                } while (1);
            }
            continue;
        }

        if (b == '\r' || b == '\n') {
            /* Swallow a paired \n riding right behind \r in the same packet. */
            if (b == '\r' && c->raw_pos < c->raw_len && c->raw[c->raw_pos] == '\n') {
                c->raw_pos++;
            }
            send_line(c->fd, "\r\n");
            out[len] = '\0';
            return (int)len;
        }

        if (b == 8 || b == 127) { /* backspace / DEL */
            if (len > 0) {
                len--;
                send_line(c->fd, "\b \b");
            }
            continue;
        }

        if (b < 32) continue; /* drop other control bytes */

        if (len + 1 < outsz) {
            out[len++] = (char)b;
            char echo[2] = { (char)b, '\0' };
            send_line(c->fd, echo);
        }
    }
    return -1;
}

static int add_client(int fd, const char *name) {
    pthread_mutex_lock(&clients_lock);
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].in_use) { slot = i; break; }
    }
    if (slot >= 0) {
        clients[slot].fd = fd;
        clients[slot].in_use = 1;
        snprintf(clients[slot].name, NAME_LEN, "%s", name);
        reset_limbs(&clients[slot]);
    }
    pthread_mutex_unlock(&clients_lock);
    return slot;
}

static void remove_client(int fd) {
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].in_use && clients[i].fd == fd) {
            clients[i].in_use = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_lock);
}

static void *client_thread(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    conn_t conn = { .fd = fd, .raw_len = 0, .raw_pos = 0 };
    unsigned int rng_seed = (unsigned int)time(NULL) ^ (unsigned int)fd ^ (unsigned int)(size_t)&conn;

    /* Ask the telnet client to let us echo and to send character-at-a-time
     * instead of buffering a whole line client-side. */
    unsigned char negotiate[] = {
        TN_IAC, TN_WILL, TN_ECHO,
        TN_IAC, TN_WILL, TN_SGA,
        TN_IAC, TN_DO,   TN_SGA,
    };
    write(fd, negotiate, sizeof(negotiate));

    char buf[BUF_LEN];
    char name[NAME_LEN];

    send_line(fd, "Welcome to the talker. Enter your name: ");
    int n = telnet_read_line(&conn, buf, sizeof(buf));
    if (n < 0) { close(fd); return NULL; }
    if (buf[0] == '\0') snprintf(buf, sizeof(buf), "Anonymous");
    snprintf(name, sizeof(name), "%.*s", (int)sizeof(name) - 1, buf);

    if (add_client(fd, name) < 0) {
        send_line(fd, "Server full, try again later.\r\n");
        close(fd);
        return NULL;
    }

    char msg[BUF_LEN];
    snprintf(msg, sizeof(msg), "*** %s has joined ***\r\n", name);
    broadcast(msg, fd);
    snprintf(msg, sizeof(msg),
             "Hi %s! Type say <message> (or '<message>) to talk, "
             "attack <name> <limb> to fight, status for your condition, "
             "/who to list users, /quit to leave.\r\n", name);
    send_line(fd, msg);

    while ((n = telnet_read_line(&conn, buf, sizeof(buf))) >= 0) {
        if (buf[0] == '\0') continue;

        if (strcmp(buf, "/quit") == 0) {
            break;
        } else if (strcmp(buf, "/who") == 0) {
            list_who(fd);
        } else if (strcmp(buf, "status") == 0 || strcmp(buf, "health") == 0) {
            do_status(fd);
        } else if (buf[0] == '\'') {
            do_say(fd, name, buf + 1);
        } else if (strncmp(buf, "say", 3) == 0 && (buf[3] == '\0' || buf[3] == ' ')) {
            do_say(fd, name, buf + 3);
        } else if (strncmp(buf, "attack", 6) == 0 && (buf[6] == '\0' || buf[6] == ' ')) {
            do_attack(fd, name, buf + 6, &rng_seed);
        } else {
            send_line(fd, "Huh? Type say <message> (or '<message>) to talk.\r\n");
        }
    }

    remove_client(fd);
    snprintf(msg, sizeof(msg), "*** %s has left ***\r\n", name);
    broadcast(msg, fd);
    close(fd);
    return NULL;
}

static void handle_sigint(int sig) {
    (void)sig;
    shutting_down = 1;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN); /* don't die if a client disconnects mid-write */

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("Talker server listening on port %d. Press Ctrl+C to stop.\n", port);

    while (!shutting_down) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        int *fd_arg = malloc(sizeof(int));
        *fd_arg = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, fd_arg) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(fd_arg);
            continue;
        }
        pthread_detach(tid);
    }

    printf("Shutting down.\n");
    close(listen_fd);
    return 0;
}
