#!/usr/bin/env python3
"""Smoke test for weather + the darkness half of "light levels" (Sneezy →
Tobin feature audit, "Weather & light levels"). Checked Sneezy's own
Weather class first: a real barometric-pressure simulation, moon phases,
per-room wetness -- trimmed hard to a single world-wide sky state advanced
by a simple weighted transition table (see weather.h's own doc comment).
gametime_is_daytime() already existed before this session (used by the
lamplighter mob, mob_ai.c) -- what's new is actually WIRING darkness into
`look`/`exits` so carried light sources matter for the first time. Covers:
  1. `weather` reports a real sky condition and day/night status.
  2. A plain outdoor room at night, no light: `look`/`exits` show only
     darkness.
  3. Lighting a carried torch restores normal vision in that same room.
  4. An ALWAYS-LIT room ignores night/no-light entirely.
  5. An INDOORS room ignores night/no-light entirely.

    python3 tests/smoke_test_weather.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_weather", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_OUTDOOR = 940000 + (int(time.time()) % 60000)
ROOM_LIT = ROOM_OUTDOOR + 1
ROOM_INDOORS = ROOM_OUTDOOR + 2
TORCH_VNUM = 105  # real seeded OBJ_CAT_LIGHT, val: 5/-1/40/0 (unrefuelable, starts unlit)

ROOM_FLAG_ALWAYS_LIT = 1
ROOM_FLAG_INDOORS = 8


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
    send_line(s, "1"); recv_all(s)  # territory: urban
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


def goto_room(sock, name, pw, room_vnum):
    """Moves `name` to `room_vnum` via load_room -- `quit!` first so a
    raw close's linkdead-body-resume doesn't ignore the change (same
    lesson every other sandbox-room test this session documents)."""
    sql(f"UPDATE player SET load_room={room_vnum} WHERE name='{name}';")
    cmd(sock, "quit!")
    sock.close()
    return relog(name, pw)


imm_name, imm_pw = f"Wximmb{_suffix}", "wximmpw1234"
mort_name, mort_pw = f"Wxmortb{_suffix}", "wxmortpw1234"

si = make_char(imm_name, imm_pw); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sA = make_char(mort_name, mort_pw)
cmd(sA, "quit!")
sA.close()

for vnum, flag, name in ((ROOM_OUTDOOR, 0, "Weather Sandbox Outdoor"),
                          (ROOM_LIT, ROOM_FLAG_ALWAYS_LIT, "Weather Sandbox Always-Lit"),
                          (ROOM_INDOORS, ROOM_FLAG_INDOORS, "Weather Sandbox Indoors")):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare sandbox room.\\n',NULL,{flag},0,0,0,0,0,0,0,0,0);")

si = relog(imm_name, imm_pw)

# --- 1: weather reports a real condition ---
out = cmd(si, "weather")
check(any(w in out for w in ("clear", "cloudy", "rainy", "stormy")), "weather reports a real sky condition")
check("daylight" in out or "dark" in out, "weather reports day/night status")

# Force night deterministically. A single big jump doesn't work -- landing
# in the 20:00-06:00 window depends on the (unknown) starting hour, and a
# fixed-size jump just as reliably lands back in daylight as it does at
# night. Poll in small increments instead, checking after each: the day
# window is at most 14h (56 ticks of 15 game-minutes each), so working
# through it 10 ticks (2.5h) at a time is guaranteed to cross into night
# within a bounded number of iterations, however the starting hour landed.
for _ in range(10):
    out = cmd(si, "weather")
    if "dark" in out:
        break
    cmd(si, "aitick 10")
else:
    out = cmd(si, "weather")
check("dark" in out, "forcing time forward reliably lands in the night window")

# --- 2: a plain outdoor room at night with no light shows only darkness ---
sA = relog(mort_name, mort_pw)
sA = goto_room(sA, mort_name, mort_pw, ROOM_OUTDOOR)
out = cmd(sA, "look")
check("pitch black" in out, "a dark outdoor room with no light shows only darkness on look")
out = cmd(sA, "exits")
check("pitch black" in out, "the same darkness gate applies to exits")

# --- 3: a lit carried torch restores normal vision there ---
cmd(si, f"goto {ROOM_OUTDOOR}")
cmd(si, f"load obj {TORCH_VNUM}")
cmd(si, "drop torch")
cmd(sA, "get torch")
cmd(sA, "light torch")
out = cmd(sA, "look")
check("pitch black" not in out, "a lit carried torch restores normal vision in the dark room")

# --- 4/5: ALWAYS-LIT and INDOORS rooms ignore night entirely, even with
# the torch extinguished ---
cmd(sA, "extinguish torch")
sA = goto_room(sA, mort_name, mort_pw, ROOM_LIT)
out = cmd(sA, "look")
check("pitch black" not in out, "an ALWAYS-LIT room is never dark, light or no light")

sA = goto_room(sA, mort_name, mort_pw, ROOM_INDOORS)
out = cmd(sA, "look")
check("pitch black" not in out, "an INDOORS room is never dark, light or no light")

# Cleanup: this test forces the shared server's game clock forward and
# leaves it wherever it lands (persisted to game_config, survives past
# this run) -- restore a neutral daytime hour afterward so it doesn't
# silently break some OTHER, unrelated test's plain (non-ALWAYS-LIT)
# sandbox room the next time anything calls `look`/`exits` there. Same
# "don't leave shared mutable state behind" concern as any other global
# fixture.
for _ in range(10):
    out = cmd(si, "weather")
    if "daylight" in out:
        break
    cmd(si, "aitick 10")

sA.close()
si.close()
announce_done("smoke_test_weather", host, port)
print("=== ALL CHECKS PASSED ===")
