#!/usr/bin/env python3
"""Per-round mana depletion for delayed casts (user 2026-08-16:
"mana should deplete for each round of casting, if it takes 3 rounds to
cast then divide by 3 and apply each round, that way if they lose
concentration it doesn't cost the same as a full cast").

A Mage/Druid cast no longer pays its whole mana cost up front -- the
multi-round casting task (spellcast.c) now draws a proportional slice
each round, and the final round squares those integer-divided slices up
to exactly the full cost. This test pins the load-bearing invariant: a
cast that actually runs the multi-round DELAYED path (not an instant
fumble) charges exactly the spell's full cost -- no residue, no
double-charge -- proving the per-round draws sum precisely to the cost.
A cast broken mid-way then pays only for the rounds it spent, which is
the same loop stopping early plus the reset paths in spellcast.c.

Two things make this rigorous rather than accidentally true:
  * The caster's gust proficiency is seeded to 100, so the cast reliably
    passes its skill roll and takes the real delayed path (a fumble would
    also burn the full 10 up front, which would mask a broken per-round
    charge -- so we additionally assert the delayed-cast completion
    flavor "strains at its limits" is present and no "fumble" appears).
  * Passive mana regen (this session's ~36s Sneezy mana tick) is held off
    two ways: the caster starts AT max mana (regen only fires below max),
    and stays in a fight for the whole cast (mana_piety_regen_tick_run()
    skips fighting beings). So the post-cast reading is exact.

gust's real cost is 10 (MANA_10 upstream).

    python3 tests/smoke_test_cast_perround.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_cast_perround", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
ROOM = 979000 + (int(time.time()) % 1000)


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


pw = "perroundpw123"
caster = f"Percas{_suffix}"
dummy = f"Perdum{_suffix}"    # sparring partner: keeps the caster fighting (regen off)
victim = f"Pervic{_suffix}"   # case 2 gust-target -- quits mid-cast, then is purged
imm = f"Perimm{_suffix}"      # level-60 helper: purges the quit victim's lingering being

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Perround Arena','A bare sparring floor.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

sockets = []
try:
    make_char(caster, pw, "1")   # Mage
    make_char(dummy, pw, "3")    # Warrior punching bag
    make_char(victim, pw, "3")   # Warrior gust-target for case 2
    make_char(imm, pw, "3")      # level-60 purge helper
    sql(f"UPDATE player SET class=0 WHERE name='{caster}';")
    sql(f"UPDATE player SET class=2 WHERE name='{dummy}';")
    sql(f"UPDATE player SET class=2 WHERE name='{victim}';")
    sql(f"UPDATE player SET class=2 WHERE name='{imm}';")
    for n in (caster, dummy, victim, imm):
        sql(f"UPDATE player SET load_room={ROOM} WHERE name='{n}';")
    # Purge helper: level 60 so `purge linkdead` (58+) is available.
    sql(f"UPDATE player_progress SET level=60 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm}');")
    # Caster: level 20 + full basic discipline (cast access). Starts at 100
    # mana -- being_calc_max_mana() recomputes a Mage's max to ~100 on load,
    # so 100 lands the caster exactly AT its cap, where the regen tick skips
    # it (it only fires below max). Huge HP so the sparring never threatens
    # the multi-round cast.
    cid = f"(SELECT id FROM player WHERE name='{caster}')"
    sql(f"UPDATE player_progress SET level=20, basic_disc_pct=100, mana=100, "
        f"max_mana=100, hp=9999, max_hp=9999 WHERE player_id={cid};")
    # gust proficiency 100 -> the skill roll always succeeds
    # (skill_roll_success: pct >= 100 is unconditional), so the cast takes
    # the real multi-round delayed path instead of fumbling.
    sql(f"DELETE FROM player_skill WHERE player_id={cid} AND skill_name='gust';")
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ({cid}, 'gust', 100, 0);")
    # Dummy + victim: huge HP so they survive (dummy keeps the caster
    # 'fighting' -> regen suppressed; neither dies to gust).
    for n in (dummy, victim):
        sql(f"UPDATE player_progress SET level=20, hp=9999, max_hp=9999 "
            f"WHERE player_id=(SELECT id FROM player WHERE name='{n}');")

    sc = login(caster, pw); sockets.append(sc)
    sd = login(dummy, pw); sockets.append(sd)
    sv = login(victim, pw); sockets.append(sv)
    si = login(imm, pw); sockets.append(si)
    cmd(sc, "toggle pk"); cmd(sd, "toggle pk"); cmd(sv, "toggle pk")

    # gust's reagent, same as smoke_test_mana section 3 -- one per cast.
    cmd(sc, "load obj 200"); cmd(sc, "load obj 200")
    recv_all(sc, 0.3)

    # Start the fight FIRST -- from here on the caster is fighting, so the
    # ~36s mana-regen tick skips them and every reading below is stable.
    cmd(sc, f"kill {dummy}")
    recv_all(sc, 0.6); recv_all(sd, 0.6)

    mana_before = mana_of(sc)
    check(mana_before == 100,
          f"caster starts a fight at its full 100 mana (at cap, so regen skips it) (got {mana_before})")

    # Cast, collecting the whole multi-round transcript (rounds arrive one
    # ~1.2s tick apart; the final round prints the completion flavor and
    # resolves the same tick). 6s clears the longest 3-round cast.
    send_line(sc, "cast gust")
    buf = ""
    t0 = time.time()
    while time.time() - t0 < 6:
        buf += recv_all(sc, 0.5)
    out_cast = strip(buf)

    check("strains at its limits" in out_cast,
          "the cast ran the full multi-round DELAYED path (its completion flavor appeared), "
          "so this really exercises per-round charging and not an instant fumble")
    check("fumble" not in out_cast.lower(),
          "the cast did not fumble (a fumble would burn the whole cost up front and mask a "
          "broken per-round charge)")

    mana_after = mana_of(sc)
    check(mana_after == 90,
          f"the completed multi-round cast charged exactly gust's full 10 mana -- no more, no "
          f"less (100 -> {mana_after}); the per-round draws summed precisely to the cost while "
          f"the ongoing fight held passive regen off")

    # --- Case 2: an INTERRUPTED cast charges only the rounds it spent ---
    # The caster is still sparring `dummy` (regen stays off) and now sits at
    # 90 mana. Aim a fresh gust at `victim` (a named target sets cast_target
    # to that being) and, an instant later, make `victim` quit and have the
    # immortal purge its lingering linkdead body. being_destroy() clears the
    # caster's cast_target, so the very next casting tick fizzles the
    # half-formed spell WITHOUT charging any further rounds. Only round 1's
    # share (paid synchronously at cast start) was ever drawn, so the caster
    # lands strictly BETWEEN a completed cast (would spend the full 10, to 0)
    # and an unpaid one (would stay at 10) -- proof the cost is per round.
    cmd(sc, f"cast gust {victim}")
    cmd(sv, "quit!")            # victim goes linkdead -- its being still lingers
    recv_all(sv, 0.2)
    cmd(si, "purge linkdead")  # extract that lingering being -> clears caster's cast_target
    time.sleep(6)              # let the fizzle land and the input-lock clear
    recv_all(sc, 1.0)

    mana_interrupted = mana_of(sc)
    check(mana_interrupted is not None and 80 < mana_interrupted < 90,
          f"an interrupted cast charged only its elapsed round(s), strictly less than a full "
          f"cast: mana {mana_after} -> {mana_interrupted} (a completed cast would have spent the "
          f"full 10 down to 80; charging nothing would have left it at 90)")

    print("=== ALL CHECKS PASSED ===")
finally:
    for s in sockets:
        try:
            cmd(s, "quit!"); s.close()
        except Exception:
            pass
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
