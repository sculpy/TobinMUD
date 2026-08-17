#!/usr/bin/env python3
"""Verifies the 2026-08-17 spell-component binding fix: 21 material spells
that were silently falling back to the GENERIC "any component" cast gate now
demand their SPECIFIC Sneezy reagent. Two failure modes were fixed:

  * COMP_FILEMAP name diverged from skill.c (strcasecmp missed) -- e.g.
    "sorcerers globe" vs skill.c's "sorcerer's globe".  Representative here:
    sorcerer's globe (Mage L1), reagent filenum 6.
  * COMP_FILEMAP was missing the filenum entirely though the reagent object
    was seeded -- the six Druid skills.  Representative here: barkskin
    (Druid L3), reagent "a dollop of petrified syrup" (filenum 283).

A correctly-bound spell cast with NO reagent names the exact reagent ("You
need <X> to cast that."); a spell still on the generic gate instead says the
generic "You don't have the spell components to cast that." -- so asserting
the reagent's own name appears is a direct proof the binding took.

    python3 tests/smoke_test_component_binding_fix.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_component_binding_fix", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))


def make_char(name, pw, char_class):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)   # race: human
    send_line(s, "1"); recv_all(s)   # territory: urban
    send_line(s, char_class); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "compfixpw123"

# --- Case 1: name-mismatch fix. Mage L1, sorcerer's globe. ---
mage = f"Cfixmg{_sfx}"
make_char(mage, pw, "1")
sql("UPDATE player_progress SET level=10, basic_disc_pct=100, mana=100, max_mana=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{mage}');")
sm = login(mage, pw)
out = cmd(sm, "cast sorcerer's globe").lower()
check("don't have the spell components" not in out,
      "sorcerer's globe is NOT on the generic component gate anymore")
check("daemon" in out or "black ink" in out or "need" in out,
      "sorcerer's globe names its specific reagent when cast without one")
sm.close()

# --- Case 2: filemap-add fix. Druid L3, barkskin. ---
dru = f"Cfixdr{_sfx}"
make_char(dru, pw, "5")  # class 5 = Druid (CLASS_MAGE=0..CLASS_DRUID=4, 1-based menu)
sql("UPDATE player_progress SET level=10, basic_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{dru}');")
sd = login(dru, pw)
out = cmd(sd, "cast barkskin").lower()
check("don't have the spell components" not in out,
      "barkskin is NOT on the generic component gate anymore")
check("petrified syrup" in out or "need" in out,
      "barkskin names its specific reagent (a dollop of petrified syrup)")
sd.close()

announce_done("smoke_test_component_binding_fix", host, port)
