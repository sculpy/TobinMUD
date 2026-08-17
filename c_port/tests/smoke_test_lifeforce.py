#!/usr/bin/env python3
"""Smoke test for the Druid Lifeforce resource pool.

Druid Lifeforce is now a real, scaling pool (being_calc_max_mana() off the
learn-by-doing 'lifeforce' skill, trained on every cast in cmd_cast.c),
not the old flat-100 placeholder. Verifies: score labels the pool
'Lifeforce' (not Mana/Piety), the pool has a real positive maximum, and
casting a Druid spell spends from it.

    python3 tests/smoke_test_lifeforce.py [host] [port]
"""
import socket, sys, time, re
from mud_test_utils import send_line, recv_all, check, sql, cmd, drain, announce

host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
announce('smoke_test_lifeforce', host, port)

_seed = int(time.time())
_sfx = ''.join(chr(ord('a') + (_seed // 26**i) % 26) for i in range(4))
pw = 'lifepw1'
dru = 'Life' + _sfx


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


def act(s, line, wait=0.8):
    send_line(s, line)
    time.sleep(wait)
    out = ''
    _d = time.time() + 2.5
    while time.time() < _d:
        out += recv_all(s, 0.5)
    return out


sd = None
try:
    make_char(dru, '5')   # Druid
    # Level + disciplines so a real spell is castable; train the lifeforce
    # skill high so the pool is clearly bigger than the flat-100 floor.
    sql("UPDATE player_progress SET level=40, true_level=40, basic_disc_pct=100, "
        "combat_disc_pct=100, advanced_disc_pct=100 WHERE player_id="
        "(SELECT id FROM player WHERE name='%s');" % dru)
    for sk in ('lifeforce', 'apply herbs'):
        sql("INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
            "SELECT id, '%s', 100, 0 FROM player WHERE name='%s' "
            "ON DUPLICATE KEY UPDATE pct=100;" % (sk, dru))
    # Injure the druid so the heal actually applies (and so a full pool at
    # login isn't instantly refilled by regen masking the spend).
    sql("UPDATE player_progress SET hp=40, max_hp=800 WHERE player_id="
        "(SELECT id FROM player WHERE name='%s');" % dru)

    sd = login(dru)

    # (1) score labels the pool Lifeforce and shows a real maximum.
    sc = act(sd, 'score')
    check('Lifeforce' in sc, "score labels the Druid pool 'Lifeforce'")
    m = re.search(r'Lifeforce[:\s]*([0-9]+)\s*/\s*([0-9]+)', sc)
    if not m:
        m = re.search(r'Lifeforce[^0-9]*([0-9]+)[^0-9]+([0-9]+)', sc)
    check(bool(m), 'score shows a Lifeforce current/max pair')
    cur, mx = (int(m.group(1)), int(m.group(2))) if m else (0, 0)
    # lifeforce skill at 100 -> 100 + 100*3 = 400 (times race mult); must
    # be well above the old flat-100 placeholder.
    check(mx > 150, 'the lifeforce skill scales the pool above the old flat 100 (max=%d)' % mx)

    # (2) casting spends Lifeforce.
    cmd(sd, 'stand')
    before = cur
    act(sd, 'cast apply herbs', wait=1.0)
    time.sleep(4.0)  # let the multi-round cast charge its Lifeforce slices
    sc2 = act(sd, 'score')
    m2 = re.search(r'Lifeforce[:\s]*([0-9]+)\s*/\s*([0-9]+)', sc2) or \
         re.search(r'Lifeforce[^0-9]*([0-9]+)[^0-9]+([0-9]+)', sc2)
    after = int(m2.group(1)) if m2 else before
    check(after < before, 'casting a Druid spell spends Lifeforce (%d -> %d)' % (before, after))

    print('>>> ALL LIFEFORCE CHECKS PASSED')
finally:
    if sd:
        try:
            sd.close()
        except Exception:
            pass
