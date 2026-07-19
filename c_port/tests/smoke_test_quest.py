#!/usr/bin/env python3
"""Smoke test for the quest system (Sneezy → Tobin feature audit, "Quest
system"). User, AskUserQuestion 2026-07-19: infrastructure only -- no
actual quest content, no conditional trigger scripting. Sneezy's real
system is a fixed 454-bit array tied to hand-authored content that doesn't
exist in Tobin; this ports the SHAPE of it (a player's stage in a named
quest, visible only where an immortal has written a description for that
exact stage) rather than the meaningless bit numbers. Covers:
  1. A stage with no matching quest_def is invisible (`quest` lists
     nothing, `quest <name>` says not on it) -- same "only bits with a
     help file are visible" rule as the original.
  2. Once an immortal writes a description (`questdef`) and sets a
     player's stage (`set <player> quest <name> <stage>`), it appears in
     both the summary list and the full `quest <name>` view.
  3. Advancing to a stage with no description makes it disappear from the
     list again, even though the player's stage number really did change.
  4. Setting stage 0 clears the quest entirely.

    python3 tests/smoke_test_quest.py [host] [port]
"""
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


announce("smoke_test_quest")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
quest_name = f"testquest{_suffix}"


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


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name, imm_pw = f"Qimmb{_suffix}", "qimmpw1234"
mort_name, mort_pw = f"Qmortb{_suffix}", "qmortpw1234"

si = make_char(imm_name, imm_pw); si.close()
# SET_MIN_LEVEL is 58, not the usual IMMORTAL_LEVEL_MIN (51) -- below 58,
# "set" itself resolves to "setsev" instead (cmd_table.c's own comment on
# that collision), so this needs 58+, not just any immortal level.
sql(f"UPDATE player_progress SET level=58 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sA = make_char(mort_name, mort_pw)

si = relog(imm_name, imm_pw)

# --- 1: a stage with no quest_def is invisible ---
out = cmd(si, f"set {mort_name} quest {quest_name} 1")
check("stage 1" in out, "immortal sets the mortal's quest stage")
out = cmd(sA, "quest")
check(quest_name not in out, "a stage with no description is invisible in the summary list")
out = cmd(sA, f"quest {quest_name}")
check("aren't currently on" in out, "a stage with no description reports as not-on-that-quest")

# --- 2: writing a description makes it visible in both views ---
out = cmd(si, f"questdef {quest_name} 1 Find the missing amulet and return it.")
check("description set" in out, "questdef confirms")
out = cmd(sA, "quest")
check(quest_name in out, "the quest now appears in the summary list")
out = cmd(sA, f"quest {quest_name}")
check("missing amulet" in out, "quest <name> shows the full stage description")

# --- 3: advancing to an undefined stage hides it again ---
cmd(si, f"set {mort_name} quest {quest_name} 2")
out = cmd(sA, "quest")
check(quest_name not in out, "advancing to a stage with no description hides the quest again")

# --- 4: stage 0 clears it entirely ---
cmd(si, f"questdef {quest_name} 2 Return the amulet to the shrine.")
out = cmd(sA, "quest")
check(quest_name in out, "the quest reappears once stage 2 also has a description")
cmd(si, f"set {mort_name} quest {quest_name} 0")
out = cmd(sA, "quest")
check(quest_name not in out, "stage 0 clears the quest entirely, regardless of its description")

sA.close()
si.close()
announce_done("smoke_test_quest")
print("=== ALL CHECKS PASSED ===")
