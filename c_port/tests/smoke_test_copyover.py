#!/usr/bin/env python3
"""Smoke test for `copyover` (Erwin Andreasen-style hot reboot):
  1. Gate: mortals (and even a level-58 Greater God) get Huh?! -- the
     command is Administrator (59) and up.
  2. Player connections SURVIVE the copyover: both a playing immortal and
     a playing mortal keep their sockets, see the reborn/reforms
     messages, and can run commands afterward. The immortal's non-home
     location (The Void, via goto) is preserved -- copyover restores
     where you were standing, not your load room.
  3. Fighting is stopped by the copyover and does not resume after it.
  4. A connection still sitting at the login prompt is told to reconnect
     and dropped (not resumable mid-dialog).
  5. The reborn server still accepts NEW connections (the listening
     socket survived the exec).

    python3 tests/smoke_test_copyover.py [host] [port]
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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def set_level(name, level):
    subprocess.run(
        ["mariadb", "sneezy", "-e",
         f"UPDATE player_progress SET level={level} WHERE player_id=(SELECT id FROM player WHERE name='{name}');"],
        check=True,
    )


def make_player(tag):
    name = f"Cpo{tag}{_suffix}"
    pw = "copyoverpw"
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


def relog(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "copyoverpw")
    recv_all(s)
    send_line(s, "1")
    recv_all(s)
    return s


sImm, nameImm = make_player("Imm")
sMort, nameMort = make_player("Mort")

# --- Part 1: the gate ---
send_line(sMort, "copyover")
check("Huh?!" in recv_all(sMort), "a mortal typing copyover gets Huh?! (hidden)")

set_level(nameImm, 58)
sImm.close()
sImm = relog(nameImm)
send_line(sImm, "copyover")
check("Huh?!" in recv_all(sImm), "even a level-58 Greater God can't copyover (gate is 59)")

set_level(nameImm, 59)
sImm.close()
sImm = relog(nameImm)

# --- Setup for parts 2-4: a fight in progress, a non-home room, a login-state conn ---
send_line(sMort, f"attack {nameImm}")
recv_all(sMort)  # the fight starts; rounds now resolve on the global pulse

send_line(sImm, "goto 0")
out = recv_all(sImm)
check("The Void" in out, "immortal relocated to The Void before the copyover")

sLogin = socket.create_connection((host, port), timeout=5)
recv_all(sLogin)  # sitting at the account-name prompt

# --- The copyover itself ---
send_line(sImm, "copyover")
warn = recv_all(sImm, timeout=2.0)
check("COPYOVER in 5 seconds" in warn,
      "everyone is warned 5 seconds before the copyover")
time.sleep(7)  # the 5s warning window + exec + DB reconnect + recovery

out_imm = recv_all(sImm, timeout=2.0)
check("world is reborn" in out_imm or "Time stops" in out_imm,
      "the initiator saw the pre-exec announcement")
check("copyover complete" in out_imm, "the initiator's connection survived the exec")
check("The Void" in out_imm, "the initiator is still standing in The Void (not load room)")

out_mort = recv_all(sMort, timeout=2.0)
check("copyover complete" in out_mort, "the other player's connection survived too")

out_login = recv_all(sLogin, timeout=2.0)
check("please reconnect" in out_login, "the login-state connection was told to reconnect")
dropped = False
try:
    sLogin.settimeout(2.0)
    dropped = sLogin.recv(1024) == b""  # EOF = closed by the exec
except ConnectionError:
    dropped = True  # a reset proves the same thing
except socket.timeout:
    dropped = False  # still open -- that would be a leak
check(dropped, "the login-state connection was dropped by the exec")
sLogin.close()

# --- Part 3: commands work, fighting stayed stopped ---
send_line(sImm, "who")
out = recv_all(sImm)
check(nameImm.capitalize() in out and nameMort.capitalize() in out,
      "who works after the copyover and lists both survivors")

# No combat messages should arrive on their own anymore.
quiet_imm = recv_all(sImm, timeout=2.5)
quiet_mort = recv_all(sMort, timeout=2.5)
check("hits you" not in quiet_imm and "hits you" not in quiet_mort
      and "You hit" not in quiet_imm and "You hit" not in quiet_mort,
      "the pre-copyover fight did not resume after the reboot")

send_line(sMort, "score")
out = recv_all(sMort)
check("Level" in out, "the mortal can run commands normally after the copyover")

# --- Part 5: brand-new connections still work (listen fd survived) ---
sNew = socket.create_connection((host, port), timeout=5)
out = recv_all(sNew)
check("Account name" in out, "a brand-new connection is greeted by the reborn server")
sNew.close()

# hygiene
set_level(nameImm, 1)
sImm.close()
sMort.close()
print("=== ALL CHECKS PASSED ===")
