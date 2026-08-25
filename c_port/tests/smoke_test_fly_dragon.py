#!/usr/bin/env python3
"""Smoke test for the new `fly` command (dragon ride system, user
2026-08-25: "implement a dragon ride system to take players from one
area to another distant area for a fee"). Covers:

  1. `fly` with no args, standing at the Market Square roost (room 7900),
     lists the Araxus route and its fee.
  2. `fly araxus` with enough gold succeeds: gold is deducted, the
     traveler's own room becomes the Araxus roost (room 7901).
  3. `fly araxus` with too little gold is refused: no gold deducted, the
     traveler is still standing in the Market Square roost.
  4. `fly` from an ordinary room (not a roost) is refused outright.

    python3 tests/smoke_test_fly_dragon.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_fly_dragon", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))

ROOST_MARKET = 7900
ROOST_ARAXUS = 7901
FEE = 1500


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
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


# --- 1 & 2: list routes, then a successful flight with gold deduction ---
rich_name, rich_pw = f"Flyrich{_suffix}", "flyrichpw123"
s = make_char(rich_name, rich_pw)
cmd(s, "quit!")
s.close()
sql(f"UPDATE player SET load_room={ROOST_MARKET} WHERE name='{rich_name}';")
sql(f"UPDATE player_progress SET gold=2000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{rich_name}');")
s = relog(rich_name, rich_pw)

out = cmd(s, "fly")
check("Araxus" in out and str(FEE) in out, "bare fly lists the Araxus route and its fee")

out = cmd(s, "fly araxus", timeout=2.0)
check("dragon" in out.lower(), "fly araxus reads as riding a dragon, not a generic teleport")
check("Araxus Walls" in out, "fly araxus lands the traveler at the Araxus roost")

# 2000 gold minus the 1500 fee leaves 500 -- not enough for the 1500
# return trip, which confirms the fee was actually deducted (rather
# than, say, silently failing to charge at all).
out = cmd(s, "fly")
check("Tobin City" in out, "the Araxus roost offers a route back to Tobin City")
out = cmd(s, "fly tobin", timeout=2.0)
check(f"{FEE} gold" in out, "only 500 gold remains after the first flight -- the return trip is correctly refused")
s.close()

# --- 3: insufficient gold is refused, no state change ---
poor_name, poor_pw = f"Flypoor{_suffix}", "flypoorpw123"
s = make_char(poor_name, poor_pw)
cmd(s, "quit!")
s.close()
sql(f"UPDATE player SET load_room={ROOST_MARKET} WHERE name='{poor_name}';")
sql(f"UPDATE player_progress SET gold=100 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{poor_name}');")
s = relog(poor_name, poor_pw)

out = cmd(s, "fly araxus", timeout=2.0)
check(f"{FEE} gold" in out, "fly araxus without enough gold reports the real fee")
check("Market Square" in cmd(s, "look"), "the too-poor traveler is still at the Market Square roost")
s.close()

# --- 4: fly from an ordinary (non-roost) room is refused outright ---
grounded_name, grounded_pw = f"Flygrnd{_suffix}", "flygrndpw123"
s = make_char(grounded_name, grounded_pw)
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET gold=5000 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{grounded_name}');")
s = relog(grounded_name, grounded_pw)

out = cmd(s, "fly")
check("nothing to fly" in out.lower(), "fly from an ordinary room is refused outright")
s.close()

announce_done("smoke_test_fly_dragon", host, port)
print("=== ALL CHECKS PASSED ===")
