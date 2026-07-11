#!/usr/bin/env python3
"""Smoke test for linkdead persistence (user 2026-07-09): a character whose
link drops stays in their room -- linkdead, not destroyed -- until the same
account reconnects to it (world_find_linkdead_pc() in world.c/descriptor.c).
  1. After an abrupt disconnect, the room still lists the character (an
     immortal watcher's `look` shows them) -- not silently removed.
  2. Reconnecting as that character resumes them in the SAME room (not their
     load room), proving the live being_t was reused, not a fresh DB load.
  3. A second, unrelated character can still connect normally in the
     meantime (the linkdead body doesn't block anything else).

    python3 tests/smoke_test_linkdead.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
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
    announce(f"done {test_name}", host, port)


announce("smoke_test_linkdead")

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


def set_level(name, level):
    subprocess.run(["mariadb", "sneezy", "-e",
                    f"UPDATE player_progress SET level={level} WHERE player_id="
                    f"(SELECT id FROM player WHERE name='{name}');"], check=True)


def make_char(tag, pw="linkdeadpw123"):
    name = f"Ld{tag}{_suffix}"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s, name


def relog(name, pw="linkdeadpw123"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    return s


# --- an immortal watcher, in the same room, to observe the victim ---
sImm, nameImm = make_char("Imm")
set_level(nameImm, 51)
sImm.close()
sImm = relog(nameImm)

sVictim, nameVictim = make_char("Vic")

# Move the victim one room away from their load room, so "resumed in the
# same room" is distinguishable from "reset to load room".
out = cmd(sVictim, "look")
m = re.search(r"Obvious exits:\s*([a-z ]+)", out)
check(m and m.group(1).split(), "the start room has an obvious exit")
direction = m.group(1).split()[0]
cmd(sVictim, direction)
victim_room_before = cmd(sVictim, "look")
name_line_before = [l for l in victim_room_before.splitlines() if l.strip()][0]

cmd(sImm, f"goto {nameVictim}")

# --- 1: abrupt disconnect -- the room still lists the victim, tagged ---
sVictim.close()
time.sleep(0.5)
out = cmd(sImm, "look")
check(nameVictim.capitalize() in out, "the linkdead victim is still listed in the room")
check(f"{nameVictim.capitalize()} is here. (linkdead)" in out,
      "the listing is tagged (linkdead)")

# --- 1b: no one can target a linkdead character (combat_find_room_target) ---
out = cmd(sImm, f"kill {nameVictim}")
check("aren't here" in out.lower(), "an immortal cannot target a linkdead character with kill")
check("slain" not in out.lower(), "the linkdead victim was NOT slain")

# --- 2: reconnect resumes the SAME room, not the load room ---
# (inline, not the shared relog() helper, so the post-select response --
# the resume/welcome line plus the auto-look -- can be inspected directly)
sVictim2 = socket.create_connection((host, port), timeout=5)
recv_all(sVictim2)
send_line(sVictim2, nameVictim); recv_all(sVictim2)
send_line(sVictim2, "linkdeadpw123"); recv_all(sVictim2)
out = cmd(sVictim2, "1")
check("resume where you left off" in out.lower(), "reconnecting announces resuming, not a fresh Welcome")
out2 = cmd(sVictim2, "look")
name_line_after = [l for l in out2.splitlines() if l.strip()][0]
check(name_line_before == name_line_after,
      "resumed in the same room the link was lost in (not reset to load room)")

# --- 3: an unrelated character connects fine while all this happens ---
sOther, nameOther = make_char("Other")
out3 = cmd(sOther, "look")
check("Obvious exits" in out3, "an unrelated character connects normally")

sImm.close()
sVictim2.close()
sOther.close()
announce_done("smoke_test_linkdead")
print("=== ALL CHECKS PASSED ===")
