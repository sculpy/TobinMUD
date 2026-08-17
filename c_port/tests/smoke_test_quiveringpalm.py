#!/usr/bin/env python3
"""Smoke test for the quivering palm spec-proc (Monk, level 42).

A skilled Monk's death-touch instantly slays a normal foe (100 + victim
max HP damage), then locks out re-use until the recast cooldown
(AFFECT_QUIVERING_PALM_COOLDOWN) expires -- the sole balancing gate, since
Tobin monks have no mana to spend as upstream's cost.

    python3 tests/smoke_test_quiveringpalm.py [host] [port]
"""
import socket, sys, time
from mud_test_utils import send_line, recv_all, check, sql, cmd, cmd_until, drain, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
announce('smoke_test_quiveringpalm', host, port)

_seed = int(time.time())
_sfx = ''.join(chr(ord('a') + (_seed // 26**i) % 26) for i in range(4))
ROOM = 989700 + (_seed % 400)
TANK = 1735   # 'testmob l35' -- a tough mob, to prove the touch kills regardless
LIT = 1
pw = 'quivpw1'
imm  = f'Immq{_sfx}'
monk = f'Monq{_sfx}'


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
    f"VALUES ({ROOM},0,0,0,'Quiv Test Room','A bare test room.\n',NULL,{LIT},0,0,0,0,0,0,0,0,0)"
    f" ON DUPLICATE KEY UPDATE name=VALUES(name);")

si = sm = None
try:
    make_char(imm, '1')
    make_char(monk, '6')   # Monk
    sql(f"UPDATE player_progress SET level=60, true_level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    sql(f"UPDATE player_progress SET level=45, true_level=45, basic_disc_pct=100, combat_disc_pct=100, advanced_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{monk}');")
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"SELECT id, 'quivering palm', 100, 0 FROM player WHERE name='{monk}' ON DUPLICATE KEY UPDATE pct=100;")

    si = login(imm); sm = login(monk)
    cmd(si, f'goto {ROOM}')
    cmd(si, f'load mob {TANK}')
    cmd(si, f'transfer {monk} {ROOM}')
    drain(sm)

    out = cmd_until(sm, 'quiveringpalm testmob', 'quivering palm', deadline=8.0)
    check('slain instantly by the dreaded quivering palm' in out,
          'quivering palm instantly slays the mob')

    time.sleep(2.5)  # let the COMBAT_ROUND_PULSES action-wait clear so we reach the cooldown check
    drain(sm)
    out = cmd_until(sm, 'quiveringpalm testmob', 'centered', deadline=8.0)
    check('not yet centered' in out,
          'a second quivering palm is refused by the recast cooldown')

    print('>>> ALL QUIVERING PALM CHECKS PASSED')
finally:
    for s in (si, sm):
        if s:
            try: s.close()
            except OSError: pass
    sql(f'DELETE FROM room WHERE vnum={ROOM};')
    for n in (imm, monk):
        sql(f"DELETE FROM player_skill WHERE player_id=(SELECT id FROM player WHERE name='{n}');")
        sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{n}');")
        sql(f"DELETE FROM player WHERE name='{n}';")
    announce_done('smoke_test_quiveringpalm', host, port)
