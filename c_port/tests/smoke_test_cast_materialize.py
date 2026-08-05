#!/usr/bin/env python3
"""Smoke test for `cast materialize <item>` (Mage, level 6) -- confirms
the `cast` form now routes to the real, pre-existing standalone
`materialize` command (cmd_materialize.c) instead of falling through to
the generic "nothing happens yet" stub-audit placeholder.

    python3 tests/smoke_test_cast_materialize.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
CHEAP = 942400 + (int(time.time()) % 30000)


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=class_choice)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_cast_materialize", host, port)

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,price,can_be_seen) "
    f"VALUES ({CHEAP},'trinket materializeme cheap','a cheap trinket',"
    f"'A cheap trinket is lying here.',12,1,10,1);")

mage_name, mage_pw = f"Scmmag{_suffix}", "scmmagpw123"
sm = make_char(mage_name, mage_pw, "1")
cmd(sm, "quit!")
sm.close()
sql(f"UPDATE player_progress SET level=51,gold=500 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{mage_name}');")
sm = relog(mage_name, mage_pw)

# Too-short name and a non-matching name both route through and refuse
# for the SAME real reasons the standalone command does -- not a no-op.
out1 = cmd(sm, "cast materialize xy")
check("more specific" in out1.lower(), "cast materialize refuses a too-short name, same as the standalone command")
out2 = cmd(sm, "cast materialize zzznomatch")
check("cannot be created" in out2.lower(), "cast materialize refuses a non-matching name for real, not a no-op")
check("nothing happens yet" not in out2.lower(), "cast materialize doesn't fall through to the generic stub placeholder")

# A real cheap-enough prototype match actually conjures the item.
gold_before = cmd(sm, "score")
out3 = cmd(sm, "cast materialize materializeme")
check("flash of light" in out3.lower(), "cast materialize conjures a real item on a genuine match")
inv = cmd(sm, "inventory").lower()
check("trinket" in inv, "the materialized item is really in the mage's inventory")

sm.close()

sql(f"DELETE FROM obj WHERE vnum={CHEAP};")

announce_done("smoke_test_cast_materialize", host, port)
print("=== ALL CHECKS PASSED ===")
