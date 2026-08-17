#!/usr/bin/env python3
"""Smoke test for Druid beast summon + the mob pathfinding subsystem.

A Druid casts 'beast summon'; a gray wolf (vnum 570) spawns several rooms
away and paths back to the caster one hop per combat round (mob_ai.c's
mob_hunt_tick / mob_path_next_dir BFS), becoming the caster's charmed pet
on arrival (hunt_befriend). Verifies: the spell reports the beast is on
its way, and within a generous window the wolf arrives and joins as a pet.

    python3 tests/smoke_test_beast_summon.py [host] [port]
"""
import socket, sys, time
from mud_test_utils import send_line, recv_all, check, sql, cmd, drain, announce

host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
announce('smoke_test_beast_summon', host, port)

_seed = int(time.time())
_sfx = ''.join(chr(ord('a') + (_seed // 26**i) % 26) for i in range(4))
pw = 'beastpw1'
dru = 'Beas' + _sfx
imm = 'Beai' + _sfx


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


si = sd = None
try:
    make_char(imm, '1')
    make_char(dru, '5')   # Druid
    sql("UPDATE player_progress SET level=60, true_level=60 WHERE player_id="
        "(SELECT id FROM player WHERE name='%s');" % imm)
    sql("UPDATE player_progress SET level=40, true_level=40, basic_disc_pct=100, "
        "combat_disc_pct=100, advanced_disc_pct=100, mana=5000, max_mana=5000 "
        "WHERE player_id=(SELECT id FROM player WHERE name='%s');" % dru)
    sql("INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        "SELECT id, 'beast summon', 100, 0 FROM player WHERE name='%s' "
        "ON DUPLICATE KEY UPDATE pct=100;" % dru)

    si = login(imm); sd = login(dru)

    # Put the druid in a densely connected hub (23008 has all 10 exits
    # into a tight room cluster) so the summoned wolf has a path back.
    cmd(si, 'goto 23008')
    cmd(si, 'transfer %s 23008' % dru)
    drain(sd)

    send_line(sd, 'cast beast summon')
    out = ''
    _d = time.time() + 3.0
    while time.time() < _d and 'wild' not in out.lower():
        out += recv_all(sd, 0.5)
    check('answers' in out or 'at your side' in out or 'toward you' in out,
          'beast summon reports a wolf answering the call')

    # Now wait for the wolf to path home and befriend the druid. One hop
    # per combat round (~1.2s); it spawned up to 4 rooms out, so give it
    # a wide margin.
    arrived = False
    _d = time.time() + 35.0
    buf = ''
    while time.time() < _d:
        buf += recv_all(sd, 1.0)
        if 'nuzzles your hand' in buf or 'at your side' in buf:
            arrived = True
            break
    check(arrived, 'the summoned wolf paths back and becomes the caster pet')

    # Confirm the wolf really made it into the caster's room (it is now a
    # charmed pet, so it stays put instead of wandering off).
    drain(sd)
    send_line(sd, 'look')
    g = ''
    _d = time.time() + 2.0
    while time.time() < _d and 'wolf' not in g.lower():
        g += recv_all(sd, 0.5)
    check('wolf' in g.lower(), 'the wolf is standing in the room with the caster')

    print('>>> ALL BEAST SUMMON CHECKS PASSED')
finally:
    for s2 in (si, sd):
        if s2:
            try:
                s2.close()
            except Exception:
                pass
