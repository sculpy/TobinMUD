#!/usr/bin/env python3
"""Smoke test for water, drowning, and flight (Sneezy → Tobin feature
audit, "Water, drowning, flight"). Checked Sneezy's own movement-
terrain-navigation doc first: the real system is AFF_SWIM (halves/
quarters water-sector cost) + a procCharDrowning scheduler dealing 1d10
every 3.6 real seconds to anyone underwater without AFF_WATERBREATH,
genuinely lethal via reconcileDamage() -- plus AFF_FLYING, which
quarters movement cost and bypasses the whole drowning chain. Ported
as: two new spells (`cast gills of flesh` -> AFFECT_WATERBREATH,
`cast levitate` -> AFFECT_FLYING, both already listed unimplemented in
skill.c before this), the SAME 1d10 roll on vitals_tick_run()'s own
slower ~60s cadence (vitals.c) instead of the original's 3.6s one, and
sector_move_cost()'s existing tier system quartered while flying
(cmd_move.c). Per AskUserQuestion, drowning is genuinely lethal (routed
through combat_drown_pc(), combat.c) -- unlike hunger/thirst/poison's
non-lethal floor-at-1 convention.

Proficiency for a spell a character has never cast starts at a 1% floor
(skill.c's SKILL_PROFICIENCY_FLOOR) and climbs slowly with a 30s
cooldown between gain-checks -- far too slow to reliably self-cast in
an automated test. Mortal test characters below have their
`player_skill` row seeded directly to 100% before their first cast,
same "seed the DB state directly instead of grinding it live"
precedent smoke_test_vitals.py's set_hp()/set_level() already use.

Covers:
  1. A mortal with no protection, submerged in a TEMPERATE UNDERWATER
     sector, genuinely drowns (dies) within a bounded number of forced
     vitals ticks -- ejected to the account menu, corpse left behind.
  2. `cast gills of flesh` grants AFFECT_WATERBREATH (shown in
     `affects`), and a mortal carrying it survives the same forced
     ticks underwater without drowning.
  3. `cast levitate` grants AFFECT_FLYING (shown in `affects`), and
     quarters the vit cost of moving into the underwater room.

    python3 tests/smoke_test_water_drowning_flight.py [host] [port]
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


announce("smoke_test_water_drowning_flight")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_LAND = 980000 + (int(time.time()) % 15000)        # PLAINS (sector 17, cost 1)
ROOM_WATER = ROOM_LAND + 1                              # TEMPERATE UNDERWATER (sector 27, cost 4)
COMPONENT_A = ROOM_LAND + 2
COMPONENT_B = ROOM_LAND + 3
COMPONENT_C = ROOM_LAND + 4
MOVE_COST = (1 + 4 + 1) // 2  # 3, average-of-two-sectors rule


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


def live_vit(sock):
    out = cmd(sock, "score", timeout=0.3)
    m = re.search(r"Vitality:\s*(\d+)/(\d+)", out)
    return int(m.group(1))


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def seed_proficiency(name, skill_name, pct):
    """Bypasses the 1%-floor/30s-cooldown learn-by-doing grind (skill.c)
    -- see this file's own header comment for why."""
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def make_char(name, pw, class_num="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_num); recv_all(s)  # class: mage
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


# ROOM_LAND (PLAINS) <-> ROOM_WATER (TEMPERATE UNDERWATER)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_LAND},0,0,0,'Water Sandbox Land','A flat plain.\\n',NULL,1,17,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_WATER},0,0,0,'Water Sandbox Underwater','Murky green water presses in on all sides.\\n',NULL,1,27,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) "
    f"VALUES ({ROOM_LAND},0,'','',0,0,0,0,0,{ROOM_WATER});")  # 0 = north
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) "
    f"VALUES ({ROOM_WATER},2,'','',0,0,0,0,0,{ROOM_LAND});")  # 2 = south

imm_name, imm_pw = f"Wdfimm{_suffix}", "wdfimmpw1234"
si = make_char(imm_name, imm_pw)
# `quit!` FIRST, before any SQL customization -- a raw close() leaves the
# character "linked" in server memory (the next login resumes that live
# session instead of reloading fresh from the DB, silently ignoring any
# SQL made in the meantime, same lesson smoke_test_vitality_terrain.py
# hit); `quit!` itself SAVES the live state, so it must run BEFORE the
# SQL edits below, never after, or it would just overwrite them right
# back.
cmd(si, "quit!"); si.close()
set_level(imm_name, 51)
si = relog(imm_name, imm_pw)

