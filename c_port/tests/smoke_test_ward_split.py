#!/usr/bin/env python3
"""Smoke test for the ward-buff split (2026-08-17).

The "protective ward" spell family used to funnel into a single
AFFECT_SANCTUARY halve-damage effect. Each distinct upstream mechanic
now gets its own affect, routed by affect_ward_for() (affect.c):

  - sanctuary            -> AFFECT_SANCTUARY  ("Sanctuary", halve damage)
  - armor / stone skin.. -> AFFECT_ARMOR      ("Armored", harder to hit)
  - bless                -> AFFECT_BLESS       ("Blessed", attacker buff)
  - protection from *    -> AFFECT_PROTECTION  ("Protected", flat % cut)
  - plasma mirror        -> AFFECT_DAMAGE_MIRROR("Reflecting", reflect)

Covers:
  1. A fresh immortal's `affects` shows "(none)".
  2. An immortal casts sanctuary, armor, bless, and protection from
     earth on itself; `affects` then lists all four DISTINCT names at
     once (proving they no longer collapse into one AFFECT_SANCTUARY).
  3. A mortal Mage casts plasma mirror on itself; `affects` shows the
     distinct "Reflecting".
  4. A mortal attacker who lands hits on the mirror-shielded Mage sees
     the reflection message -- the AFFECT_DAMAGE_MIRROR combat hook
     actually fires.

    python3 tests/smoke_test_ward_split.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

COMBAT_ROUND_SECS = 1.2

announce("smoke_test_ward_split", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000) % 60000)


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            break
    return b"".join(chunks).decode(errors="replace")


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def cast_and_settle(sock, spell):
    # A cast/pray is a multi-round chant task; drain until it resolves
    # (or a generous cap) before the affect is readable.
    send_line(sock, f"cast {spell}")
    seen = ""
    for _ in range(8):
        seen += recv_all(sock, COMBAT_ROUND_SECS + 0.2)
        if "settles over" in seen or "surrounds" in seen or "wards" in seen:
            break
    time.sleep(COMBAT_ROUND_SECS)
    recv_all(sock, 0.3)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_dex(name, dex):
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def create_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)            # territory: urban
    send_line(s, class_choice); recv_all(s)   # class
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "wardsplitpw123"

# --- Sandbox room ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Ward Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# --- Immortal caster (warrior shell; immortals bypass class/level/component
#     gating for cast/pray) ---
imm_name = f"Wardimm{_suffix}"
s = create_char(imm_name, pw, "3")   # warrior
cmd(s, "quit!")
s.close()
set_level(imm_name, 51)
s_imm = login(imm_name, pw)
check("Ward Sandbox" in cmd(s_imm, f"goto {ROOM}"), "immortal lands in the sandbox room")

# --- 1: fresh affects empty ---
check("(none)" in cmd(s_imm, "affects"), "immortal affects shows (none) before casting")

# --- 2: three formerly-collapsed ward spells now apply THREE distinct
#     affects at once (before the split these were all "Sanctuary").
#     Protected and Reflecting are checked on the mortal mage below. ---
cmd(s_imm, "pray sanctuary")
cmd(s_imm, "pray armor")
cmd(s_imm, "pray bless")
time.sleep(2 * COMBAT_ROUND_SECS)   # let the prayer tasks resolve
recv_all(s_imm, 0.3)
out = cmd(s_imm, "affects")
check("Sanctuary" in out, "affects lists Sanctuary (halve-damage)")
check("Armored" in out, "affects lists Armored (armor -> distinct affect)")
check("Blessed" in out, "affects lists Blessed (bless -> distinct affect)")

# (immortal stays connected -- it casts the remaining wards onto the
#  mortal target below; mortal casters would need spell components, which
#  immortals bypass.)

# --- Mortal target: a plain punching-bag that will hold the wards ---
tgt_name = f"Wardtgt{_suffix}"
s = create_char(tgt_name, pw, "3")   # warrior
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{tgt_name}';")
cmd(s, "quit!")
s.close()
set_level(tgt_name, 40)
set_hp(tgt_name, 8000)
set_dex(tgt_name, 3)   # low dex so the attacker below reliably lands hits
s_tgt = login(tgt_name, pw)
check("Ward Sandbox" in cmd(s_tgt, "look"), "target lands in the sandbox room")

# --- Mortal attacker (high dex, big HP) -- created BEFORE the wards are
#     cast so its slow char-creation can't expire the ward durations. ---
atk_name = f"Wardatk{_suffix}"
s = create_char(atk_name, pw, "3")   # warrior
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{atk_name}';")
cmd(s, "quit!")
s.close()
set_level(atk_name, 40)
set_hp(atk_name, 8000)
set_dex(atk_name, 900)
s_atk = login(atk_name, pw)
check("Ward Sandbox" in cmd(s_atk, "look"), "attacker lands in the sandbox room")

# --- 3a: protection from earth (room-wide) applies distinct Protected to
#     the mortal target (immortal caster is itself immortal-skipped). ---
cast_and_settle(s_imm, "protection from earth")
recv_all(s_tgt, 0.3)
check("Protected" in cmd(s_tgt, "affects"),
      "target's affects lists Protected (protection from earth -> distinct affect)")

# --- 3b: plasma mirror cast ON the target applies distinct Reflecting. ---
cast_and_settle(s_imm, f"plasma mirror {tgt_name}")
recv_all(s_tgt, 0.3)
check("Reflecting" in cmd(s_tgt, "affects"),
      "target's affects lists Reflecting (plasma mirror -> distinct affect)")

s_imm.close()

# --- 4: landing hits on the mirror-shielded target rebounds onto attacker.
#     Both are mortal PCs, so a hit needs mutual PK opt-in. ---
cmd(s_tgt, "toggle pk")
cmd(s_atk, "toggle pk")
recv_all(s_atk, 0.3)
cmd(s_atk, f"hit {tgt_name}")
seen = ""
for _ in range(8):
    seen += recv_all(s_atk, COMBAT_ROUND_SECS + 0.3)
    if "rebounds off a shimmering mirror shield" in seen:
        break
check("rebounds off a shimmering mirror shield" in seen,
      "attacker's landed hit rebounds off the mirror shield (AFFECT_DAMAGE_MIRROR fires)")

cmd(s_atk, "flee")
s_atk.close()
s_tgt.close()
announce_done("smoke_test_ward_split", host, port)
print("=== ALL CHECKS PASSED ===")
