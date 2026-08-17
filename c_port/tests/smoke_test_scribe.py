#!/usr/bin/env python3
"""Smoke test for the scribe spec-proc (Mage, level 25).

A Mage inscribes a spell they know onto a handwritten scroll (an ephemeral
obj carrying the spell on obj_t.scribed_spell); the scroll then casts that
spell on use, honoured ahead of the vnum-keyed obj_magic table
(cmd_use.c). Gibberish is refused. Payload: 'arctic blast' (a damage
spell apply_item_effect() can channel), used on a tank mob.

    python3 tests/smoke_test_scribe.py [host] [port]
"""
import socket, sys, time
from mud_test_utils import send_line, recv_all, check, sql, cmd, cmd_until, drain, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
announce('smoke_test_scribe', host, port)

_seed = int(time.time())
_sfx = ''.join(chr(ord('a') + (_seed // 26**i) % 26) for i in range(4))
ROOM = 990300 + (_seed % 300)
TANK = 1735
pw = 'scribepw1'
mage = f'Scrb{_sfx}'
imm  = f'Scri{_sfx}'


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
    f"VALUES ({ROOM},0,0,0,'Scribe Test Room','A bare test room.\n',NULL,1,0,0,0,0,0,0,0,0,0)"
    f" ON DUPLICATE KEY UPDATE name=VALUES(name);")

si = sm = None
try:
    make_char(imm, '1')
    make_char(mage, '1')   # Mage
    sql(f"UPDATE player_progress SET level=60, true_level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
    sql(f"UPDATE player_progress SET level=30, true_level=30, basic_disc_pct=100, combat_disc_pct=100, advanced_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{mage}');")
    for sp in ('scribe', 'arctic blast'):
        sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
            f"SELECT id, '{sp}', 100, 0 FROM player WHERE name='{mage}' ON DUPLICATE KEY UPDATE pct=100;")

    si = login(imm); sm = login(mage)
    cmd(si, f'goto {ROOM}')
    cmd(si, f'transfer {mage} {ROOM}')
    cmd(si, f'load mob {TANK}')
    drain(sm)

    def act(line, wait=0.8):
        send_line(sm, line)
        time.sleep(wait)
        return recv_all(sm, 1.2)

    out = act('scribe zzqqxx')
    check("can't scribe a scroll of that type" in out, 'gibberish spell is refused')

    out = act('scribe arctic blast')
    check('handwritten scroll of arctic blast' in out, 'scribe reports minting the scroll')

    out = act('inventory')
    check('handwritten scroll of arctic blast' in out, 'the scroll is in the mage inventory')

    # a newbie mage's inventory is long enough to trip the pager
    # ('[ ENTER for more, Q to stop ]'); if left open it eats the next
    # command (the  line advances the pager instead of running), so
    # exhaust it before doing anything else.
    while 'ENTER for more' in out or 'Q to stop' in out:
        send_line(sm, 'q')
        out = recv_all(sm, 0.8)

    # inventory output arrives over several game pulses; a short drain can
    # return in a pulse gap and let the tail bleed into the use capture, so
    # drain with a quiet window wider than the pulse before trusting .
    drain(sm, quiet=0.7)
    send_line(sm, 'use scroll testmob')
    out = ''
    _deadline = time.time() + 7.0
    while time.time() < _deadline and 'arctic blast hits' not in out:
        out += recv_all(sm, 0.6)
    check('arctic blast hits' in out, 'using the scribed scroll casts its spell at the foe')

    print('>>> ALL SCRIBE CHECKS PASSED')
finally:
    for s2 in (si, sm):
        if s2:
            try: s2.close()
            except OSError: pass
    sql(f'DELETE FROM room WHERE vnum={ROOM};')
    for n in (imm, mage):
        sql(f"DELETE FROM player_skill WHERE player_id=(SELECT id FROM player WHERE name='{n}');")
        sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{n}');")
        sql(f"DELETE FROM player WHERE name='{n}';")
    announce_done('smoke_test_scribe', host, port)
