#!/usr/bin/env python3
"""Smoke test for `help` and `wizhelp`:
  1. `help` lists every mortal-usable command (including `quit!`, which is
     hardcoded in since it's deliberately excluded from the dispatch
     table's abbreviation matching).
  2. `wizhelp` refuses a mortal caller with a clear rejection message
     rather than silently doing nothing.
  3. A mortal's `help` must NOT list immortal-only commands (cmd_dispatch()
     hides over-level commands entirely as of Phase 2A).
  4. `wizhelp` for an immortal (hand-promoted via the DB, same pattern as
     every other immortal-only test) lists the real immortal-only commands
     (`transfer`, `promote` -- `goto` moved to mortal-visible 2026-07-12
     once its landmark forms opened up to everyone), and the immortal's
     `help` includes them too.

    python3 tests/smoke_test_help.py [host] [port]
"""
import socket
import subprocess
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


announce("smoke_test_help")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

MORTAL_COMMANDS = ["look", "exits", "who", "score", "color", "attack", "kill", "say", "limbs", "help", "quit!"]


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
    send_line(s, "y")
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, pw)  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done")
    recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s, name


# --- Part 1: `help` lists every mortal command ---
sA, nameA = make_player("A")
send_line(sA, "help")
out = recv_all(sA)
check("Available commands" in out, "help shows a header")
for cmd in MORTAL_COMMANDS:
    check(cmd in out, f"help lists '{cmd}'")

check("transfer" not in out and "promote" not in out and "wizhelp" not in out,
      "a mortal's help does not leak immortal-only commands (wizhelp included)")

# --- Part 2: wizhelp is INVISIBLE to mortals (Tier 3) ---
send_line(sA, "wizhelp")
out = recv_all(sA)
check("Command not found" in out, "a mortal typing wizhelp gets Command not found (hidden, not just refused)")

# --- Part 3: an immortal sees the real immortal-only list ---
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
check("transfer" in out, "wizhelp lists the level-51 immortal commands")
check("delbug" not in out and "promote" not in out,
      "a level-51's wizhelp does NOT reveal higher-level commands (delbug 59+, promote 58+)")
check("[51+]" not in out and "[51 +]" not in out,
      "wizhelp no longer shows the level tag")

send_line(sA, "help")
out = recv_all(sA)
check("goto" in out, "an immortal's help includes their immortal commands")
check("delbug" not in out and "promote" not in out,
      "a level-51's help does not reveal delbug (59+) or promote (58+)")
check(out.index("catchup") < out.index("goto") < out.index("look"),
      "help lists commands alphabetically (catchup < goto < look)")

sA.close()
announce_done("smoke_test_help")
print("=== ALL CHECKS PASSED ===")
