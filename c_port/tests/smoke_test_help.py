#!/usr/bin/env python3
"""Smoke test for `help` and `wizhelp`:
  1. `help` lists every mortal-usable command (including `quit!`, which is
     hardcoded in since it's deliberately excluded from the dispatch
     table's abbreviation matching).
  2. `wizhelp` refuses a mortal caller with a clear rejection message
     rather than silently doing nothing.
  3. `wizhelp` for an immortal (hand-promoted via the DB, same pattern as
     every other immortal-only test) shows the immortal-only command
     section -- currently empty ("none yet"), since no command in Tobin
     is min_level-gated to immortals yet.

    python3 tests/smoke_test_help.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = str(int(time.time()) % 100000)

MORTAL_COMMANDS = ["look", "who", "score", "color", "attack", "kill", "say", "limbs", "help", "wizhelp", "quit!"]


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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def make_player(tag):
    name = f"Help{tag}{_suffix}"
    pw = "helptestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "done")
    recv_all(s)
    return s, name


# --- Part 1: `help` lists every mortal command ---
sA, nameA = make_player("A")
send_line(sA, "help")
out = recv_all(sA)
check("Available commands" in out, "help shows a header")
for cmd in MORTAL_COMMANDS:
    check(cmd in out, f"help lists '{cmd}'")

# --- Part 2: a mortal calling wizhelp is rejected, not ignored ---
send_line(sA, "wizhelp")
out = recv_all(sA)
check("not privileged" in out, "a mortal calling wizhelp is rejected with a clear message")

# --- Part 3: an immortal calling wizhelp sees the (currently empty) list ---
subprocess.run(
    ["mariadb", "sneezy", "-e",
     f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{nameA}');"],
    check=True,
)
sA.close()
sA = socket.create_connection((host, port), timeout=5)
recv_all(sA)
send_line(sA, nameA)
recv_all(sA)
send_line(sA, "helptestpw123")
recv_all(sA)
send_line(sA, "1")
recv_all(sA)

send_line(sA, "wizhelp")
out = recv_all(sA)
check("Immortal-only commands" in out, "an immortal calling wizhelp sees the immortal-only header")
check("none yet" in out, "wizhelp honestly reports no immortal-only commands exist yet")

sA.close()
print("=== ALL CHECKS PASSED ===")
