#!/usr/bin/env python3
"""Smoke test for the account color preference (descriptor.c CONN_GET_COLOR_PREF,
account_repo.c, cmd_color.c):
  1. A new account is asked about color; answering 'n' disables it, and the
     `color` command confirms it is OFF.
  2. The preference persists across a reconnect (account.color_pref).
  3. `color on` re-enables it and also persists.

    python3 tests/smoke_test_color_pref.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test. Self-contained (own socket, doesn't depend on this file's other
    helpers) so it can run at any point in the script."""
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    """Companion to announce() -- emits a [TEST] "finished" log line. Call
    once at the very end of a smoke test, just before it reports success."""
    announce(f"done {test_name}", host, port)


announce("smoke_test_color_pref")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


name = f"Colp{_suffix}"

# --- create the account, decline color, make a character ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)          # new account name
send_line(s, "colppw"); recv_all(s)      # new password
out = cmd(s, "colppw")                    # confirm password -> color prompt
check("Enable it?" in out or "color" in out.lower(),
      "a new account is asked about color at creation")
out = cmd(s, "n")                          # decline color
check("disabled" in out.lower(), "answering 'n' disables color")
# now at account menu -> create a character
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "done"); recv_all(s)         # in game now

check("currently OFF" in cmd(s, "color"), "color reports OFF in game after declining")
s.close()

# --- reconnect: preference must have persisted ---
def relogin():
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, name); recv_all(r)
    send_line(r, "colppw"); recv_all(r)
    send_line(r, "1"); recv_all(r)         # connect the only character
    return r


s = relogin()
check("currently OFF" in cmd(s, "color"), "color preference persisted as OFF across reconnect")

# --- turn it on; it should persist too ---
check("now ON" in cmd(s, "color on"), "color on re-enables color")
s.close()
s = relogin()
check("currently ON" in cmd(s, "color"), "the color-on choice persisted across reconnect")
s.close()

announce_done("smoke_test_color_pref")
print("=== ALL CHECKS PASSED ===")
