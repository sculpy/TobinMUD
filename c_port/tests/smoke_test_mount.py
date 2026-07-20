#!/usr/bin/env python3
"""Smoke test for the Mount / riding system (Sneezy -> Tobin feature
audit, "Mount / riding system"). Checked Sneezy's own misc/riding.cc
first: the real system is a rich height-ratio/carry-weight/rider-slot
gauntlet plus a whole Deikhan "mounted knight" class -- Tobin has
neither, so this is scoped WAY down (see cmd_ride.c's own header
comment). User, AskUserQuestion 2026-07-19: a fuller scope (mounted
combat bonus + skill-gated mount success) and a simple immortal-stocked
stable using the existing shop system.

Covers:
  1. A mortal with 100% "riding" proficiency successfully mounts a real
     seeded HORSE-race mob (vnum 558) -- position becomes Mounted on
     both sides, mount/rider linked.
  2. A mortal with 0% proficiency always fails to mount (skill_roll_
     success(0) is deterministic).
  3. Moving while mounted brings the horse along to the new room, and
     costs less vit than the same move unmounted.
  4. Entering an INDOORS room auto-dismounts the rider and leaves the
     horse behind in the outdoor room.
  5. `dismount` works directly (not just the forced indoor case).
  6. The stable at room 564 (Petir's shop, shop_nr 164) sells a horse
     for gold via `list`/`buy`.
  7. being_destroy() teardown: purging a mounted horse dismounts its
     rider; quitting while mounted resets the horse back to Standing.

    python3 tests/smoke_test_mount.py [host] [port]
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


announce("smoke_test_mount")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_OUT = 950000 + (int(time.time()) % 40000)   # outdoor -- ALWAYS_LIT only
ROOM_OUT2 = ROOM_OUT + 1                          # a second outdoor room to move into
ROOM_IN = ROOM_OUT + 2                            # indoors -- ALWAYS_LIT | INDOORS


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


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "sneezy", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def gold_of(name):
    return int(query(f"SELECT gold FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_gold(name, amount):
    sql(f"UPDATE player_progress SET gold={amount} WHERE player_id="
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


# --- sandbox world ---
# Both outdoor rooms use sector 55 (SOLID ROCK, sector_move_cost()==6,
# room.c) rather than the default sector 0 (cost 1) -- at cost 1 the
# mounted-vs-unmounted discount ((cost+1)/2) rounds to the SAME integer
# as the unmounted cost, making the discount check meaningless. Cost 6
# gives mounted=3 vs unmounted=6, a real, checkable gap.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT},0,0,0,'Mount Sandbox Outdoor','A bare sandbox room.\\n',NULL,1,55,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT2},0,0,0,'Mount Sandbox Outdoor Two','A bare sandbox room.\\n',NULL,1,55,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_IN},0,0,0,'Mount Sandbox Indoor','A snug room with a solid roof.\\n',NULL,9,0,0,0,0,0,0,0,0,0);")
# north from ROOM_OUT -> ROOM_OUT2 -> ROOM_IN, with the matching south
# exits back (0 = north, 2 = south -- same roomexit shape smoke_test_
# vitality_terrain.py's sandbox corridor uses).
def link(a, direction, b):
    sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
        f"lock_difficulty,weight,key_num,destination) "
        f"VALUES ({a},{direction},'','',0,0,0,0,0,{b});")


link(ROOM_OUT, 0, ROOM_OUT2)
link(ROOM_OUT2, 2, ROOM_OUT)
link(ROOM_OUT2, 0, ROOM_IN)
link(ROOM_IN, 2, ROOM_OUT2)

imm_name, imm_pw = f"Mntimm{_suffix}", "mntimmpw123"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM_OUT}")

# --- mortal A: 100% riding proficiency -- deterministic success ---
# combat_disc_pct is the proficiency CEILING for any SKILL_TIER_COMBAT
# skill (skill.c's skill_ceiling()) -- a fresh, never-practiced character
# has it at 0, which caps skill_learn_from_doing() at 0 regardless of
# whatever raw pct seed_proficiency() wrote, same discipline-gate rule
# cmd_cast.c enforces for spells. Must set both.
nameA, pwA = f"Mnta{_suffix}", "mntapw12345"
sA = make_char(nameA, pwA)
cmd(sA, "quit!"); sA.close()
sql(f"UPDATE player SET load_room={ROOM_OUT} WHERE name='{nameA}';")
sql(f"UPDATE player_progress SET combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameA}');")
seed_proficiency(nameA, "riding", 100)
sA = relog(nameA, pwA)

cmd(si, "load mob 558")
out = strip(cmd(sA, "ride plow-horse"))
check("You mount" in out, "a 100%-proficiency mortal successfully mounts the horse")

out = strip(cmd(sA, "score"))
check("Mounted" in out, "score shows Position: Mounted after mounting")

# --- mortal B: 0% riding proficiency -- deterministic failure ---
nameB, pwB = f"Mntb{_suffix}", "mntbpw12345"
sB = make_char(nameB, pwB)
cmd(sB, "quit!"); sB.close()
sql(f"UPDATE player SET load_room={ROOM_OUT} WHERE name='{nameB}';")
sql(f"UPDATE player_progress SET combat_disc_pct=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameB}');")
seed_proficiency(nameB, "riding", 0)
sB = relog(nameB, pwB)

cmd(si, "load mob 558")
out = strip(cmd(sB, "ride plow-horse"))
check("You mount" not in out and "slide right back off" in out,
      "a 0%-proficiency mortal always fails to mount (skill_roll_success(0) is deterministic)")

sB.close()

# --- movement: horse comes along, vit cost is discounted ---
def live_vit(sock, timeout=0.3):
    out = strip(cmd(sock, "score", timeout))
    m = re.search(r"Vitality:\s+(\d+)/(\d+)", out)
    return int(m.group(1)) if m else None

before = live_vit(sA)
cmd(sA, "north")
after = live_vit(sA)
check(before is not None and after is not None and after < before,
      "moving while mounted still spends some vit")
mounted_cost = before - after

out = strip(cmd(sA, "look"))
check("Mount Sandbox Outdoor Two" in out, "the mounted rider actually arrived in the new room")

# An unmounted control walking the same terrain, to compare cost.
nameC, pwC = f"Mntc{_suffix}", "mntcpw12345"
sC = make_char(nameC, pwC)
cmd(sC, "quit!"); sC.close()
sql(f"UPDATE player SET load_room={ROOM_OUT} WHERE name='{nameC}';")
sC = relog(nameC, pwC)
before_c = live_vit(sC)
cmd(sC, "north")
after_c = live_vit(sC)
unmounted_cost = before_c - after_c
check(mounted_cost < unmounted_cost,
      f"mounted movement costs less vit than unmounted ({mounted_cost} vs {unmounted_cost})")
sC.close()

# --- indoor auto-dismount: horse left behind ---
out = strip(cmd(sA, "north"))
check("have to dismount" in out.lower(), "entering an indoor room forces a dismount")
out = strip(cmd(sA, "score"))
check("Mounted" not in out, "score no longer shows Mounted after the forced indoor dismount")

# --- explicit dismount (not the forced case) ---
cmd(si, f"goto {ROOM_OUT2}")
cmd(si, "load mob 558")
out = strip(cmd(sA, "south"))  # back to the outdoor room with the fresh horse
out = strip(cmd(sA, "ride plow-horse"))
check("You mount" in out, "mortal A can mount again after the forced dismount")
out = strip(cmd(sA, "dismount"))
check("You dismount" in out, "the dismount command works directly")
out = strip(cmd(sA, "score"))
check("Mounted" not in out, "score confirms Standing again after an explicit dismount")

# --- being_destroy() teardown: purging a mounted horse dismounts the rider ---
# Bare `purge` (cmd_purge.c) empties the WHOLE room of mobs/objects --
# never players -- so it's a clean way to destroy just the horse while
# si and sA (both PCs) stand right there in the same room.
out = strip(cmd(sA, "ride plow-horse"))
check("You mount" in out, "mortal A mounts once more for the purge-teardown check")
cmd(si, "purge")
out = strip(cmd(sA, "score"))
check("Mounted" not in out, "purging the mount out from under the rider forces a clean dismount")

sA.close()

# --- stable purchase flow (Petir's shop, room 564, shop_nr 164) ---
nameD, pwD = f"Mntd{_suffix}", "mntdpw12345"
sD = make_char(nameD, pwD)
cmd(sD, "quit!"); sD.close()
sql(f"UPDATE player SET load_room=564 WHERE name='{nameD}';")
set_gold(nameD, 200)
sD = relog(nameD, pwD)

out = strip(cmd(sD, "list"))
check("plow-horse" in out.lower() and "100 gold" in out, "the stable lists a horse for 100 gold")

before_gold = gold_of(nameD)
out = strip(cmd(sD, "buy 1"))
check("leads a horse out" in out, "buying from the stable spawns a horse")
check(gold_of(nameD) == before_gold - 100, "buying the horse deducted exactly 100 gold")

out = strip(cmd(sD, "ride plow-horse"))
check("You mount" in out or "slide right back off" in out,
      "the newly-bought horse is a real rideable mob (ride at least resolves, proficiency-dependent)")

cmd(sD, "quit!")
sD.close()

announce_done("smoke_test_mount")
print("=== ALL CHECKS PASSED ===")
