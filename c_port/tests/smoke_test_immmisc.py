#!/usr/bin/env python3
"""Smoke test for a few immortal conveniences:
  1. `goto <player>` teleports to that online player's room (not just a vnum).
  2. `help edit` lists the ed* editor commands.
  3. who/score tint an immortal's name by rank tier (color escape present for
     an immortal, absent for a mortal).

    python3 tests/smoke_test_immmisc.py [host] [port]
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


announce("smoke_test_immmisc")

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
    for step in (nm, "y", "immpw", "immpw", "new", nm, "done"):
        send_line(s, step); recv_all(s)
    return s


def promote(nm, level):
    subprocess.run(["mariadb", "sneezy", "-e",
                    f"UPDATE player_progress SET level={level} WHERE player_id="
                    f"(SELECT id FROM player WHERE name='{nm}');"], check=True)


def relogin(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (nm, "immpw", "1"):
        send_line(s, step); recv_all(s)
    return s


nameA, nameB, nameC = f"Imma{_suffix}", f"Immb{_suffix}", f"Immc{_suffix}"
make_char(nameA).close(); make_char(nameB).close()
promote(nameA, 59); promote(nameB, 51)
A = relogin(nameA)
B = relogin(nameB)
C = make_char(nameC)          # mortal
recv_all(A); recv_all(B); recv_all(C)

# --- goto <player> ---
cmd(A, "goto 100")            # A to Center Square
cmd(A, "color off")
cmd(B, "color off")
out = cmd(B, f"goto {nameA}")
check("Center Square" in out, "goto <player> teleports to that player's room")

# --- help edit ---
out = cmd(A, "help edit")
check("edroom" in out and "ednews" in out and "Editor" in out,
      "help edit lists the ed* editor commands")

# --- rank color in score (color ON) ---
cmd(A, "color on")
out = cmd(A, "score")
check("\x1b[" in out, "an immortal's score name carries a rank color (ANSI escape present)")

cmd(C, "color on")
outC = cmd(C, "score")
check("\x1b[" not in outC, "a mortal's score has no rank color")

A.close(); B.close(); C.close()
announce_done("smoke_test_immmisc")
print("=== ALL CHECKS PASSED ===")
