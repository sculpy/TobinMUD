#!/usr/bin/env python3
"""Smoke test for hunger/thirst/age (Sneezy â†’ Tobin feature audit, "Vital
statistics"). Scoped down from the original per user AskUserQuestion
2026-07-19: age is track+display only, no stat-curve effects -- see
being.h's progress_t field comment. Covers:
  1. A freshly created character starts well fed/quenched, with a fresh
     Age line in `score`.
  2. `eat` restores hunger by the food object's own val[0] and consumes it.
  3. `drink` from a fountain fully refills thirst.
  4. Starvation (hunger AND thirst at 0) costs 1 HP per vitals tick
     (forced deterministically via `aitick`, see cmd_aitick.c).
  5. That HP loss is floored at 1 -- never lethal outside real combat,
     same precedent as cmd_sip.c's poison roll.
  6. An immortal is immune: `score` shows "Immune" regardless of level.

    python3 tests/smoke_test_vitals.py [host] [port]
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


announce("smoke_test_vitals")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 910000 + (int(time.time()) % 70000)
FOOD_VNUM = 405   # "steak beef marinated" -- real seeded FOOD, val0=24
FOUNTAIN_VNUM = 3  # "fountain water" -- real seeded DRINK, never runs dry


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


def query(stmt):
    return subprocess.run(["mariadb", "-N", "sneezy", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def hunger_of(name):
    return int(query(f"SELECT hunger FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def thirst_of(name):
    return int(query(f"SELECT thirst FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def hp_of(name):
    return int(query(f"SELECT hp FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_hunger_thirst(name, hunger, thirst):
    sql(f"UPDATE player_progress SET hunger={hunger}, thirst={thirst} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


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
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


mort_name, mort_pw = f"Vitmortb{_suffix}", "vitmortpw1234"
imm_name, imm_pw = f"Vitimmb{_suffix}", "vitimmpw1234"

sA = make_char(mort_name, mort_pw); sA.close()
si = make_char(imm_name, imm_pw); si.close()
set_level(imm_name, 51)

sA = relog(mort_name, mort_pw)
si = relog(imm_name, imm_pw)

# --- 1: a fresh character starts well fed/quenched, with an Age line ---
out = cmd(sA, "score")
check("Hunger: well fed" in out, "a fresh character starts well fed")
check("Thirst: quenched" in out, "a fresh character starts quenched")
check(re.search(r"Age:\s+\d+ years old", out) is not None,
      "score shows a fresh Age line (starts at 17, real-time converted via gametime's mud-year ratio)")

# --- bootstrap a sandbox room + real food/drink objects ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Vitals Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sA, "quit!")
sA.close()
sA = relog(mort_name, mort_pw)

cmd(si, f"goto {ROOM}")
cmd(si, f"load obj {FOOD_VNUM}")
cmd(si, "drop steak")
cmd(si, f"load obj {FOUNTAIN_VNUM}")
cmd(si, "drop fountain")
out = cmd(sA, "get steak")
check("You get" in out or "you get" in out.lower(), "the mortal picks up the loaded steak")

# --- 2: eat restores hunger by the food's own val[0] (24) and consumes it ---
# A raw close (NOT `quit!`) from here on -- `quit!` drops every carried item
# on the floor (its own documented behavior, cmd_quit.c), which would dump
# the steak we just picked up. A raw close leaves the character linkdead in
# the SAME room (no load_room change needed anymore at this point), which
# reconnecting resumes into -- inventory intact, exactly what these steps
# need to pick up each direct SQL edit.
set_hunger_thirst(mort_name, 40, 100)
sA.close()
sA = relog(mort_name, mort_pw)
out = cmd(sA, "eat steak")
check("You eat" in out, "eat confirms")
# A real ~60s background vitals tick could in principle land mid-test and
# shave a point off before this check runs (it did, once, for the drink
# check below) -- a tolerant range, not exact equality, is what's actually
# being tested here (the steak's own val0=24 restore), not test timing.
check(62 <= hunger_of(mort_name) <= 64, "hunger rose by ~the steak's own val0 (40 -> ~64)")
out = cmd(sA, "eat steak")
check("aren't carrying" in out, "the steak was fully consumed, not left as a leftover")

# --- 3: drink from a fountain fully refills thirst ---
set_hunger_thirst(mort_name, 100, 20)
sA.close()
sA = relog(mort_name, mort_pw)
out = cmd(sA, "drink fountain")
check("Refreshing" in out, "drink confirms")
check(98 <= thirst_of(mort_name) <= 100, "thirst fully (or near-fully) refilled from the fountain")

# --- 4/5: starvation costs 1 HP per tick, floored at 1 (never lethal) ---
set_hunger_thirst(mort_name, 0, 0)
set_hp(mort_name, 50, 50)
sA.close()
sA = relog(mort_name, mort_pw)
cmd(si, "aitick 1")
check(47 <= hp_of(mort_name) <= 49, "starving at hunger AND thirst 0 costs ~1 HP for one forced tick")

set_hp(mort_name, 1, 50)
sA.close()
sA = relog(mort_name, mort_pw)
cmd(si, "aitick 1")
check(hp_of(mort_name) == 1, "starvation damage is floored at 1 HP -- never lethal outside real combat")

# --- 6: an immortal is immune regardless of stored value ---
out = cmd(si, "score")
check("Hunger: immune" in out, "an immortal's score shows Hunger: immune")
check("Thirst: immune" in out, "an immortal's score shows Thirst: immune")

sA.close()
si.close()
announce_done("smoke_test_vitals")
print("=== ALL CHECKS PASSED ===")
