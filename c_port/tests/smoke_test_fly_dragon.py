#!/usr/bin/env python3
"""Smoke test for the `fly` command (dragon ride system, user 2026-08-25:
"implement a dragon ride system to take players from one area to another
distant area for a fee"; follow-up same day: "there must be a series of
rooms to go through for flavor, a flight should take between 10-15
seconds to complete"). Covers:

  1. `fly` with no args, standing at the Market Square roost (room 7900),
     lists the Araxus route and its fee.
  2. `fly araxus` with enough gold charges the fee immediately, but does
     NOT land the traveler right away -- the flight is a multi-tick
     background task (fly.c) landing in a real 12-15s window: every
     other command is locked out mid-air ("You are still recovering!",
     the same wait-state gate spellcast/combat use) until the flight
     actually lands, so this waits out the real trip rather than forcing
     it with the immortal-only `aitick` (that only fast-forwards which
     sky waypoint a flier is in, not the wait-lockout itself, which is
     real-time-driven and would leave every command refused as "still
     recovering" for the rest of the trip regardless).
  3. `fly araxus` with too little gold is refused outright: no gold
     deducted, no flight started, the traveler is still standing (and
     free to act) in the Market Square roost.
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
FLY_LEG_COUNT = 5
FLY_TICK_SECONDS = 3  # ~30 pulses/leg at 100ms/pulse -- see fly.c's FLY_TICK_PULSES
# Landing floats in a [(FLY_LEG_COUNT-1)*3s, FLY_LEG_COUNT*3s) real-time
# window (fly.c's own doc comment on the absolute-pulse-clock alignment
# jitter) -- wait past the far end, plus slack for scheduling jitter.
FLY_WAIT_SECONDS = FLY_LEG_COUNT * FLY_TICK_SECONDS + 3


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


# --- 1 & 2: list routes, then a successful multi-tick flight ---
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
check("Araxus Walls" not in out, "fly araxus does not land the traveler immediately -- the flight takes time")

out = cmd(s, "look")
check("recovering" in out.lower(), "every other command is locked out while airborne, same as a spellcast/combat wait-state")

time.sleep(FLY_WAIT_SECONDS)

# Confirmed via `fly`'s own route listing rather than `look` -- the
# roosts are outdoor rooms with no permanent light, so `look` here is
# at the mercy of the in-game day/night cycle (a real, unrelated
# mechanic: an unlit outdoor room at night reads as pitch black to a
# character with no light source, same as any other dark room). `fly`'s
# listing is unconditional text, not gated by visibility, and doubles as
# proof the wait-lockout has actually cleared (a still-airborne
# character would get "You are still recovering!" instead).
out = cmd(s, "fly")
check("Tobin City" in out, "after the real flight time has elapsed, the traveler has landed at the Araxus roost and can see the route back")
check("recovering" not in out.lower(), "the wait-lockout clears once the flight actually lands")

# 2000 gold minus the 1500 fee leaves 500 -- not enough for the 1500
# return trip, which confirms the fee was actually deducted (rather
# than, say, silently failing to charge at all).
out = cmd(s, "fly tobin", timeout=2.0)
check(f"{FEE} gold" in out, "only 500 gold remains after the first flight -- the return trip is correctly refused")
s.close()

# --- 3: insufficient gold is refused, no state change, no flight started ---
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
out = cmd(s, "fly")
check("Araxus" in out, "the too-poor traveler is still at the Market Square roost (its route listing still offers Araxus)")
check("recovering" not in out.lower(), "a refused fly never locks out other commands -- no flight was started")
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
