#!/usr/bin/env python3
"""Smoke test for ranged combat (shoot) + the Fast load skill.

A Warrior wields a bow (vnum 170), carries arrows (vnum 166) and fires at
a tank mob with `shoot`. Verifies: the ranged-weapon gate (no bow wielded
-> refused), a successful shot ('You loose ...'), the Fast load reload cut
('... in a blur'), and ammunition depletion ('out of ammunition' once the
arrows run out).

    python3 tests/smoke_test_shoot.py [host] [port]
"""
import socket, sys, time
from mud_test_utils import send_line, recv_all, check, sql, cmd, drain, announce

host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
announce('smoke_test_shoot', host, port)

_seed = int(time.time())
_sfx = ''.join(chr(ord('a') + (_seed // 26**i) % 26) for i in range(4))
pw = 'shootpw1'
war = 'Shot' + _sfx
imm = 'Shoi' + _sfx
ROOM = 23008
TANK = 1735
BOW = 170
ARROW = 166


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


si = sw = None
try:
    make_char(imm, '1')
    make_char(war, '3')   # Warrior
    sql("UPDATE player_progress SET level=60, true_level=60 WHERE player_id="
        "(SELECT id FROM player WHERE name='%s');" % imm)
    # Level 50 with a big HP pool so the warrior easily survives the parallel
    # melee round while we run the shots; basic_disc for the CLASS-tier skill.
    sql("UPDATE player_progress SET level=50, true_level=50, hp=8000, max_hp=8000, "
        "basic_disc_pct=100, combat_disc_pct=100 WHERE player_id="
        "(SELECT id FROM player WHERE name='%s');" % war)
    sql("INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        "SELECT id, 'fast load', 100, 0 FROM player WHERE name='%s' "
        "ON DUPLICATE KEY UPDATE pct=100;" % war)

    si = login(imm); sw = login(war)
    cmd(si, 'goto %d' % ROOM)
    cmd(si, 'transfer %s %d' % (war, ROOM))

    # Hand the warrior a bow and three arrows.
    cmd(si, 'load obj %d' % BOW)
    cmd(si, 'give bow %s' % war)
    for _ in range(3):
        cmd(si, 'load obj %d' % ARROW)
        cmd(si, 'give arrow %s' % war)
    drain(sw)

    # (A) Ranged-weapon gate: bow carried but NOT wielded yet.
    send_line(sw, 'shoot someone')
    out = recv_all(sw, 1.0)
    check('ranged weapon' in out, 'shooting with no ranged weapon wielded is refused')

    # Wield the bow and bring in a target.
    cmd(sw, 'wield bow')
    cmd(si, 'load mob %d' % TANK)
    drain(sw)

    saw_loose = saw_blur = saw_out = False
    # Three arrows -> three shots land, the fourth reports empty. Sleep past
    # the reload lag (up to 3 rounds ~3.6s) between attempts.
    for i in range(5):
        send_line(sw, 'shoot testmob')
        o = ''
        _d = time.time() + 3.0
        while time.time() < _d:
            o += recv_all(sw, 0.6)
        if 'You loose' in o:
            saw_loose = True
        if 'in a blur' in o:
            saw_blur = True
        if 'out of ammunition' in o:
            saw_out = True
            break
        time.sleep(4.5)

    check(saw_loose, 'a shot lands (You loose ... strike ...)')
    check(saw_blur, 'Fast load cuts the reload (nock your next shot in a blur)')
    check(saw_out, 'running out of arrows reports out of ammunition')

    print('>>> ALL SHOOT CHECKS PASSED')
finally:
    for s2 in (si, sw):
        if s2:
            try:
                s2.close()
            except Exception:
                pass
