#!/usr/bin/env python3
"""Smoke test for passive HP regeneration: a resting (not fighting)
character slowly heals over time. Hand-sets a fresh character's HP down via
direct SQL (same pattern as smoke_test_level_titles.py's set_level()) and
reconnects to force a DB reload -- deliberately NOT going through combat to
produce the damage, since combat_defeat() now ejects the loser to the
account menu (see STATUS.md) rather than leaving them in-world to rest, so
chaining off a fight is no longer a reliable way to get a "damaged but
still playing" character.

    python3 tests/smoke_test_regen.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_regen", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_player(tag):
    name = f"Regen{tag}{_suffix}"
    pw = "regentestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "y")
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, pw)  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done")
    recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s, name


def set_hp(name, hp):
    subprocess.run(
        ["mariadb", "tobin", "-e",
         f"UPDATE player_progress SET hp={hp} WHERE player_id=(SELECT id FROM player WHERE name='{name}');"],
        check=True,
    )


def read_hp(out):
    out = re.sub(r'\x1b\[[0-9;]*m', '', out)
    m = re.search(r"HP:\s+(\d+)/(\d+)", out)
    check(m is not None, "score output includes a parseable HP line")
    return int(m.group(1)), int(m.group(2))


s, name = make_player("A")
send_line(s, "score")
out = recv_all(s)
_, max_hp = read_hp(out)

# Hand-damage this character down to a low HP via direct SQL, then
# reconnect so the running server picks up the new value from the DB.
low_hp = max(1, max_hp // 4)
set_hp(name, low_hp)

s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name)
recv_all(s)
send_line(s, "regentestpw123")
recv_all(s)
send_line(s, "1")
recv_all(s)

send_line(s, "score")
out = recv_all(s)
hp_before, _ = read_hp(out)
# NOTE: not a strict equality check against low_hp. Every recv_all() call
# in this test suite effectively costs ~1s (it blocks until a full
# `timeout` gap with no data, not just until the reply arrives), and this
# point in the script is already ~10 such calls deep -- comfortably enough
# elapsed wall-clock time for a regen tick (REGEN_PULSES=50, ~5s) to have
# already nudged HP up once or twice before this check even runs. A loose
# range still proves the hand-set low value was actually picked up from
# the DB (not silently ignored/overwritten to something unrelated).
check(low_hp <= hp_before < max_hp,
      "reconnecting picked up the hand-set low HP from the DB (allowing for regen ticks "
      "that may have already fired during this script's own round-trip overhead)")
print(f"HP right after reconnecting: {hp_before}/{max_hp} (hand-set to {low_hp})")

# Not fighting anyone -- wait a few regen ticks (REGEN_PULSES=50, ~5s each)
# and confirm HP went up on its own.
time.sleep(11.0)
send_line(s, "score")
out = recv_all(s)
hp_after, _ = read_hp(out)
print(f"HP after ~11s of resting: {hp_after}/{max_hp}")

check(hp_after > hp_before, "HP increased while resting (not fighting) -- passive regen is working")
check(hp_after <= max_hp, "regen never overheals past max HP")

s.close()
announce_done("smoke_test_regen", host, port)
print("=== ALL CHECKS PASSED ===")
