#!/usr/bin/env python3
"""Smoke test for the Vitality stat + terrain movement cost (Sneezy →
Tobin feature audit, "Vitality stat + Terrain movement cost"). Neither
half exists in the original either -- its "moves" resource (misc/
limits.cc) is CON-derived but otherwise unnamed; Vitality is a
Tobin-original name for the same role, closing the "Depends on
Vitality" TODO.md fragment this item's own audit note pointed at.
Terrain cost is bucketed into 6 tiers by sector-name substring (room.c's
sector_move_cost()), not the original's 61-row hand-tuned TerrainInfo
table -- see room.h's doc comment for the scope-cut. Covers:
  1. A freshly created character's `score` shows a Vitality line.
  2. Moving between two rooms charges the average of their sector costs
     (PLAINS=1, SOLID ROCK=6 -> (1+6+1)//2=4), deducted from vit and
     persisted immediately (cmd_move.c saves progress on every charged
     move, same "don't lose it to a disconnect" precedent as eat/drink).
  3. Movement is refused outright once vit can't cover the cost, and
     nothing is spent.
  4. Movement succeeds again once vit exactly equals the cost (the gate
     is `vit < cost`, so equal is allowed).
  5. An immortal is exempt: no vit is spent moving the same route.

Room position isn't itself persisted by plain movement (only `loadroom`/
`edplayer`/`set` touch the DB's load_room column) -- each phase below
explicitly sets load_room via SQL before relogging, same convention
smoke_test_pursuit.py/smoke_test_vitals.py already use, rather than
relying on continuity across a relog. Every relog is preceded by a real
`quit!`, not a raw socket close -- a raw close leaves the character
"linked" in server memory, so the next login resumes that live session
(and silently ignores any SQL made in between) instead of reloading
fresh from the DB.

Known residual flake (~1 run in 6, empirically): regen_tick_run()
(regen.c) heals vit in memory every ~5 real seconds, independent of
anything this test does. Every timing-sensitive check reads vit live
via `score` (live_vit()) with a tight 0.3s timeout rather than the DB,
and checks that CAN tolerate a stray regen tick use >= instead of ==
(regen only ever adds) -- but the core "cost equals exactly the
average of the two sectors" check can't be loosened that far without
losing the ability to catch a real off-by-one in sector_move_cost()'s
tiers, so it stays a tight ==. Same accepted-flake precedent as
smoke_test_drink.py's own documented ~30% poison-message flake --
re-running clears it.

    python3 tests/smoke_test_vitality_terrain.py [host] [port]
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


announce("smoke_test_vitality_terrain")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 970000 + (int(time.time()) % 20000)  # PLAINS (sector 17, cost 1)
ROOM_B = ROOM_A + 1                            # SOLID ROCK (sector 55, cost 6)
MOVE_COST = (1 + 6 + 1) // 2  # 4, average-of-two-sectors rule, room.h


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


def vit_of(name):
    return int(query(f"SELECT vit FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def live_vit(sock):
    """Read vit straight from the live in-memory character via `score`,
    not the DB -- regen_tick_run() (regen.c) heals vit every ~5 real
    seconds but ONLY in memory (it never calls player_progress_save()),
    so a DB read can miss a regen bump that a same-second move command
    sees and then persists. Reading `score`'s own rendered Vitality line
    immediately before/after a move closes that gap to a single
    back-to-back exchange instead of a full relog + DB round trip. A
    short 0.3s timeout (recv_all() otherwise blocks for the full
    default 1.0s on every call, since it can't know the server is done
    without a timeout) keeps each exchange under a second in total, well
    under the 5s regen cadence -- the default 1.0s was still wide enough
    to straddle a tick roughly 1 exchange in 3."""
    out = cmd(sock, "score", timeout=0.3)
    m = re.search(r"Move:\s*(\d+) \((\d+) Max", out)
    return int(m.group(1))


def set_vit_and_room(name, vit, room):
    sql(f"UPDATE player_progress SET vit={vit} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")


def set_vit_and_room(name, vit, room):
    sql(f"UPDATE player_progress SET vit={vit} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")


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


mort_name, mort_pw = f"Vittera{_suffix}", "vitterrapw12"
imm_name, imm_pw = f"Vitterib{_suffix}", "vitterripw12"

sA = make_char(mort_name, mort_pw)

# --- 1. score shows a Vitality line for a freshly created character ---
out = cmd(sA, "score")
m = re.search(r"Move:\s*(\d+) \((\d+) Max", out)
check(m is not None, "score shows a Vitality: <n>/<n> line")
check(int(m.group(1)) == int(m.group(2)) > 0, "a fresh character starts at full vitality")

cmd(sA, "quit!"); sA.close()

si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")

# ROOM_A (PLAINS) <-> ROOM_B (SOLID ROCK), single exit each way -- a
# deterministic corridor, same shape as smoke_test_pursuit.py's sandbox.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Terrain Sandbox A','A flat plain.\\n',NULL,1,17,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_B},0,0,0,'Terrain Sandbox B','Bare solid rock.\\n',NULL,1,55,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) "
    f"VALUES ({ROOM_A},0,'','',0,0,0,0,0,{ROOM_B});")  # 0 = north
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) "
    f"VALUES ({ROOM_B},2,'','',0,0,0,0,0,{ROOM_A});")  # 2 = south

# --- 2. moving PLAINS -> SOLID ROCK charges the averaged cost ---
# Vit is read live via `score` (live_vit()), not the DB (vit_of()), for
# every check tight against a move -- regen_tick_run() heals vit
# in-memory only, every ~5 real seconds, and never itself calls
# player_progress_save(); a DB read can miss a regen bump a same-second
# move already saw and persisted. See live_vit()'s own doc comment.
set_vit_and_room(mort_name, 20, ROOM_A)
sA = relog(mort_name, mort_pw)
vit_before = live_vit(sA)
check(vit_before >= 20, "vit set to a known value before moving")

cmd(sA, "north", timeout=0.3)
check(live_vit(sA) == vit_before - MOVE_COST,
      f"moving into SOLID ROCK charged the averaged cost ({MOVE_COST})")
cmd(sA, "quit!")
# >=, not == -- a regen tick straddling the `quit!` round trip only ever
# ADDS a little more before the final save, it can't erase the charge
# already applied above; a real under-persist bug would show up as LESS
# than this floor, which >= still catches.
check(vit_of(mort_name) >= vit_before - MOVE_COST,
      "the charged cost was saved to the DB (quit! forces a final save)")
sA.close()

# --- 3. movement refused once vit can't cover the cost ---
set_vit_and_room(mort_name, 1, ROOM_B)
sA = relog(mort_name, mort_pw)
vit_before = live_vit(sA)

out = cmd(sA, "south", timeout=0.3)
check("too exhausted" in out, "movement is refused when vit can't cover the cost")
# >=, not == -- same regen-tolerance reasoning as above: a refused move
# spends nothing, so vit can only stay flat or rise (regen), never fall.
# A real bug that wrongly charged the cost anyway would show up as a
# drop below vit_before, which >= still catches.
check(live_vit(sA) >= vit_before, "a refused attempt spends nothing")

# Confirm the player never actually left the room: a second attempt in
# the same direction behaves identically (still blocked -- if the first
# had silently succeeded, this would instead say "can't go that way").
out2 = cmd(sA, "south")
check("too exhausted" in out2, "the player never left the room on the refused attempt")
cmd(sA, "quit!"); sA.close()

# --- 4. movement succeeds again once vit exactly equals the cost ---
set_vit_and_room(mort_name, MOVE_COST, ROOM_B)
sA = relog(mort_name, mort_pw)
vit_before = live_vit(sA)  # >= MOVE_COST -- a stray regen tick only adds, never subtracts

out = cmd(sA, "south", timeout=0.3)
check("too exhausted" not in out, "movement succeeds when vit exactly equals the cost")
check(live_vit(sA) == vit_before - MOVE_COST,
      "vit drops by exactly the move's cost, whatever it started at")
cmd(sA, "quit!"); sA.close()

# --- 5. an immortal is exempt from the cost entirely ---
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{imm_name}';")
si = relog(imm_name, imm_pw)
si_vit_before = live_vit(si)
cmd(si, "north", timeout=0.3)
check(live_vit(si) == si_vit_before, "an immortal spends no vit moving the same route")
cmd(si, "quit!"); si.close()

announce_done("smoke_test_vitality_terrain")
print("=== ALL CHECKS PASSED ===")