# --- 1: unprotected drowning is genuinely lethal ---
mortA_name, mortA_pw = f"Wdfa{_suffix}", "wdfapw12345"
sA = make_char(mortA_name, mortA_pw)
cmd(sA, "quit!"); sA.close()
set_level(mortA_name, 1)
set_hp(mortA_name, 5, 100)  # low but nonzero -- confirms real drowning damage, not an instakill
sql(f"UPDATE player SET load_room={ROOM_WATER} WHERE name='{mortA_name}';")
sA = relog(mortA_name, mortA_pw)
check("Water Sandbox Underwater" in cmd(sA, "look"), "the mortal starts out genuinely submerged")

drowned = False
for _ in range(10):
    cmd(si, "aitick 1")  # aitick's own reply goes to the immortal, not the mortal
    out = cmd(sA, "", timeout=0.3)  # drain the mortal's own socket for the tick's messages
    if "DROWNED" in out:
        drowned = True
        break
check(drowned, "an unprotected mortal genuinely drowns within a bounded number of forced ticks")

out = cmd(sA, "")
check("Connect Player" in out, "the drowned socket landed back at the account menu, not mid-game")
check("TEMPERATE UNDERWATER" in cmd(si, f"goto {ROOM_WATER}"),
      "regression: the room itself is unaffected by the drowning")
out = cmd(si, "look")
check("corpse" in out.lower(), "a corpse was left behind where the mortal drowned")
si_room_out = out
sA.close()

# --- 2: `cast gills of flesh` grants water-breathing, which prevents it ---
# Affects are in-memory only, per session (not persisted -- see
# player_repo.c/affect.c) -- a quit!/relog after casting would wipe it
# right back off. Move into ROOM_WATER by real `north` movement instead
# of relogging, so the just-granted affect survives the trip. A fresh
# level-9 character's starting vit (well over 50) comfortably covers
# the single move's cost without needing any vit setup at all.
mortB_name, mortB_pw = f"Wdfb{_suffix}", "wdfbpw12345"
sB = make_char(mortB_name, mortB_pw)
cmd(sB, "quit!"); sB.close()
set_level(mortB_name, 9)
seed_proficiency(mortB_name, "gills of flesh", 100)
sql(f"UPDATE player SET load_room={ROOM_LAND} WHERE name='{mortB_name}';")
sB = relog(mortB_name, mortB_pw)

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT_A},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")
cmd(si, f"goto {ROOM_LAND}")
cmd(si, f"load obj {COMPONENT_A}")
cmd(sB, "get pouch")

out = cmd(sB, "cast gills of flesh")
check("gills split open" in out, "casting gills of flesh grants water breathing")
check("Water Breathing" in cmd(sB, "affects"), "affects shows Water Breathing active")

out = cmd(sB, "north")
check("Water Sandbox Underwater" in out, "the mortal walks into the underwater room under their own power")
check("Water Breathing" in cmd(sB, "affects"), "water breathing survived the walk (only quit!/relog would wipe it)")

for _ in range(5):
    cmd(si, "aitick 1")
    cmd(sB, "", timeout=0.3)
out = cmd(sB, "score")
m = re.search(r"HP:\s*(\d+)/(\d+)", out)
check(int(m.group(1)) == int(m.group(2)), "a mortal with water breathing takes no drowning damage underwater")
cmd(sB, "quit!"); sB.close()

# --- 3: `cast levitate` grants flight, which quarters the movement cost ---
mortC_name, mortC_pw = f"Wdfc{_suffix}", "wdfcpw12345"
sC = make_char(mortC_name, mortC_pw)
cmd(sC, "quit!"); sC.close()
set_level(mortC_name, 11)
seed_proficiency(mortC_name, "levitate", 100)
sql(f"UPDATE player SET load_room={ROOM_LAND} WHERE name='{mortC_name}';")
sC = relog(mortC_name, mortC_pw)

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT_B},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")
cmd(si, f"goto {ROOM_LAND}")
cmd(si, f"load obj {COMPONENT_B}")
cmd(sC, "get pouch")

out = cmd(sC, "cast levitate")
check("rise gently" in out, "casting levitate grants flight")
check("Flying" in cmd(sC, "affects"), "affects shows Flying active")

# A fresh level-11 character's starting vit is well over MOVE_COST, so
# no vit setup is needed before the single move below (and a mid-session
# SQL vit change wouldn't take effect anyway -- see live_vit()'s own
# doc comment, same lesson smoke_test_vitality_terrain.py already hit).
vit_before = live_vit(sC)
cmd(sC, "north", timeout=0.3)
flying_cost = vit_before - live_vit(sC)
check(flying_cost == (MOVE_COST + 3) // 4,
      f"flight quarters the movement cost (charged {flying_cost}, expected {(MOVE_COST + 3) // 4})")
cmd(sC, "quit!"); sC.close()

si.close()
announce_done("smoke_test_water_drowning_flight")
print("=== ALL CHECKS PASSED ===")
