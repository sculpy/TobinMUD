#!/usr/bin/env python3
"""Delayed proficiency FUMBLE for `cast` (user 2026-08-16: "a proficiency
FUMBLE still fizzles instantly for its full cost -- make that failure play
out over a round or two too").

The sibling smoke_test_cast_perround.py already pinned the SUCCESS path:
a cast that passes its proficiency roll runs 2-3 rounds and draws mana a
slice at a time. This test pins the FAILURE path's new behavior: a cast
that FAILS its proficiency roll no longer fizzles in one instant line for
the full cost -- it now enters the same multi-round casting task (a
shorter 1-2 rounds, marked cast_fumble in spellcast.c), so the caster
visibly strains through the botched incantation and pays mana per round,
then fizzles at the end instead of resolving any effect.

What's asserted, all against ONE observed fumble while the caster is
sparring (so the ~36s passive mana tick stays off and every reading is
exact):
  * the delayed-fumble OPENER ("tangle on your tongue") appeared -- the
    failed roll took the new deferred path, not the old instant one;
  * a real casting-animation line ("Arcane energy crackles") appeared --
    proof the botch played out over at least one full round of flavor,
    not a single-line instant fizzle;
  * the delayed fizzle line ("fizzles into nothing") appeared and the
    SUCCESS completion boast ("strains at its limits") did NOT -- the
    cast failed and resolved no effect;
  * mana dropped by exactly gust's full cost (10) -- a fumble that runs
    to completion still costs the whole spell, the per-round draws
    (spellcast_pay_round) summing precisely to the cost, same mechanism
    smoke_test_cast_perround.py already proved charges less when a cast
    is broken mid-way.

gust's real cost is 10 (MANA_10 upstream). The caster's gust proficiency
is seeded to 1 so skill_roll_success(1) fails ~99% of the time (pct<=0 is
an unconditional fail, pct>=100 an unconditional success -- 1 sits at the
fumble-almost-always end); the test still loops a few attempts in the
off chance an early roll succeeds.

    python3 tests/smoke_test_cast_fumble_perround.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_cast_fumble_perround", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
ROOM = 978000 + (int(time.time()) % 1000)


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw, char_class):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)   # race: human (mana_mult 1.0)
    send_line(s, "1"); recv_all(s)   # territory: urban
    send_line(s, char_class); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def mana_of(sock):
    out = cmd(sock, "score")
    m = re.search(r"Mana:\s*(\d+)", out)
    return int(m.group(1)) if m else None


pw = "fumblepw123"
caster = f"Fumcas{_suffix}"
dummy = f"Fumdum{_suffix}"    # sparring partner: keeps the caster fighting (regen off)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Fumble Arena','A bare sparring floor.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    make_char(caster, pw, "1")   # Mage
    make_char(dummy, pw, "3")    # Warrior punching bag
    sql(f"UPDATE player SET class=0 WHERE name='{caster}';")
    sql(f"UPDATE player SET class=2 WHERE name='{dummy}';")
    for n in (caster, dummy):
        sql(f"UPDATE player SET load_room={ROOM} WHERE name='{n}';")

    cid = f"(SELECT id FROM player WHERE name='{caster}')"
    # Caster: level 20 + full basic discipline (cast access), starts AT its
    # ~100 mana cap so passive regen (below-max only) skips it. Huge HP so
    # the sparring never threatens the multi-round cast.
    sql(f"UPDATE player_progress SET level=20, basic_disc_pct=100, mana=100, "
        f"max_mana=100, hp=9999, max_hp=9999 WHERE player_id={cid};")
    # gust proficiency 1 -> skill_roll_success(1) fails ~99% of the time,
    # so the cast reliably takes the new DELAYED-fumble path.
    sql(f"DELETE FROM player_skill WHERE player_id={cid} AND skill_name='gust';")
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ({cid}, 'gust', 1, 0);")
    sql(f"UPDATE player_progress SET level=20, hp=9999, max_hp=9999 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{dummy}');")

    sc = login(caster, pw); sockets.append(sc)
    sd = login(dummy, pw); sockets.append(sd)
    cmd(sc, "toggle pk"); cmd(sd, "toggle pk")

    # gust's reagent -- one consumed per cast attempt; load a handful.
    for _ in range(6):
        cmd(sc, "load obj 200")
    recv_all(sc, 0.3)

    # Fight FIRST -- from here the caster is 'fighting', so the ~36s mana
    # regen tick skips them and every reading below is stable.
    cmd(sc, f"kill {dummy}")
    recv_all(sc, 0.6); recv_all(sd, 0.6)

    mana_before = mana_of(sc)
    check(mana_before == 100,
          f"caster starts a fight at its full 100 mana (at cap, so regen skips it) (got {mana_before})")

    # Attempt the fumble. Loop a few times only in case an early 1%-chance
    # success sneaks through; assert on the FIRST cast that actually fumbles.
    fumbled = False
    for attempt in range(6):
        m_pre = mana_of(sc)
        send_line(sc, "cast gust")
        buf = ""
        t0 = time.time()
        while time.time() - t0 < 5:   # clears the longest 2-round fumble
            buf += recv_all(sc, 0.5)
        out_cast = strip(buf)
        if "fizzles into nothing" in out_cast:
            m_post = mana_of(sc)
            check("tangle on your tongue" in out_cast,
                  "the failed roll took the new DELAYED-fumble path (its opener appeared), "
                  "not the old one-line instant fizzle")
            check("Arcane energy crackles" in out_cast,
                  "the botched cast played out over at least a full round of casting animation "
                  "(a casting-flavor line appeared), not a single instant line")
            check("strains at its limits" not in out_cast,
                  "the fumble never showed the SUCCESS completion boast -- it failed and "
                  "resolved no effect")
            check(m_pre is not None and m_post is not None and (m_pre - m_post) == 10,
                  f"the completed fumble charged exactly gust's full 10 mana via its per-round "
                  f"draws -- no more, no less ({m_pre} -> {m_post})")
            fumbled = True
            break
        # Rare success (1%): a gust actually landed. Ignore and retry --
        # proficiency stays low, so the next attempt almost certainly fumbles.
        time.sleep(0.5)
        recv_all(sc, 0.5)

    check(fumbled, "a fumble was observed within the attempt budget (gust proficiency 1)")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
