#!/usr/bin/env python3
"""Smoke test for (c) per-sector effects -- the last slice of the
room-flag TODO. Ports the spirit of the original's TerrainInfo
hunger/thirst columns (misc/constants.cc -> TBeing::foodNDrink): hot/arid
and hard-travel sectors burn thirst/hunger faster. The derived per-sector
weights (room.c: sector_thirst_rate / sector_hunger_rate, plus the older
sector_move_cost) are surfaced in the immortal builder-header of look
(cmd_look.c) as '[ NAME | mvN thrN hunN ]', so this test reads them back
deterministically per sector rather than racing the ~60s probabilistic
vitals drain tick.

  * PLAINS   -> baseline mv1 thr2 hun2 (no sector penalty; drain unchanged)
  * DESERT   -> mv2 thr6 hun4 (parches AND starves -- hardest survival)
  * SAVANNAH -> mv2 thr5 hun2 (arid: thirst only)
  * T.MTNS   -> mv5 thr2 hun4 (exertion: hunger only, costly to cross)

    python3 tests/smoke_test_sector_effects.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, cmd_until, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce('smoke_test_sector_effects', host, port)

_seed = int(time.time())
_sfx = ''.join(chr(ord('a') + (_seed // 26**i) % 26) for i in range(4))
BASE = 987200 + (_seed % 600)
PLAINS, DESERT, SAVANNAH, TMTNS = BASE, BASE + 1, BASE + 2, BASE + 3

# sector indices (src/core/room.c SECTOR_NAMES): 17 PLAINS, 34 DESERT,
# 35 SAVANNAH, 22 TEMPERATE MOUNTAINS.
SECT = {PLAINS: 17, DESERT: 34, SAVANNAH: 35, TMTNS: 22}
# expected (mv, thr, hun) per the room.c bucket rules.
EXPECT = {
    PLAINS:   (1, 2, 2),
    DESERT:   (2, 6, 4),
    SAVANNAH: (2, 5, 2),
    TMTNS:    (5, 2, 4),
}
LIT = 1
pw = 'sectpw1'
imm = f'Sect{_sfx}i'


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, 'y', pw, pw, 'new', name, '1', '1', '1', 'done', 'done'):
        send_line(s, step); recv_all(s, 0.5)
    s.close()


def login(name, pw):
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


vnums = tuple(SECT.keys())
sql(f'DELETE FROM room WHERE vnum IN {vnums};')
for v, sec in SECT.items():
    mkroom(v, f'Sect Test {v}', sec)

s = None
try:
    make_char(imm, pw)
    sql(f"UPDATE player_progress SET level=60, true_level=60 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    s = login(imm, pw)

    for v in vnums:
        mv, thr, hun = EXPECT[v]
        tag = f'mv{mv} thr{thr} hun{hun}'
        out = cmd_until(s, f'goto {v}', tag, deadline=8.0)
        check(tag in out,
              f'room {v} (sector {SECT[v]}) builder-header shows [{tag}]')

    print('>>> ALL SECTOR-EFFECT CHECKS PASSED')
finally:
    if s:
        try: s.close()
        except OSError: pass
    sql(f'DELETE FROM room WHERE vnum IN {vnums};')
    sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    sql(f"DELETE FROM player WHERE name='{imm}';")
    announce_done('smoke_test_sector_effects', host, port)
