/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>

#include "being.h"
#include "log.h"

/* `exec <shell command>`: run a command on the host box from in game.
 * Implementor-only (level 60) admin tool. This is deliberately powerful, so
 * it is fenced three ways:
 *   1. A blocklist rejects commands that could harm the game or the box
 *      (process kills, disk wipes, reboots, privilege escalation, touching
 *      the mud binary/process, ...).
 *   2. Every command runs under `timeout` so a runaway can't freeze the
 *      single-threaded game loop.
 *   3. Every invocation is logged (who + the command) for an audit trail.
 * The game runs as an unprivileged user, so this is not a root shell. */

#define EXEC_MIN_LEVEL 60
#define EXEC_TIMEOUT_SECS 10
#define EXEC_OUTPUT_MAX 8192

/* Case-insensitive substrings that, if present anywhere in the command, get
 * it refused. Broad on purpose -- an implementor who needs one of these can
 * do it from a real shell. */
static const char *const BLOCKED[] = {
    "rm ", "rm\t", "rmdir", "unlink", "shred", "mkfs", "fdisk", "dd ",
    "kill", "pkill", "killall", "shutdown", "reboot", "halt", "poweroff",
    "init ", "systemctl", "service ", "mount", "umount", "chown", "chmod ",
    "passwd", "useradd", "userdel", "sudo", "doas", "su ", "> /dev", "/dev/sd",
    ":(){", "fork", "tobin_c", "mysqld", "mariadb", "drop ", "> /etc", "/etc/",
    "crontab", "iptables", "nft ", "curl ", "wget ", "nc ", "ncat",
};
#define NUM_BLOCKED (sizeof(BLOCKED) / sizeof(BLOCKED[0]))

/* Case-insensitive "does haystack contain needle" (strcasestr is GNU-only). */
static bool ci_contains(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++)
        if (strncasecmp(p, needle, nlen) == 0)
            return true;
    return false;
}

bool cmd_exec(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args == ' ')
        args++;
    if (!*args) {
        descriptor_send(d, "Usage: exec <shell command>\r\n");
        return true;
    }

    for (size_t i = 0; i < NUM_BLOCKED; i++) {
        if (ci_contains(args, BLOCKED[i])) {
            descriptor_send(d, "<r>Refused:<z> that command is on the exec blocklist.\r\n");
            log_info("EXEC REFUSED: %s tried '%s'", ch->base.name, args);
            return true;
        }
    }

    /* Audit every accepted invocation before running it. */
    log_info("EXEC: %s ran '%s'", ch->base.name, args);

    /* Wrap in `timeout` so a runaway can't wedge the game loop. popen runs it
     * via `sh -c`, so shell syntax (pipes, redirects) still works. */
    char cmdline[1024];
    snprintf(cmdline, sizeof(cmdline), "timeout %d %s 2>&1", EXEC_TIMEOUT_SECS, args);

    FILE *fp = popen(cmdline, "r");
    if (!fp) {
        descriptor_send(d, "<r>exec: could not start the command.<z>\r\n");
        return true;
    }

    char out[EXEC_OUTPUT_MAX];
    size_t n = 0;
    int c;
    while (n + 1 < sizeof(out) && (c = fgetc(fp)) != EOF) {
        /* Normalize bare LF to CRLF for the telnet client. */
        if (c == '\n' && (n == 0 || out[n - 1] != '\r') && n + 2 < sizeof(out))
            out[n++] = '\r';
        out[n++] = (char)c;
    }
    out[n] = '\0';
    int status = pclose(fp);
    int rc = (status != -1 && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;

    descriptor_send(d, "\r\n<c>-- exec output --<z>\r\n");
    if (n == 0)
        descriptor_send(d, "<k>(no output)<z>\r\n");
    else
        descriptor_send(d, out);
    /* `timeout` exits 124 when it had to kill the command. */
    if (rc != 0) {
        char tail[96];
        if (rc == 124)
            snprintf(tail, sizeof(tail), "<r>[command timed out after %ds]<z>\r\n", EXEC_TIMEOUT_SECS);
        else
            snprintf(tail, sizeof(tail), "<k>[exit status %d]<z>\r\n", rc);
        descriptor_send(d, tail);
    }
    return true;
}
