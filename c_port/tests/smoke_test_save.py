#!/usr/bin/env python3
"""Smoke test for `player_save()` / the `save` command / quit-auto-save
(user 2026-07-07: "add a save command to manually save your character";
user 2026-07-12: "the game should automatically save a char upon death
or quit"). Covers:

  1. `save` reports "Saved." and persists in-memory-only state (mid-fight
     HP loss, which is normally NOT written to the DB until defeat) to
     `player_progress.hp` immediately.
  2. Quitting (`quit!`) WITHOUT an explicit `save` first still persists
     that same in-memory-only HP loss -- the auto-save wired into
     `descriptor_leave_to_menu()`.

Mid-fight HP loss is used as the test signal because it's the one state
change in the whole game that is deliberately NOT saved at the moment it
happens (see TODO.md's "Mid-fight persistence" gap) -- exactly what would
have been silently lost before this feature.

    python3 tests/smoke_test_save.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_save", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000000) % 90000)


def query(stmt):
    out = subprocess.run(["mariadb", "tobin", "-N", "-e", stmt],
                          check=True, capture_output=True, text=True)
    return out.stdout.strip()


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def db_hp(name):
    return int(query(f"SELECT hp FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "1"); recv_all(sock)  # class: mage (irrelevant -- HP is overridden via SQL)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def parse_hp(out):
    m = re.search(r"HP:\s+(\d+) \((\d+) Max", out)
    return (int(m.group(1)), int(m.group(2))) if m else (None, None)


imm_name = f"Savetestimm{_suffix}"
imm_pw = "savetestimmpw123"
mort_name = f"Savetestmor{_suffix}"
mort_pw = "savetestmorpw123"

s_imm = socket.create_connection((host, port), timeout=5)
make_char(s_imm, imm_name, imm_pw)
set_level(imm_name, 51)
s_imm.close()
s_imm = login(imm_name, imm_pw)

s_mort = socket.create_connection((host, port), timeout=5)
make_char(s_mort, mort_name, mort_pw)
sql(f"UPDATE player_progress SET hp=100000, max_hp=100000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{mort_name}');")
s_mort.close()
s_mort = login(mort_name, mort_pw)

full_hp = 100000
check(db_hp(mort_name) == full_hp, "the mortal's full HP is what's on record before any damage")

# Get the immortal into the same room as the mortal.
out = cmd(s_mort, "score")
hp_before, _ = parse_hp(out)
check(hp_before == full_hp, "the mortal's live HP starts at full")

# Find the mortal's current room via a `where`-style lookup isn't available;
# use `goto` on the immortal by reading the mortal's room off `look` is not
# reliable either, so route the immortal to the mortal directly by vnum:
# the freshly-created mortal lands at the default mortal load room (100).
cmd(s_imm, "goto 100")

# --- 1: mid-fight damage is NOT saved until an explicit `save` ---
cmd(s_imm, f"hit {mort_name}")
hp_now = hp_before
for _ in range(15):
    time.sleep(0.6)
    out = cmd(s_mort, "score", timeout=0.5)
    h, _ = parse_hp(out)
    if h is not None:
        hp_now = h
    if hp_now < hp_before:
        break

check(hp_now < hp_before, "the mortal actually took mid-fight damage in memory")
check(db_hp(mort_name) == full_hp, "mid-fight damage is NOT yet persisted to the DB")

out = cmd(s_mort, "save")
check("Saved." in out, "the save command reports success")
# Combat is pulse-driven and keeps running in the background, so another
# round can land between capturing hp_now and this save -- allow the
# saved value to be lower than hp_now (never higher, never back to
# full), rather than requiring exact equality.
saved_hp = db_hp(mort_name)
check(saved_hp <= hp_now and saved_hp < full_hp, "save persists the current in-memory HP to the DB")

# --- 2: quitting without an explicit save still auto-saves ---
cmd(s_imm, f"hit {mort_name}")
hp_before2 = hp_now
for _ in range(15):
    time.sleep(0.6)
    out = cmd(s_mort, "score", timeout=0.5)
    h, _ = parse_hp(out)
    if h is not None:
        hp_now = h
    if hp_now < hp_before2:
        break

check(hp_now < hp_before2, "the mortal took further mid-fight damage in memory")
cmd(s_mort, "quit!")
# Same pulse-driven-combat tolerance as the save check above.
quit_saved_hp = db_hp(mort_name)
check(quit_saved_hp <= hp_now and quit_saved_hp < full_hp,
      "quit! auto-saves the current in-memory HP without an explicit save")

s_imm.close()
s_mort.close()
announce_done("smoke_test_save", host, port)
print("=== ALL CHECKS PASSED ===")
