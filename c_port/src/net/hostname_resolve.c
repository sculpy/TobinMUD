/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "hostname_resolve.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "descriptor.h"

/* Small fixed-size result mailbox between resolver threads and the main
 * loop -- no unbounded queue, no dynamic growth. Sized well above any
 * realistic number of lookups in flight at once (connections don't
 * arrive that fast); a lookup that finds every slot full on completion
 * is simply dropped (best-effort feature, never worth blocking on). */
#define RESOLVE_SLOTS 32

typedef struct {
    bool in_use;
    bool ready;
    int fd;
    char ip[46];
    char hostname[64];
} resolve_slot_t;

static resolve_slot_t g_slots[RESOLVE_SLOTS];
static pthread_mutex_t g_slots_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int fd;
    char ip[46];
} resolve_args_t;

/* Thread entry point for a single background reverse-DNS lookup, spawned
 * by hostname_resolve_start(). Takes ownership of `arg` (frees it before
 * returning either way) and, on success, stashes the resolved hostname
 * in a free/oldest slot for hostname_resolve_poll() to pick up. */
static void *resolve_thread_main(void *arg) {
    resolve_args_t *a = (resolve_args_t *)arg;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    if (inet_pton(AF_INET, a->ip, &sa.sin_addr) == 1) {
        char host[64];
        /* NI_NAMEREQD: fail outright rather than falling back to
         * returning the numeric address again -- an unresolved lookup
         * should leave `hostname` empty (display falls back to `ip`
         * anyway), not "resolve" to the same string we started with. */
        if (getnameinfo((struct sockaddr *)&sa, sizeof(sa), host, sizeof(host),
                         NULL, 0, NI_NAMEREQD) == 0) {
            pthread_mutex_lock(&g_slots_lock);
            /* Reuse a free slot if one exists; otherwise overwrite the
             * oldest still-unread result rather than growing -- losing an
             * unread hostname is harmless (display just keeps showing the
             * raw IP), unlike losing a game event would be. */
            int slot = -1;
            for (int i = 0; i < RESOLVE_SLOTS; i++) {
                if (!g_slots[i].in_use) { slot = i; break; }
            }
            if (slot < 0)
                slot = 0;
            g_slots[slot].in_use = true;
            g_slots[slot].ready = true;
            g_slots[slot].fd = a->fd;
            snprintf(g_slots[slot].ip, sizeof(g_slots[slot].ip), "%s", a->ip);
            snprintf(g_slots[slot].hostname, sizeof(g_slots[slot].hostname), "%s", host);
            pthread_mutex_unlock(&g_slots_lock);
        }
    }

    free(a);
    return NULL;
}

/* Kicks off a background reverse-DNS lookup for `ip`, tagged with `fd` so
 * the result can be matched back to the right descriptor later. Best
 * effort: silently does nothing if the thread can't be created. */
void hostname_resolve_start(int fd, const char *ip) {
    if (!ip || !ip[0])
        return;

    resolve_args_t *a = malloc(sizeof(*a));
    if (!a)
        return;
    a->fd = fd;
    snprintf(a->ip, sizeof(a->ip), "%s", ip);

    pthread_t tid;
    if (pthread_create(&tid, NULL, resolve_thread_main, a) != 0) {
        free(a);
        return;
    }
    pthread_detach(tid);
}

/* Applies any completed lookups to their matching (still-connected, same
 * fd+ip) descriptor's `hostname` field. Called once per game-loop tick. */
void hostname_resolve_poll(void) {
    pthread_mutex_lock(&g_slots_lock);
    for (int i = 0; i < RESOLVE_SLOTS; i++) {
        if (!g_slots[i].in_use || !g_slots[i].ready)
            continue;
        /* Match on fd AND ip together -- a closed connection's fd number
         * can be reused by the OS for a brand-new, unrelated connection
         * before a slow lookup finishes; requiring the ip to still match
         * too makes that (already rare) mismatch essentially impossible. */
        for (descriptor_t *d = g_descriptors; d; d = d->next) {
            if (d->fd == g_slots[i].fd && strcmp(d->ip, g_slots[i].ip) == 0) {
                snprintf(d->hostname, sizeof(d->hostname), "%s", g_slots[i].hostname);
                break;
            }
        }
        g_slots[i].in_use = false;
        g_slots[i].ready = false;
    }
    pthread_mutex_unlock(&g_slots_lock);
}
