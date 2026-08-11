#!/usr/bin/env python3
"""Smoke test for the cast/pray/put quality-of-life batch:

  1. Per-word spell-name abbreviation: "sorc globe" resolves to
     "sorcerer's globe" (spell_name_matches() -- each typed token must
     prefix the matching word, so multi-word names abbreviate word by
     word, which the old whole-string strncasecmp could not do).
  2. Immortal component bypass: an immortal casts a spell that has a
     bound reagent while carrying NO component at all -- no refusal
     (NOHASSLE; component_for_cast() returns the immortal sentinel).
  3. Immortal holy-symbol bypass: an immortal prays while carrying NO
     symbol -- no refusal (find_holy_symbol() returns the immortal
     sentinel).
  4. Bulk `put all.<name> <container>`: every matching loose item is
     moved into the container in one command (and same-vnum spell
     components merge on the way in).

    python3 tests/smoke_test_abbrev_bypass_putall.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_abbrev_bypass_putall", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
pw = "abpw123"


def make_char(name, char_class):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    for step in [name, "y", pw, pw, "new", name, "1", "1", char_class, "done", "done"]:
        send_line(s, step); recv_all(s, 0.7)
    s.close()


def login(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    send_line(s, name); recv_all(s, 0.7)
    send_line(s, pw); recv_all(s, 0.7)
    send_line(s, "1"); recv_all(s, 0.7)
    cmd(s, "color off")
    return s


imm = f"Abim{_sfx}"
make_char(imm, "1")  # Mage
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
s = login(imm)

# 1. per-word abbreviation resolves the multi-word spell name
out = cmd(s, "cast sorc globe")
check("don't know a spell" not in out.lower(),
      "'sorc globe' abbreviates to a real spell (per-word prefix match), not an unknown-spell error")

# 2. immortal casts a BOUND spell (faerie fire, reagent 227) with no component
out = cmd(s, "cast faerie fire")
check("need" not in out.lower() and "spell components" not in out.lower(),
      "an immortal casts a bound spell with no component at all (NOHASSLE bypass)")

# 3. immortal prays with no holy symbol
out = cmd(s, "pray armor")
check("holy symbol" not in out.lower(),
      "an immortal prays with no holy symbol (NOHASSLE bypass)")

# 4. `put all.<name> <container>` moves every match into the bag (and merges)
cmd(s, "load obj 321")   # a small spellbag (container)
cmd(s, "load obj 200")   # two identical components
cmd(s, "load obj 200")
out = cmd(s, "put all.lasso spellbag")
check("You put a tiny lasso" in out, "put all.<name> moves the matching items into the container")
inside = cmd(s, "look in spellbag")
check("basilisk hair" in inside.lower(),
      "the components are now inside the spellbag")
# both went in and merged -> the bag lists the lasso exactly once
check(inside.lower().count("basilisk hair") == 1,
      "the two put-away components merged into a single stack inside the bag")
s.close()

announce_done("smoke_test_abbrev_bypass_putall", host, port)
print("PASS: smoke_test_abbrev_bypass_putall")
