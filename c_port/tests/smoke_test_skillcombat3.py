#!/usr/bin/env python3
"""Smoke test for the second batch of spell/skill functional-completeness
audit (2026-07-27) level-1 roster entries: sneak, grapple, berserk, rally
(all Warrior/Thief). See cmd_sneak.c/cmd_grapple.c/cmd_berserk.c/
cmd_rally.c's own header comments for scope-down rationale.

    python3 tests/smoke_test_skillcombat3.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_skillcombat3", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_WARRIOR = 2
CLASS_THIEF = 3


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_room(name, room):
    sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")


def set_combat_disc(name, pct):
    sql(f"UPDATE player_progress SET combat_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_basic_disc(name, pct):
    sql(f"UPDATE player_progress SET basic_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_single(prefix, cls, level=None, room=None):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    set_class(name, cls)
    if level is not None:
        sql(f"UPDATE player_progress SET level={level} WHERE player_id="
            f"(SELECT id FROM player WHERE name='{name}');")
    if room is not None:
        set_room(name, room)
    set_combat_disc(name, 100)
    set_basic_disc(name, 100)
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


def attack_and_settle(sock, target_name):
    cmd(sock, f"attack {target_name}")
    time.sleep(1.3)


ROOM_A = 972000 + (int(time.time()) % 10000)
ROOM_B = ROOM_A + 1
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Sneak Sandbox A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0),"
    f"({ROOM_B},1,0,0,'Sneak Sandbox B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
# direction 0 = north (DIR_NAMES order) -- a real walkable exit so `north`
# actually goes through cmd_move.c's own arrival/departure echo, unlike
# `transfer` (a separate immortal-only teleport path with its own
# messaging that never touches cmd_move.c's sneak gate at all).
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM_A},0,'','',0,0,0,0,0,{ROOM_B});")

# =================== 1. sneak (Thief-only in this roster) ===================
nameA, sA = make_single("Snkw", CLASS_THIEF, level=5, room=ROOM_A)
nameB, sB = make_single("Snkwo", CLASS_WARRIOR, level=5, room=ROOM_B)

# Baseline: without sneaking, walking through the exit announces arrival.
recv_all(sB, 0.2)
send_line(sA, "north")
recv_all(sA, 0.4)
out_b_baseline = strip(recv_all(sB, 0.4))
check("has arrived" in out_b_baseline.lower(), "a normal (non-sneaking) arrival is visible to the room")

# Walk back, then toggle sneak on and repeat -- the arrival should be suppressed.
send_line(sA, "south")
recv_all(sA, 0.4)
recv_all(sB, 0.3)
out_toggle = strip(cmd(sA, "sneak"))
check("moving quietly" in out_toggle.lower(), "sneak toggles on")
recv_all(sB, 0.2)
send_line(sA, "north")
recv_all(sA, 0.4)
out_b_sneak = strip(recv_all(sB, 0.4))
check("has arrived" not in out_b_sneak.lower(),
      "a sneaking arrival is NOT announced to the room")

# Entering combat breaks sneak -- cmd_sneak.c refuses to toggle at all
# while fighting, so a cleared flag shows up as the ATTACK itself
# succeeding normally (no leftover suppression bug carrying into combat).
attack_and_settle(sB, nameA)
out_refuse = strip(cmd(sA, "sneak"))
check("not while you're fighting" in out_refuse.lower(),
      "sneak can't be re-toggled while fighting (sanity check on the guard)")
sA.close(); sB.close()

# =================== 2. grapple ===================
(nameC, sC) = make_single("Grpw", CLASS_WARRIOR, level=5)
(nameD, sD) = make_single("Grpwo", CLASS_WARRIOR, level=5)
seed_proficiency(nameC, "grapple", 100)
attack_and_settle(sC, nameD)
send_line(sC, f"grapple {nameD}")
out = strip(recv_all(sC, 0.4))
check("pinning them down" in out.lower(), "100%-proficiency grapple succeeds")
send_line(sC, "look")
send_line(sD, "look")
out_c = strip(recv_all(sC, 0.4))
out_d = strip(recv_all(sD, 0.4))
check("still recovering" in out_c.lower(), "grapple locks up the ATTACKER too")
check("still recovering" in out_d.lower(), "grapple locks up the defender")
sC.close(); sD.close()

# =================== 3. berserk (unparryable + blocks rescue) ===================
# Create every character BEFORE casting berserk -- character creation
# itself takes several real seconds of round-trips, and berserk's own
# duration is finite, so casting it last (right before the checks that
# need it still active) avoids a flaky race against its own expiry.
(nameE, sE) = make_single("Bszw", CLASS_WARRIOR, level=5)
(nameZ, sZ) = make_single("Bszwz", CLASS_WARRIOR, level=5)  # attacks the berserker, so it's actually "in danger" to rescue from
(nameF, sF) = make_single("Bszwr", CLASS_WARRIOR, level=5)  # the would-be rescuer
seed_proficiency(nameE, "berserk", 100)

out = strip(cmd(sE, "berserk"))
check("berserk rage" in out.lower(), "100%-proficiency berserk succeeds")

attack_and_settle(sZ, nameE)
out = strip(cmd(sF, f"rescue {nameE}"))
check("too much" in out.lower(),
      "a berserking character can't be rescued")

time.sleep(2.5)  # berserk's own being_set_wait() blocks ALL commands, including `affects`, until it expires
out_affects = strip(cmd(sE, "affects"))
check("berserk" in out_affects.lower(), "the berserk affect shows in `affects`")
sE.close(); sF.close(); sZ.close()

# =================== 4. rally ===================
(nameG, sG) = make_single("Ralw", CLASS_WARRIOR, level=20)
(nameH, sH) = make_single("Ralwo", CLASS_WARRIOR, level=20, room=None)
sql(f"UPDATE player SET load_room=(SELECT load_room FROM player WHERE name='{nameG}') WHERE name='{nameH}';")
sH.close()
sH = relog(nameH, f"Ralwopw12345")
seed_proficiency(nameG, "rally", 100)
out = strip(cmd(sG, "rally"))
check("rousing battlecry" in out.lower(), "100%-proficiency rally succeeds")
out_h_affects = strip(cmd(sH, "affects"))
check("rally" in out_h_affects.lower(), "an ally in the room picks up the rally buff")
sG.close(); sH.close()

announce_done("smoke_test_skillcombat3", host, port)
print("=== ALL CHECKS PASSED ===")
