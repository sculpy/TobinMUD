#!/usr/bin/env python3
"""Smoke test for `materialize` (spell/skill functional-completeness
audit continued, Mage level 6). See cmd_materialize.c's own header
comment for the real-upstream research and scope-down rationale.

    python3 tests/smoke_test_materialize.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_materialize", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_MAGE = 0


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def gold_from_score(out):
    m = re.search(r"Gold:\s+(-?\d+)", out)
    return int(m.group(1)) if m else None


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_single(prefix):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    sql(f"UPDATE player SET class={CLASS_MAGE} WHERE name='{name}';")
    sql(f"UPDATE player_progress SET level=10, basic_disc_pct=100, combat_disc_pct=100, "
        f"gold=1000 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    s = relog(name, pw)
    return name, s


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


CHEAP_VNUM = 977000 + (int(time.time()) % 1000)
PRICEY_VNUM = CHEAP_VNUM + 1
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,val2,val3,"
    f"weight,volume,price,can_be_seen,max_struct,cur_struct,material,decay,max_exist) VALUES "
    f"({CHEAP_VNUM},'pebble quartzcheapite trinket','a quartzcheapite pebble',"
    f"'A quartzcheapite pebble sits here.',0,0,0,0,0,0,1,1,10,1,0,0,0,-1,20),"
    f"({PRICEY_VNUM},'gem quartzpricey expensive','a huge quartzpricey gem',"
    f"'A huge quartzpricey gem sits here.',0,0,0,0,0,0,1,1,500,1,0,0,0,-1,20)")
# obj_find_vnum_by_name() is a literal substring match, not a per-keyword
# one (thing_name_matches()'s own convention) -- these two distinct,
# non-overlapping single-word substrings avoid any risk of one search
# term accidentally matching both rows or the wrong one.

sockets = []
try:
    # --- 1: too-short name is refused, no gold spent ---
    nameA, sA = make_single("Mat")
    sockets.append(sA)
    seed_proficiency(nameA, "materialize", 100)
    recv_all(sA)
    before = gold_from_score(cmd(sA, "score"))
    out = strip(cmd(sA, "materialize xy"))
    check("more specific" in out.lower(), "materialize refuses a too-short name")
    after = gold_from_score(cmd(sA, "score"))
    check(before == after, "a refused short name doesn't spend gold")

    # --- 2: no matching cheap-enough item -- gold IS spent, real upstream quirk ---
    out2 = strip(cmd(sA, "materialize zzznomatch"))
    check("cannot be created" in out2.lower(), "materialize refuses when nothing matches")
    after2 = gold_from_score(cmd(sA, "score"))
    check(after2 == after - 100, "gold is still spent on a failed materialize (real upstream's own gamble)")

    # --- 3: an item too expensive to conjure is refused the same way ---
    out3 = strip(cmd(sA, "materialize quartzpricey"))
    check("cannot be created" in out3.lower(), "materialize refuses an item priced above the cap")

    # --- 4: 100%-proficiency materialize conjures the cheap match ---
    before4 = gold_from_score(cmd(sA, "score"))
    out4 = strip(cmd(sA, "materialize quartzcheapite"))
    check("flash of light" in out4.lower(), "a successful materialize conjures the item")
    inv = strip(cmd(sA, "inventory"))
    check("pebble" in inv.lower(), "the conjured item is actually in inventory")
    after4 = gold_from_score(cmd(sA, "score"))
    check(after4 == before4 - 100, "a successful materialize spends the same flat price")
    sA.close()

    # --- 5: can't afford it at all ---
    nameB, sB = make_single("Matp")
    sockets.append(sB)
    seed_proficiency(nameB, "materialize", 100)
    # A live session already has last-loaded gold in memory -- a plain
    # SQL UPDATE after login never reaches it (same "SQL-then-relog,
    # never SQL-then-quit!" trap smoke_test_skillcombat3.py's own
    # make_pair() docstring already names). quit!-then-SQL-then-relog
    # forces a fresh load instead.
    cmd(sB, "quit!"); sB.close()
    sql(f"UPDATE player_progress SET gold=10 WHERE player_id=(SELECT id FROM player WHERE name='{nameB}');")
    sB = relog(nameB, f"Matppw12345")
    sockets[-1] = sB
    recv_all(sB)
    out5 = strip(cmd(sB, "materialize quartzcheapite"))
    check("don't have the money" in out5.lower(), "materialize refuses without enough gold")
    sB.close()

    sockets = []
    announce_done("smoke_test_materialize", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Mat", "Matp"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_inventory WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM obj WHERE vnum IN ({CHEAP_VNUM}, {PRICEY_VNUM});")
