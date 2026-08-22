#!/usr/bin/env python3
"""Smoke test for `mapexport`/`maprecalc` (TODO.md's mapping-support
item, world_map_repo.c, cmd_mapexport.c, cmd_maprecalc.c):
  1. Gate: `mapexport` is Administrator (59+) -- a level-58 immortal gets
     Command not found. `maprecalc` is Implementor (60+) -- a level-59
     immortal gets Command not found.
  2. `mapexport [filename]` dumps the WHOLE `room`/`roomexit` DB tables
     (not just the in-memory/visited cache) to map_exports/<filename> in
     the client's own map.dat format -- a SQL-bootstrapped sandbox room
     and its exits show up in the file with the right vnum/name/exits.
  3. `mapexport` rejects a filename containing a path separator or
     starting with a dot (no path traversal).
  4. `maprecalc` derives x/y/z for every room from the roomexit graph
     and persists them -- two sandbox rooms connected north/south end up
     exactly 1 apart on y (and equal on x/z), matching the fixed
     direction-delta convention (room.c's DIR_NAMES order).

All setup happens in SQL-bootstrapped sandbox rooms at high vnums
(900000+); the seeded world is never touched. mapexport/maprecalc
themselves necessarily touch the WHOLE room table (that's the feature),
but only WRITE to room.x/y/z (maprecalc) or read-only (mapexport) --
neither can corrupt existing room name/exit data.

    python3 tests/smoke_test_mapexport.py [host] [port]
"""
import os
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_mapexport", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 80000)
ROOM_A, ROOM_B = BASE, BASE + 1


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


name = f"Maptest{_suffix}"
pw = "maptestpw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human
send_line(s, "1"); recv_all(s)  # territory: urban
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)
send_line(s, "done"); recv_all(s)  # alignment: neutral

# --- 1: the gate, both commands, both boundaries ---
set_level(name, 58)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

check("Command not found" in cmd(s, "mapexport"), "a level-58 immortal can't mapexport (gate is 59)")

set_level(name, 59)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

check("Command not found" in cmd(s, "maprecalc"), "a level-59 immortal can't maprecalc (gate is 60)")

# --- bootstrap: two sandbox rooms, A's north exit -> B ---
for vnum, desc in ((ROOM_A, f"Map Sandbox A {_suffix}"), (ROOM_B, f"Map Sandbox B {_suffix}")):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{desc}','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM_A},0,'','',0,0,0,0,0,{ROOM_B});")  # north (dir 0), plain, open
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM_B},2,'','',0,0,0,0,0,{ROOM_A});")  # south (dir 2) back

# --- 3: filename safety, before promoting further (58 is already
#         below mapexport's own gate -- promote just enough) ---
set_level(name, 59)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

check("path separator" in cmd(s, "mapexport ../evil").lower(), "mapexport rejects a path-traversal filename (../)")
check("path separator" in cmd(s, f"mapexport dir/evil").lower(), "mapexport rejects a filename with a path separator")
check("start with a dot" in cmd(s, "mapexport .evil").lower(), "mapexport rejects a filename starting with a dot")

# --- 2: mapexport dumps the whole world, sandbox room included ---
export_name = f"smoketest_{_suffix}.dat"
out = cmd(s, f"mapexport {export_name}", timeout=8.0)  # ~19k rooms -- a real, synchronous, single-threaded DB dump, not instant
check("Exported" in out and "rooms to" in out, "mapexport confirms and reports a room count")

remote_path = os.path.expanduser(f"~/TobinMUD/c_port/map_exports/{export_name}")
with open(remote_path, encoding="utf-8") as f:
    lines = {ln.split("\t", 1)[0]: ln.rstrip("\n") for ln in f if ln.strip()}

check(str(ROOM_A) in lines, "the sandbox room A appears in the exported file")
line_a = lines[str(ROOM_A)]
parts = line_a.split("\t")
# VNUM<TAB>NAME<TAB>e0,...,e9<TAB>X,Y,Z (x/y/z added for the real-GDI map
# view, TODO.md) -- maprecalc hasn't run yet at this point in the test
# (that's part 4, below), so the coords field is just "0,0,0" here; this
# only checks the format itself, not the values.
check(len(parts) == 4 and parts[1] == f"Map Sandbox A {_suffix}",
      "room A's exported line carries its real name")
exits_a = parts[2].split(",")
check(len(exits_a) == 10 and exits_a[0] == str(ROOM_B),
      "room A's exported exits carry the north (index 0) exit to room B")
coords_a = parts[3].split(",")
check(len(coords_a) == 3 and all(c.lstrip("-").isdigit() for c in coords_a),
      "room A's exported line carries a well-formed X,Y,Z field")
os.remove(remote_path)

# --- 4: maprecalc derives coordinates from the exit graph ---
set_level(name, 60)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

out = cmd(s, "maprecalc", timeout=10.0)  # ~19k rooms, one transaction of ~19k UPDATEs -- genuinely slower than mapexport's read-only pass
check("Recalculated coordinates for" in out and "connected component" in out,
      "maprecalc confirms and reports rooms/components")

xa, ya, za = query(f"SELECT x FROM room WHERE vnum={ROOM_A};"), \
             query(f"SELECT y FROM room WHERE vnum={ROOM_A};"), \
             query(f"SELECT z FROM room WHERE vnum={ROOM_A};")
xb, yb, zb = query(f"SELECT x FROM room WHERE vnum={ROOM_B};"), \
             query(f"SELECT y FROM room WHERE vnum={ROOM_B};"), \
             query(f"SELECT z FROM room WHERE vnum={ROOM_B};")
check(xa == xb and za == zb, "room A and room B (a north/south pair) share x and z after recalc")
check(int(yb) - int(ya) == 1, "room B is exactly 1 north (y+1) of room A after recalc")

# --- 5: a mapexport taken after maprecalc carries the real coordinates ---
export_name2 = f"smoketest2_{_suffix}.dat"
out = cmd(s, f"mapexport {export_name2}", timeout=8.0)
check("Exported" in out and "rooms to" in out, "second mapexport confirms and reports a room count")
remote_path2 = os.path.expanduser(f"~/TobinMUD/c_port/map_exports/{export_name2}")
with open(remote_path2, encoding="utf-8") as f:
    lines2 = {ln.split("\t", 1)[0]: ln.rstrip("\n") for ln in f if ln.strip()}
parts_a2 = lines2[str(ROOM_A)].split("	")
coords_a2 = [int(v) for v in parts_a2[3].split(",")]
check(coords_a2[0] == int(xa) and coords_a2[1] == int(ya) and coords_a2[2] == int(za),
      "a post-maprecalc mapexport carries room A's real recalculated x/y/z")
os.remove(remote_path2)

s.close()
announce_done("smoke_test_mapexport", host, port)
print("=== ALL CHECKS PASSED ===")
