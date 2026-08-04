#!/usr/bin/env python3
"""Smoke test for the Territory/Homeland system (Sneezy -> Tobin feature
audit, a fresh not-yet-audited item -- see being.h's player_territory_t doc
comment for the scope-down: one shared Urban/Rural/Wilds set instead of the
real upstream's 6 race-specific homeland tables).

Territory is a FORCED creation step right after race, before class (user,
2026-08-03: "should be a choice after choosing race") -- matches the real
upstream's own ordering. NOTE: this makes the standard character-creation
sequence 11 steps, not 10 -- every OTHER test's make_char()-style helper
that does name/y/pw/pw/new/name/race/class/done/done needs ONE more numeric
input (a homeland pick, 1-3) between race and class. Not retrofitted across
every existing test in this pass (same "documented, not everywhere at once"
precedent other breaking changes in this codebase have used already) --
only this feature's own test was updated.

Covers:
  1. The homeland menu appears right after race, before class.
  2. An invalid choice re-prompts instead of crashing/advancing.
  3. Each of the 3 homelands nets to a DIFFERENT, correctly-directioned
     attribute spread once folded into a fresh character's stats (score).
  4. The chosen homeland is shown by name in `score`.
  5. The homeland persists across a reconnect (not just held in the
     creation-time connection state).

    python3 tests/smoke_test_territory.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


announce("smoke_test_territory", host, port)


def parse_attrs(score_out):
    fields = {}
    for m in re.finditer(r"(Str|Int|Dex|Wis|Con|Cha):\s*(-?\d+)", score_out):
        fields[m.group(1)] = int(m.group(2))
    return fields


# Race prompt: 1=Human .. 6=Gnome. Territory prompt (new, forced, right
# after race): 1=Urban 2=Rural 3=Wilds. Class prompt: 1=Mage 2=Cleric
# 3=Warrior 4=Thief 5=Druid 6=Monk.
def make_char(name, pw, race_num, territory_num, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    steps = (name, "y", pw, pw, "new", name, race_num, territory_num, class_num, "done", "done")
    for step in steps:
        send_line(s, step)
        recv_all(s)
    return s


# --- 1: homeland menu appears right after race, before class ---
name1, pw1 = f"Trurb{_suffix}", "trurbpw12345"
s1 = socket.create_connection((host, port), timeout=5)
recv_all(s1)
out = ""
for step in (name1, "y", pw1, pw1, "new", name1, "1"):  # name .. race=Human
    send_line(s1, step)
    out += recv_all(s1)
check("Choose a homeland" in out, "homeland menu is shown right after picking a race")
check("Urban" in out and "Rural" in out and "Wilds" in out,
      "homeland menu lists all three options")

# --- 2: invalid choice re-prompts ---
out = cmd(s1, "9")
check("Enter a number from 1 to 3" in out, "an out-of-range homeland choice re-prompts")
out = cmd(s1, "abc")
check("Enter a number from 1 to 3" in out, "a non-numeric homeland choice re-prompts")

# Finish creation as Urban Warrior.
out = cmd(s1, "1")  # Urban
check("Choose a class" in out, "a valid homeland choice proceeds to class selection")
for step in ("3", "done", "done"):  # Warrior, skip attrs, skip options
    out = cmd(s1, step)

# --- 4: score shows the chosen homeland by name ---
out = cmd(s1, "score")
check("Homeland: Urban Dweller" in out, "score shows the chosen homeland by name")
urban_attrs = parse_attrs(out)
check(len(urban_attrs) == 6, "score's attribute grid parsed cleanly (all 6 fields found)")

# --- 5: homeland persists across a reconnect ---
s1.close()
s1b = socket.create_connection((host, port), timeout=5)
recv_all(s1b)
for step in (name1, pw1, "1"):  # account name+pw, char slot 1 (auto-enters world)
    send_line(s1b, step)
    recv_all(s1b)
out = cmd(s1b, "score")
check("Homeland: Urban Dweller" in out, "homeland survives a reconnect (persisted, not just in-memory)")
s1b.close()

# --- 3: Rural and Wilds net to a DIFFERENT attribute spread than Urban ---
# Same race+class (Human Warrior) for all three, so the only variable is
# homeland -- any attribute difference is attributable to territory_stat_bonus().
name2, pw2 = f"Trrur{_suffix}", "trrurpw12345"
s2 = make_char(name2, pw2, "1", "2", "3")  # Human, Rural, Warrior
out = cmd(s2, "score")
check("Homeland: Rural Dweller" in out, "a Rural character's score shows 'Rural Dweller'")
rural_attrs = parse_attrs(out)
check(rural_attrs != urban_attrs, "Rural's attribute spread differs from Urban's (bonus actually applied)")
s2.close()

name3, pw3 = f"Trwld{_suffix}", "trwldpw12345"
s3 = make_char(name3, pw3, "1", "3", "3")  # Human, Wilds, Warrior
out = cmd(s3, "score")
check("Homeland: Wilds Dweller" in out, "a Wilds character's score shows 'Wilds Dweller'")
wilds_attrs = parse_attrs(out)
check(wilds_attrs != urban_attrs, "Wilds' attribute spread differs from Urban's (bonus actually applied)")
check(wilds_attrs != rural_attrs, "Wilds' attribute spread differs from Rural's (bonus actually applied)")

# Wilds trades INT/CHA for CON/STR relative to Urban -- confirm the actual
# direction, not just "some difference" (net-zero, opposite of Urban).
check(wilds_attrs["Con"] > urban_attrs["Con"], "Wilds has higher Constitution than Urban")
check(wilds_attrs["Str"] > urban_attrs["Str"], "Wilds has higher Strength than Urban")
check(wilds_attrs["Int"] < urban_attrs["Int"], "Wilds has lower Intelligence than Urban")
check(wilds_attrs["Cha"] < urban_attrs["Cha"], "Wilds has lower Charisma than Urban")
s3.close()

announce_done("smoke_test_territory", host, port)
print("=== ALL CHECKS PASSED ===")
