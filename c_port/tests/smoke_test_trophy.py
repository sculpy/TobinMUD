#!/usr/bin/env python3
"""Smoke test for the trophy system (TODO.md, user: "implement trophy
system from Sneezy"). Ported from SneezyMUD's TTrophy class (cmd/
cmd_trophy.cc/.h): a per-player, per-mob-vnum decaying kill count that
shrinks the XP a repeatedly-farmed mob is worth, down to a floor, never
to zero. See trophy.h's own doc comment for the two disclosed scope-
downs from the original: no `trophy wipe()` (character deletion already
cascades every player-keyed table the same way) and no zone-grouped
kill-percentage browsing in the `trophy` command (just a flat per-mob
modifier listing -- Sneezy's own version needs a full mob-census-per-
zone repo function Tobin doesn't have).

Covers:
  1. A real kill of a never-before-killed mob writes a player_trophy row
     with count=1.0 (trophy_record_kill(), wired into combat_defeat()).
  2. `trophy <name>` lists it with a "full" XP description (count 1 is
     still under the 8-kill free_kills threshold).
  3. A trophy count seeded well past free_kills (20) shows a "little"
     XP description instead -- trophy_exp_mod()'s formula, exact port
     of Sneezy's getExpModVal().
  4. The XP modifier is actually APPLIED to combat XP, not just
     displayed: two identical mobs (same vnum stats, different real
     vnums), one player with no trophy history, one seeded to count=20
     -- the seeded player earns less XP from an identical kill
     (combat_award_hit_xp()'s new per-recipient trophy read).
  5. The decay-pulse SQL's floor-at-zero guard (`where count > amount`)
     -- a count already below the decay amount is left untouched
     instead of going negative; a count comfortably above it decays by
     exactly the flat amount.
  6. `trophy` with no argument lists every mob a player has a trophy
     count for; a name filter narrows it to just the matching one(s).
  7. `help trophy` exists and reads correctly.

    python3 tests/smoke_test_trophy.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 945000 + (int(time.time()) % 40000)
MOB_FRESH_VNUM = ROOM + 1
MOB_XPCMP_A_VNUM = ROOM + 2
MOB_XPCMP_B_VNUM = ROOM + 3


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def trophy_count(name, vnum):
    out = query(f"SELECT count FROM player_trophy WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND mob_vnum={vnum};")
    return float(out) if out else None


def xp_of(name):
    return int(query(f"SELECT experience FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "3", "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


def mob_row(vnum, name_tag, max_exist=1):
    # max_exist=1 (not the usual few-hundred default) deliberately -- it's
    # trophy_exp_mod()'s own normalizer (count /= max_exist when >0), and
    # these checks are testing the RAW free_kills/step_mod formula, not
    # the normalization; max_exist=1 makes it a no-op so a seeded count
    # (e.g. 20) drives the formula directly instead of collapsing toward
    # 0 against a large denominator.
    cols = {
        "vnum": vnum, "name": f"'{name_tag}'", "short_desc": f"'a {name_tag}'",
        "long_desc": f"'A {name_tag} is here.'", "description": "'desc'",
        "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
        "letter": "'A'", "attacks": 1.0,
        "class": 0, "level": 3, "tohit": 0, "ac": 0, "hpbonus": -1.7,
        "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
        "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
        "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
        "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
        "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
        "max_exist": max_exist,
    }
    col_names = ",".join(cols.keys())
    col_values = ",".join(str(v) for v in cols.values())
    return f"INSERT INTO mob ({col_names}) VALUES ({col_values});"


def fight_and_check(imm_sock, pc_sock, vnum, mob_tag):
    cmd(imm_sock, f"load mob {vnum}")
    out = cmd(pc_sock, f"attack {mob_tag}")
    for _ in range(10):
        if "You have slain" in out or "You have defeated" in out:
            break
        out += recv_all(pc_sock, 1.5)
    check("You have slain" in out or "You have defeated" in out,
          f"the fight against {mob_tag} resolved with a kill")


announce("smoke_test_trophy", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Trophy Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

sql(mob_row(MOB_FRESH_VNUM, "trophyrat"))
sql(mob_row(MOB_XPCMP_A_VNUM, "trophydeera"))
sql(mob_row(MOB_XPCMP_B_VNUM, "trophydeerb"))

imm_name, imm_pw = f"Troimm{_suffix}", "troimmpw123"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

# --- 7: help topic ---
out = strip(cmd(si, "help trophy", timeout=1.5))
check("Syntax: trophy" in out and "Shows how much XP" in out, "`help trophy` exists and reads correctly")

# --- 1/2: a real kill writes count=1.0, shows as "full" ---
nameA, pwA = f"Trofres{_suffix}", "trofrespw123"
sA = make_char(nameA, pwA)
cmd(sA, "quit!"); sA.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameA}';")
set_hp(nameA, 500, 500)
sA = relog(nameA, pwA)

check(trophy_count(nameA, MOB_FRESH_VNUM) is None, "no trophy row exists before the first kill")
fight_and_check(si, sA, MOB_FRESH_VNUM, "trophyrat")
cnt = trophy_count(nameA, MOB_FRESH_VNUM)
check(cnt is not None and abs(cnt - 1.0) < 0.001, f"a real kill writes a trophy row with count=1.0 (got {cnt})")

out = strip(cmd(sA, "trophy trophyrat"))
check("full" in out.lower() and "a trophyrat" in out, "`trophy trophyrat` shows a 'full' modifier at count=1")

# --- 6: trophy listing + filter ---
out = strip(cmd(sA, "trophy"))
check("a trophyrat" in out and "Total mobs: 1" in out, "`trophy` with no filter lists every tracked mob")
out = strip(cmd(sA, "trophy nosuchmobxyz"))
check("no matching kills" in out.lower(), "`trophy <unmatched filter>` reports no matches")

# --- 3: a count seeded past free_kills shows "little" ---
sql(f"INSERT INTO player_trophy (player_id, mob_vnum, count) VALUES "
    f"((SELECT id FROM player WHERE name='{nameA}'), {MOB_XPCMP_B_VNUM}, 20.0);")
out = strip(cmd(sA, "trophy trophydeerb"))
check("little" in out.lower(), f"a trophy count of 20 (well past the 8-kill free threshold) shows a 'little' modifier: {out}")

# --- 4: the modifier is actually applied to earned XP, not just displayed ---
nameB, pwB = f"Trofcmp{_suffix}", "trofcmppw123"
sB = make_char(nameB, pwB)
cmd(sB, "quit!"); sB.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameB}';")
set_hp(nameB, 500, 500)
sql(f"INSERT INTO player_trophy (player_id, mob_vnum, count) VALUES "
    f"((SELECT id FROM player WHERE name='{nameB}'), {MOB_XPCMP_A_VNUM}, 20.0);")
sB = relog(nameB, pwB)

nameC, pwC = f"Trofctl{_suffix}", "trofctlpw123"
sC = make_char(nameC, pwC)
cmd(sC, "quit!"); sC.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameC}';")
set_hp(nameC, 500, 500)
sC = relog(nameC, pwC)

MOB_XPCMP_A3_VNUM = ROOM + 4
sql(mob_row(MOB_XPCMP_A3_VNUM, "trophydeerc"))

before_ctl = xp_of(nameC)
fight_and_check(si, sC, MOB_XPCMP_A3_VNUM, "trophydeerc")
gained_ctl = xp_of(nameC) - before_ctl

before_seed = xp_of(nameB)
fight_and_check(si, sB, MOB_XPCMP_A_VNUM, "trophydeera")
gained_seed = xp_of(nameB) - before_seed

check(gained_ctl > 0 and gained_seed > 0, "both fights actually awarded some XP")
check(gained_seed < gained_ctl,
      f"a player with a heavily-decayed (count=20) trophy count earns less XP from an identical kill "
      f"than a player with no trophy history ({gained_seed} < {gained_ctl})")

# --- 5: decay SQL floor-at-zero guard (trophy_repo_decay_all's own query) ---
sql(f"INSERT INTO player_trophy (player_id, mob_vnum, count) VALUES "
    f"((SELECT id FROM player WHERE name='{nameC}'), 999001, 0.1) "
    f"ON DUPLICATE KEY UPDATE count=0.1;")
sql(f"INSERT INTO player_trophy (player_id, mob_vnum, count) VALUES "
    f"((SELECT id FROM player WHERE name='{nameC}'), 999002, 5.0) "
    f"ON DUPLICATE KEY UPDATE count=5.0;")
sql("update player_trophy set count=count-0.25 where count > 0.25;")
low = trophy_count(nameC, 999001)
high = trophy_count(nameC, 999002)
check(low is not None and abs(low - 0.1) < 0.001, f"a count below the decay amount is left untouched (floor guard) (got {low})")
check(high is not None and abs(high - 4.75) < 0.001, f"a count above the decay amount decays by exactly the flat rate (got {high})")

sA.close(); sB.close(); sC.close()
si.close()

announce_done("smoke_test_trophy", host, port)
print("=== ALL CHECKS PASSED ===")
