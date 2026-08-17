#!/usr/bin/env python3
"""Smoke test for the hide spec-proc (Thief, level 31).

A Thief with maxed hide proficiency drops out of another mortal's room
person-listing on a successful hide, and reappears the moment concealment
breaks (here: by attacking). Faithful to upstream doHide as a stationary
stealth roll; the room-listing filter lives in cmd_look.c, the break in
being_break_hiding() (called from cmd_move.c/cmd_attack.c).

Setup: an immortal loads a tank mob into a sandbox room and transfers a
Thief (hider) and a plain mortal (observer) in. The observer's look is the
oracle -- immortal viewers bypass the hide filter by design, so the
observer must be mortal.

    python3 tests/smoke_test_hide.py [host] [port]
"""
import socket, sys, time
from mud_test_utils import send_line, recv_all, check, sql, cmd, cmd_until, drain, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
announce('smoke_test_hide', host, port)

_seed = int(time.time())
_sfx = ''.join(chr(ord('a') + (_seed // 26**i) % 26) for i in range(4))
ROOM = 989100 + (_seed % 500)
TANK = 1735   # 'testmob l35' -- survives so the hider stays in-room, fighting
LIT = 1
pw = 'hidepw1'
imm  = f'Immh{_sfx}'
hidr = f'Hidr{_sfx}'
obsr = f'Obsr{_sfx}'


def make_char(name, cls):
    s = socket.create_connection((host, port), timeout=5); recv_all(s)
    for step in (name, 'y', pw, pw, 'new', name, '1', '1', cls, 'done', 'done'):
        send_line(s, step); recv_all(s, 0.5)
    s.close()


def login(name):
    s = socket.create_connection((host, port), timeout=5); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, '1'); recv_all(s)
    cmd(s, 'color off')
    return s


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Hide Test Room','A bare test room.\n',NULL,{LIT},0,0,0,0,0,0,0,0,0)"
    f" ON DUPLICATE KEY UPDATE name=VALUES(name);")

si = sh = so = None
try:
    make_char(imm, '1')      # class irrelevant for the immortal
    make_char(hidr, '4')     # Thief
    make_char(obsr, '1')
    sql(f"UPDATE player_progress SET level=60, true_level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    sql(f"UPDATE player_progress SET level=35, true_level=35, basic_disc_pct=100, combat_disc_pct=100, advanced_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{hidr}');")
    hid_id = None
    # force hide proficiency to 100 so the roll is deterministic
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"SELECT id, 'hide', 100, 0 FROM player WHERE name='{hidr}' "
        f"ON DUPLICATE KEY UPDATE pct=100;")

    si = login(imm); sh = login(hidr); so = login(obsr)

    cmd(si, f'goto {ROOM}')
    cmd(si, f'load mob {TANK}')
    cmd(si, f'transfer {hidr} {ROOM}')
    cmd(si, f'transfer {obsr} {ROOM}')
    drain(so)

    out = cmd_until(so, 'look', hidr, deadline=8.0)
    check(hidr in out, 'observer sees the Thief in the room before hiding')

    out = cmd_until(sh, 'hide', 'hidden', deadline=8.0)
    check('hidden' in out.lower(), 'hide reports success (hidden from view)')

    drain(so)
    out = cmd_until(so, 'look', 'Test Room', deadline=8.0)
    check(hidr not in out, 'observer no longer sees the hidden Thief')

    # break hiding by attacking the tank mob; the Thief stays in-room, fighting
    cmd(sh, 'attack testmob')
    drain(so)
    out = cmd_until(so, 'look', hidr, deadline=8.0)
    check(hidr in out, 'attacking breaks hiding -- the Thief is visible again')

    print('>>> ALL HIDE CHECKS PASSED')
finally:
    for s in (si, sh, so):
        if s:
            try: s.close()
            except OSError: pass
    sql(f'DELETE FROM room WHERE vnum={ROOM};')
    for n in (imm, hidr, obsr):
        sql(f"DELETE FROM player_skill WHERE player_id=(SELECT id FROM player WHERE name='{n}');")
        sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{n}');")
        sql(f"DELETE FROM player WHERE name='{n}';")
    announce_done('smoke_test_hide', host, port)
