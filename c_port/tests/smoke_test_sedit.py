#!/usr/bin/env python3
"""Smoke test for the new sedit shop editor (`sedit` / `edit shop`, 58+):
menu-driven, targeted by the immortal's own current room, editing shop
#104 ("Delayn's Hide and Herbal Shop", room 572, real seeded shop --
picked because it's a small, non-critical shop far from anything else
under test). Covers: level gate, opening the editor via a real
shoptype-42/43/50 shop room, editing+saving a scalar field
(profit_buy), and the accepted-item-types submenu (add + remove),
restoring the shop's real state afterward so the test is side-effect
free on live data.

    python3 tests/smoke_test_sedit.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done, drain

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_sedit", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
SHOP_ROOM = 572
SHOP_NR = 104


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


imm_name, imm_pw = f"Sedittest{_suffix}", "sedittestpw1"
low_name, low_pw = f"Seditlow{_suffix}", "seditlowpw12"

si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {SHOP_ROOM}")
drain(si)

lo = make_char(low_name, low_pw)
cmd(lo, "quit!"); lo.close()
sql(f"UPDATE player SET load_room={SHOP_ROOM} WHERE name='{low_name}';")
lo = relog(low_name, low_pw)
drain(lo)

# --- 1: below-level character is refused (both entry points) ---
out = cmd(lo, "sedit")
check("command not found" in out.lower(), "a level-1 character cannot reach the standalone sedit command")
out = cmd(lo, "edit shop")
check("command not found" in out.lower(), "a level-1 character cannot reach edit shop either")

# --- 2: the level-59 immortal opens the editor on the shop in this room ---
out = cmd(si, "sedit")
check(f"#{SHOP_NR}" in out and f"room {SHOP_ROOM}" in out,
      "sedit opens on the shop operating out of the immortal's current room")

# --- 3: editing and saving a scalar field round-trips through the DB ---
out = cmd(si, "1")
check("buy price multiplier" in out.lower(), "menu option 1 prompts for the buy price multiplier")
out = cmd(si, "3.75")
check("unsaved changes" in out.lower(), "changing a field marks the working copy dirty")
out = cmd(si, "s")
check("shop saved" in out.lower(), "Save commits the change")
out = cmd(si, "q")
out = cmd(si, "sedit")
check("3.75" in out, "the saved profit_buy value persists across a fresh sedit open")

# --- 4: accepted item types submenu -- add then remove, restoring original state ---
out = cmd(si, "t")
check("50" in out and "52" in out, "the shoptype submenu lists this shop's real accepted types (50, 52)")
out = cmd(si, "a")
check("enter a number to add" in out.lower(), "A) opens the add-a-type picker")
out = cmd(si, "8")
check("added" in out.lower() and "#8 " in out, "adding type 8 (TREASURE) shows up in the list")
out = cmd(si, "r")
out = cmd(si, "8")
check("removed" in out.lower() and "#8 " not in out, "removing type 8 takes it back out of the list")

# --- cleanup: restore shop 104's real profit_buy (1.15) and quit the editor cleanly ---
out = cmd(si, "q")
check("editing shop" in out.lower(), "leaving the type submenu returns to the main sedit menu")
cmd(si, "1")
cmd(si, "1.15")
out = cmd(si, "s")
check("shop saved" in out.lower(), "profit_buy restored to its real seeded value")
cmd(si, "q")

sql(f"DELETE FROM player WHERE name IN ('{imm_name}','{low_name}');")
sql(f"DELETE FROM player_progress WHERE player_id NOT IN (SELECT id FROM player);")

announce_done("smoke_test_sedit", host, port)
