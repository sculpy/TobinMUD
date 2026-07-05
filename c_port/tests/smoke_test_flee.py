#!/usr/bin/env python3
"""Smoke test for `flee` (cmd_flee.c):
  1. `flee` when not fighting is rejected.
  2. In a fight, flee (retried past its chance-to-fail) breaks combat: the
     fleer ends up not fighting, and their opponent stops fighting too.

    python3 tests/smoke_test_flee.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
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


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "fleepw"); recv_all(s)
    send_line(s, "fleepw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "fleepw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


nameC, nameD = f"Flec{_suffix}", f"Fled{_suffix}"
make_char(nameC).close()
make_char(nameD).close()
for nm in (nameC, nameD):
    sql(f"UPDATE player_progress SET hp=9999, max_hp=9999 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{nm}');")

sC = relogin(nameC)
sD = relogin(nameD)
recv_all(sC); recv_all(sD)

# flee when not fighting
check("aren't fighting" in cmd(sC, "flee"), "flee when not fighting is rejected")

# start a fight
check("You attack" in cmd(sC, f"attack {nameD}"), "attack starts a fight")


def poll_fighting(sock):
    for _ in range(8):
        if "Fighting" in cmd(sock, "score"):
            return True
    return False


check(poll_fighting(sC), "score reads Fighting once combat is underway")

# flee, retrying past the chance-to-fail
fled = False
for _ in range(15):
    out = cmd(sC, "flee")
    if "flee head over heels" in out:
        fled = True
        break
check(fled, "flee eventually succeeds and whisks you away")

# After fleeing, C is no longer fighting...
check("Fighting" not in cmd(sC, "score"), "the fleer is no longer in combat")
# ...and D's fight ended too.
check("Fighting" not in cmd(sD, "score"), "the opponent also stopped fighting")

sC.close(); sD.close()
print("=== ALL CHECKS PASSED ===")
