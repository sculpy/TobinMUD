#!/usr/bin/env python3
"""Smoke test for WEAR_PAIRED (user 2026-08-18): an item occupies BOTH members
of its paired slot -- both hands for a two-handed weapon, both legs for paired
armor. Covers occupancy, the both-free refusals, remove clearing both slots,
and persistence across quit!/relog (the paired partner is re-derived on load,
since inventory saves a single slot per object).

The two-handed specialization DAMAGE bonus (combat.c) is not asserted here --
it's a small proficiency-scaled damroll add on an already-random hit, verified
by code review, same precedent as the other weapon-specialization passives.

    python3 tests/smoke_test_wear_paired.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_sfx = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 946000 + (int(time.time()) % 30000)
TWOHAND, DAGGER, LEGGINGS, SINGLELEG = BASE, BASE + 1, BASE + 2, BASE + 3

WEAR_TAKE, WEAR_LEGS, WEAR_HOLD, WEAR_PAIRED = 1, 32, 16384, 512


def make_char(name, pw, cls):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step); recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=cls)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step); recv_all(s)
    cmd(s, "color off")
    return s


def mkobj(vnum, name, short, wear_flag, otype):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({vnum},'{name}','{short}','{short} is lying here.',{otype},{wear_flag},1);")


announce("smoke_test_wear_paired", host, port)

imm_name, imm_pw = f"Wpimm{_sfx}", "wppw123"
w_name, w_pw = f"Wpwar{_sfx}", "wppw123"

s = make_char(imm_name, imm_pw, "1"); cmd(s, "quit!"); s.close()
sql(f"UPDATE player_progress SET level=58, true_level=58 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s = make_char(w_name, w_pw, "3"); cmd(s, "quit!"); s.close()  # Warrior

sql(f"DELETE FROM obj WHERE vnum IN ({TWOHAND},{DAGGER},{LEGGINGS},{SINGLELEG});")
mkobj(TWOHAND,  f"greatsword paired {_sfx}", "a massive greatsword",
      WEAR_TAKE | WEAR_HOLD | WEAR_PAIRED, 5)   # two-handed weapon
mkobj(DAGGER,   f"dagger single {_sfx}", "a small dagger",
      WEAR_TAKE | WEAR_HOLD, 5)                 # one-handed weapon
mkobj(LEGGINGS, f"leggings paired {_sfx}", "a pair of iron leggings",
      WEAR_TAKE | WEAR_LEGS | WEAR_PAIRED, 9)   # both-legs armor
mkobj(SINGLELEG, f"greave single {_sfx}", "a single greave",
      WEAR_TAKE | WEAR_LEGS, 9)                 # one-leg armor

imm = relog(imm_name, imm_pw)
w = relog(w_name, w_pw)


def give(vnum, keyword):
    check("You conjure" in cmd(imm, f"load obj {vnum}"), f"imm loads {vnum}")
    cmd(imm, f"drop {keyword}")
    # bring the warrior to the immortal so the drop lands in reach
    cmd(imm, f"transfer {w_name}")
    check("you get" in cmd(w, f"get {keyword}").lower(), f"warrior picks up {keyword}")


# ---- two-handed weapon occupancy ----
give(TWOHAND, "greatsword")
out = cmd(w, "wield greatsword")
check("with both hands" in out.lower(), "wielding a two-handed weapon takes both hands")

give(DAGGER, "dagger")
out = cmd(w, "wield dagger")
check("hands are full" in out.lower(), "can't wield a second weapon -- both hands are occupied")

out = cmd(w, "remove greatsword")
check("remove" in out.lower(), "remove the two-handed weapon")
out = cmd(w, "wield dagger")
check("wield" in out.lower() and "full" not in out.lower(),
      "after removing the two-hander both hands are free again")

# one hand occupied by the dagger -> two-hander refused
out = cmd(w, "wield greatsword")
check("both hands free" in out.lower(), "a two-handed weapon needs BOTH hands free")

# ---- persistence across relog: the paired partner slot is re-derived on
# load (inventory saves ONE slot per object). Immortal `load obj` props are
# ephemeral and don't persist, so seed real player_inventory rows directly --
# this exercises the load path, which is the half that restores the pair.
# INV_SLOT_HELD_PRIMARY = -2, INV_SLOT_CARRIED = -1 (obj_repo.h).
cmd(w, "remove all")
cmd(w, "quit!"); w.close()
sql(f"DELETE FROM player_inventory WHERE player_id="
    f"(SELECT id FROM player WHERE name='{w_name}');")
sql(f"INSERT INTO player_inventory (player_id,vnum,slot,cur_struct,depreciation,monogram) "
    f"SELECT id,{TWOHAND},-2,0,0,'' FROM player WHERE name='{w_name}';")   # greatsword, HELD_PRIMARY
sql(f"INSERT INTO player_inventory (player_id,vnum,slot,cur_struct,depreciation,monogram) "
    f"SELECT id,{DAGGER},-1,0,0,'' FROM player WHERE name='{w_name}';")     # dagger, CARRIED
w = relog(w_name, w_pw)
out = cmd(w, "wield dagger")
check("hands are full" in out.lower(),
      "on load a WEAR_PAIRED weapon is restored into BOTH hands (paired state persists)")

# ---- paired armor occupancy ----
cmd(w, "remove greatsword")
give(LEGGINGS, "leggings")
out = cmd(w, "wear leggings")
check("you wear" in out.lower(), "wear paired leggings")
give(SINGLELEG, "greave")
out = cmd(w, "wear greave")
check("already wearing something there" in out.lower(),
      "paired leggings occupy BOTH legs -- no room for another leg item")
cmd(w, "remove leggings")
out = cmd(w, "wear greave")
check("you wear" in out.lower(), "one greave on one leg after removing the leggings")
out = cmd(w, "wear leggings")
check("both of those free" in out.lower(),
      "paired leggings refused when one leg is already occupied")

print("=== ALL WEAR_PAIRED CHECKS PASSED ===")

imm.close(); w.close()
sql(f"DELETE FROM obj WHERE vnum IN ({TWOHAND},{DAGGER},{LEGGINGS},{SINGLELEG});")
announce_done("smoke_test_wear_paired", host, port)
