#!/usr/bin/env python3
"""Smoke test for the newbie channel + flag (cmd_newbie.c, toggle newbie):
  1. New players are on the channel by default; `newbie <msg>` reaches other
     on-channel players and echoes to the sender.
  2. `toggle newbie` off removes you: you stop receiving, and can't speak.
  3. The off state persists across a reconnect (player.pflags).

    python3 tests/smoke_test_newbie.py [host] [port]
"""
import re
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


announce("smoke_test_newbie")

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


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "nbpw"); recv_all(s)
    send_line(s, "nbpw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "nbpw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


nameA, nameB = f"Nba{_suffix}", f"Nbb{_suffix}"
sA = make_char(nameA)
sB = make_char(nameB)
recv_all(sA); recv_all(sB)

# default on: A speaks, B hears, A sees own echo
out = cmd(sA, "newbie hello there")
check("You: hello there" in out, "the sender sees their own newbie line")
check("hello there" in recv_all(sB), "an on-channel player hears the newbie line")

# B leaves the channel
off_msg = re.sub(r"\x1b\[[0-9;]*m", "", cmd(sB, "toggle newbie"))
check("now off" in off_msg, "toggle newbie turns the channel off")
recv_all(sA)
cmd(sA, "newbie second message")
check("second message" not in recv_all(sB),
      "an off-channel player no longer receives newbie chat")
check("left the newbie channel" in cmd(sB, "newbie hi"),
      "you can't speak on the channel while off it")

# persistence
sB.close()
sB = relogin(nameB)
plain = re.sub(r"\x1b\[[0-9;]*m", "", cmd(sB, "toggle"))
nb_line = [l for l in plain.splitlines() if l.strip().startswith("newbie")]
check(bool(nb_line) and "off" in nb_line[0],
      "the newbie-off state persisted across reconnect")

sA.close(); sB.close()
announce_done("smoke_test_newbie")
print("=== ALL CHECKS PASSED ===")
