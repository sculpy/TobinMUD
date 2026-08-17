#!/usr/bin/env python3
"""Smoke test for the Tobin-original heat subsystem (user 2026-08-17).

A DELIBERATE INVENTION, not a port: SneezyMUD defines a TerrainInfo heat
column but never reads it, so there is no upstream behaviour to mirror
(see room.c sector_heat()). Two layers are exercised:

  1. sector_heat() values, read back deterministically from the immortal
     builder-header of look ('[ NAME | mvN thrN hunN heatN ]', cmd_look.c):
        VOLCANO LAVA 140, DESERT 120, JUNGLE 100, PLAINS 60,
        TUNDRA 0, ARCTIC WASTE -30.
  2. The cosmetic entry cue (cmd_move.c): a mortal transferred onto a mild
     plain and then walking east into an outdoor DESERT breaks into a
     sweat; walking into an ARCTIC WASTE shivers.

The damage layer (heatstroke/hypothermia HP chip in vitals.c) is not
asserted here -- it only fires on the ~60s drain tick, which cannot be
force-run for a live player by design (vitals.c incident writeup) -- same
reason smoke_test_room_flags_combat.py doesn't assert HOSPITAL regen.

    python3 tests/smoke_test_heat.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, cmd_until, drain, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce('smoke_test_heat', host, port)

_seed = int(time.time())
_sfx = ''.join(chr(ord('a') + (_seed // 26**i) % 26) for i in range(4))
BASE = 988300 + (_seed % 500)
# header-check rooms
LAVA, DESERT, JUNGLE, PLAINS, TUNDRA, ARCTIC = (BASE + i for i in range(6))
# behavioural walk rooms: a mild start with an east exit into an extreme
HOTSTART, COLDSTART = BASE + 10, BASE + 11
HEAT_SECT = {LAVA: (43, 140), DESERT: (34, 120), JUNGLE: (39, 100),
             PLAINS: (17, 60), TUNDRA: (4, 0), ARCTIC: (1, -30)}
LIT = 1
pw = 'heatpw1'
imm = f'Heat{_sfx}i'
mob = f'Heat{_sfx}m'   # the mortal walker
allrooms = list(HEAT_SECT.keys()) + [HOTSTART, COLDSTART, DESERT, ARCTIC]
allrooms = sorted(set(allrooms))


def make_char(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, 'y', pw, pw, 'new', name, '1', '1', '1', 'done', 'done'):
        send_line(s, step); recv_all(s, 0.5)
    s.close()


def login(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, '1'); recv_all(s)
    cmd(s, 'color off')
    return s


def mkroom(vnum, name, sector):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({vnum},0,0,0,'{name}','A bare test room.\n',NULL,{LIT},{sector},0,0,0,0,0,0,0,0);")


def mkexit(frm, to, direction=1):
    sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,"
        f"condition_flag,lock_difficulty,weight,key_num,destination) "
        f"VALUES ({frm},{direction},'','',0,0,-1,-1,-1,{to});")


sql(f'DELETE FROM roomexit WHERE vnum IN {tuple(allrooms)};')
sql(f'DELETE FROM room WHERE vnum IN {tuple(allrooms)};')
for v, (sec, _h) in HEAT_SECT.items():
    mkroom(v, f'Heat {v}', sec)
mkroom(HOTSTART, 'Heat Mild Start (hot)', 17)   # plains, heat 60
mkroom(COLDSTART, 'Heat Mild Start (cold)', 17)
mkexit(HOTSTART, DESERT)    # east into desert (heat 120)
mkexit(COLDSTART, ARCTIC)   # east into arctic waste (heat -30)

si = sv = None
try:
    make_char(imm)
    make_char(mob)
    sql(f"UPDATE player_progress SET level=60, true_level=60 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    si = login(imm)

    # ---- layer 1: sector_heat values via the immortal look header ----
    for v, (sec, h) in HEAT_SECT.items():
        tag = f'heat{h} ]'
        out = cmd_until(si, f'goto {v}', tag, deadline=8.0)
        check(tag in out, f'room {v} (sector {sec}) header shows heat{h}')

    # ---- layer 2: mortal entry sweat/shiver cue ----
    sv = login(mob)
    cmd(si, f'transfer {mob} {HOTSTART}')
    drain(sv)
    out = cmd_until(sv, 'east', 'sweat', deadline=8.0)
    check('sweat' in out, 'mortal walking into an outdoor DESERT breaks into a sweat')

    cmd(si, f'transfer {mob} {COLDSTART}')
    drain(sv)
    out = cmd_until(sv, 'east', 'shiver', deadline=8.0)
    check('shiver' in out, 'mortal walking into an ARCTIC WASTE shivers')

    print('>>> ALL HEAT CHECKS PASSED')
finally:
    for s in (si, sv):
        if s:
            try: s.close()
            except OSError: pass
    sql(f'DELETE FROM roomexit WHERE vnum IN {tuple(allrooms)};')
    sql(f'DELETE FROM room WHERE vnum IN {tuple(allrooms)};')
    for n in (imm, mob):
        sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{n}');")
        sql(f"DELETE FROM player WHERE name='{n}';")
    announce_done('smoke_test_heat', host, port)
