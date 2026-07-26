#!/usr/bin/env python3
"""Smoke test for gender + appearance at character creation (descriptor.c,
being.c, player_repo.c, cmd_score.c, cmd_look.c):
  1. `gender male` + `appearance <text>` on the creation screen take effect;
     score shows "Sex: male" and the appearance line.
  2. Both persist across a reconnect (player.gender / player.appearance).
  3. `look <player>` shows another player's appearance.
  4. Looking at a player with no appearance gives a gender-aware
     "nothing special about <him/her/it>" line.

    python3 tests/smoke_test_gender.py [host] [port]
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


announce("smoke_test_gender")

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


def make_char(nm, gender=None, appearance=None):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "genpw"); recv_all(s)
    send_line(s, "genpw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)      # now on the race screen
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    if gender:
        send_line(s, f"gender {gender}"); recv_all(s)
    if appearance:
        send_line(s, f"appearance {appearance}"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "genpw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


nameA = f"Gena{_suffix}"
appearA = "a towering scarred warrior"
sA = make_char(nameA, gender="male", appearance=appearA)

out = cmd(sA, "score")
check("Sex: male" in out, "score shows the chosen gender (male)")
check(appearA in out, "score shows the chosen appearance")

# Persistence across reconnect.
sA.close()
sA = relogin(nameA)
out = cmd(sA, "score")
check("Sex: male" in out and appearA in out,
      "gender and appearance persist across a reconnect")

# A neuter character with no appearance, in the same start room (Center Square).
nameB = f"Genb{_suffix}"
sB = make_char(nameB)  # defaults: neuter, no appearance
recv_all(sA)  # drain B's arrival echo

# A looks at B (neuter, no appearance) -> gender-aware nothing-special line.
out = cmd(sA, f"look {nameB[:4]}")
check("nothing special about it" in out,
      "looking at a neuter player with no appearance uses 'it'")

# B looks at A -> sees A's appearance.
out = cmd(sB, f"look {nameA[:4]}")
check(appearA in out, "look <player> shows that player's appearance")

# Looking at nobody -- and nothing (Phase 2C widened this to also search
# objects, so the wording changed from "anyone" to the more general "that").
check("don't see that" in cmd(sA, "look nosuchperson"),
      "looking at an absent name is rejected")

sA.close(); sB.close()
announce_done("smoke_test_gender")
print("=== ALL CHECKS PASSED ===")
