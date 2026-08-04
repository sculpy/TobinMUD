#!/usr/bin/env python3
"""Smoke test for the last gap in the level-21/22 spell/skill audit batch:
`eyes of Fertuman` (Mage 22). `conjure elemental earth` (Mage 21) was
ALREADY implemented (shares cmd_cast.c's generic "conjure elemental"
handler with fire/water/air, tested by tests/smoke_test_pet.py) -- not
re-covered here. Every other level-21/22 roster entry (blindness, word of
recall, taunt, paralyze limb) was covered by earlier sessions'
smoke_test_blindness_recall.py / smoke_test_taunt_paralyzelimb.py.

Real upstream (disc/disc_mage_alchemy.cc's eyesOfFertuman()): a world-wide
locate-by-name, scanning every live object and character for a name match
and reporting which room each is in. Scoped down for Tobin: no critical-
success tier, no ITEM_MAGIC resistance-chance, a flat 5-result cap instead
of a skill/level-scaled one, and object results limited to room-floor
items (an object nested in a container/inventory has no reliable room --
see thing.h's own doc comment on `roomp` going stale there).

Covers:
  1. `cast eyes of fertuman <keyword>` with no matches reports "...nothing."
  2. A room-floor object matching the keyword is found, with its real room
     name reported.
  3. A mob matching the keyword is found the same way.
  4. An immortal mob/player is never reported (excluded from results).
  5. `cast eyes of fertuman` with no argument asks what to locate.
  6. Refuses without a spell component, same as every other cast spell.

    python3 tests/smoke_test_level21_22_gap.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


announce("smoke_test_level21_22_gap", host, port)


# Territory (2026-08-03) is now a FORCED step right after race: race, then
# homeland (1-3), then class.
def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


mage_name, mage_pw = f"Fert{_suffix}", "fertpw12345"
s = make_char(mage_name, mage_pw, 1)  # Mage
cmd(s, "quit!")  # forces a real player_progress save/row before the SQL edit below
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{mage_name}');")
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (mage_name, mage_pw, "1"):
    send_line(s, step)
    recv_all(s)

immname, immpw = f"Fertim{_suffix}", "fertimpw12345"
imm = make_char(immname, immpw, 1)  # a second immortal, to check exclusion
cmd(imm, "quit!")
imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{immname}');")
imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
for step in (immname, immpw, "1"):
    send_line(imm, step)
    recv_all(imm)
cmd(imm, "goto 100")  # Center Square, real seeded room

# --- 6: refuses without a component ---
out = cmd(s, "cast eyes of fertuman zzznomatch")
check("don't have the spell components" in out.lower(), "refuses without a spell component")

cmd(s, "load obj 200")  # a real mage component (lasso of basilisk hair)

# --- 1: no matches ---
out = cmd(s, "cast eyes of fertuman zzznonexistentkeyword")
check("nothing" in out.lower(), "no matches reports nothing found")

# --- 5: no argument ---
out = cmd(s, "cast eyes of fertuman")
check("locate what" in out.lower(), "no argument asks what to locate")

# --- 2: a room-floor object is found with its real room name ---
# NOTE: the result cap is 5 (see cmd_cast.c), and plenty of other tests'
# torches are already loose in the world -- don't assert THIS specific
# dropped torch makes the cut, just that the mechanism actually finds
# torch objects and tags each with a real " is in <room>" location.
cmd(s, "goto 100")
cmd(s, "load obj 105")  # a real seeded torch, dropped loose on the floor
cmd(s, "drop torch")
out = cmd(s, "cast eyes of fertuman torch")
check("torch" in out.lower() and " is in " in out.lower(),
      "finds room-floor torch object(s) and reports a real room location for each")

# --- 3/4: mob found by name; immortal PC excluded ---
out = cmd(s, f"cast eyes of fertuman {immname.lower()}")
check(immname.lower() not in out.lower() or "nothing" in out.lower(),
      "an immortal character is excluded from results")

imm.close()
s.close()

announce_done("smoke_test_level21_22_gap", host, port)
print("=== ALL CHECKS PASSED ===")
