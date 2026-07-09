#!/usr/bin/env python3
"""Smoke test for the immortal comms commands wiznet + system.

  1. `wiznet` reaches all online immortals (and the sender), not mortals.
  2. `system` broadcasts a bare line to everyone; the sender sees it prefixed.
  3. Both are hidden from mortals.

    python3 tests/smoke_test_wizcomm.py [host] [port]
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


announce("smoke_test_wizcomm")

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
    for step in (nm, "wcpw", "wcpw", "new", nm, "done"):
        send_line(s, step); recv_all(s)
    return s


def promote(nm, level):
    subprocess.run(["mariadb", "sneezy", "-e",
                    f"UPDATE player_progress SET level={level} WHERE player_id="
                    f"(SELECT id FROM player WHERE name='{nm}');"], check=True)


def relogin(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (nm, "wcpw", "1"):
        send_line(s, step); recv_all(s)
    return s


nameA, nameB, nameC = f"Wca{_suffix}", f"Wcb{_suffix}", f"Wcc{_suffix}"

# A, B immortal; C mortal.
make_char(nameA).close(); make_char(nameB).close()
promote(nameA, 51); promote(nameB, 51)
A = relogin(nameA)
B = relogin(nameB)
C = make_char(nameC)          # stays level 1 mortal
for s in (A, B, C):
    cmd(s, "color off")
recv_all(A); recv_all(B); recv_all(C)

# --- wiznet: reaches immortals, not mortals ---
cmd(A, "wiznet hello staff and friends")
outB = recv_all(B)
outC = recv_all(C)
check("hello staff and friends" in outB and nameA in outB,
      "wiznet reaches another immortal, tagged with the sender's name")
check("[wiznet]" not in outB, "wiznet no longer shows the [wiznet] tag")

# ';' is the one-character shorthand for wiznet.
cmd(A, ";shorthand via semicolon")
check("shorthand via semicolon" in recv_all(B), "; is a shorthand for wiznet")
check("hello staff and friends" not in outC, "wiznet does NOT reach mortals")

# --- system: everyone reads the bare line, sender sees it prefixed ---
out = cmd(A, "system You hear a thud.")
check("system You hear a thud." in out, "the sender sees the system line prefixed")
check("You hear a thud." in recv_all(C), "a mortal reads the bare system line")

# --- gates ---
check("Huh?!" in cmd(C, "wiznet nope"), "wiznet is hidden from mortals")
check("Huh?!" in cmd(C, "system nope"), "system is hidden from mortals")

A.close(); B.close(); C.close()
announce_done("smoke_test_wizcomm")
print("=== ALL CHECKS PASSED ===")
