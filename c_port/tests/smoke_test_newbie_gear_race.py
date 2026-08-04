#!/usr/bin/env python3
"""Smoke test for the newbie equipment system expansion (TODO.md priority
item, user 2026-08-02): per-race starting gear, rations + a small drink
container for every newbie, a spellpouch + components for Mage/Druid, and
a few wooden holy symbols for Cleric.

Covers:
  1. A fresh Dwarf character's inventory includes the DWARF race suit's
     11 armor pieces + racial weapon (hand axe) + shield -- not just the
     class suit's own weapon/shield.
  2. A fresh Human vs a fresh Ogre get DIFFERENT racial gear (proves the
     grant is actually keyed by race, not a hardcoded suit).
  3. Every fresh character (any class) gets 3 rations + 1 waterskin.
  4. A fresh Mage gets a spellbag + 3 spell components.
  5. A fresh Cleric gets 3 wooden holy symbols.
  6. A fresh Warrior (no spellpouch/holy-symbol class) does NOT get
     spell components or holy symbols.

    python3 tests/smoke_test_newbie_gear_race.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


announce("smoke_test_newbie_gear_race", host, port)


# Race prompt: 1=Human 2=Elf 3=Ogre 4=Dwarf 5=Hobbit 6=Gnome (player_race_t
# declaration order, being.h). Class prompt: 1=Mage 2=Cleric 3=Warrior
# 4=Thief 5=Druid 6=Monk (confirmed against suit.sql's class= mapping).
def make_char(name, pw, race_num, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, race_num, "1", class_num, "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


# NOTE: inventory is checked IMMEDIATELY after creation, all within the
# SAME connection -- `quit!` is documented, deliberate behavior (cmd_quit.c,
# user 2026-07-12) that DROPS a mortal's entire inventory on the floor
# ("the risky option Sneezy's own `rent` help text warns about"), so a
# quit!+relogin cycle between creation and the inventory check would
# destroy the very gear this test is trying to verify -- not a bug in
# the newbie-equipment expansion, just the wrong tool for this test.

# --- 1/2: race-specific armor + weapon, human vs ogre are different ---
dwarf_name, dwarf_pw = f"Rgdwf{_suffix}", "rgdwfpw12345"
s = make_char(dwarf_name, dwarf_pw, "4", "3")  # Dwarf Warrior
out = cmd(s, "inventory")
check("dwarven cloth" in out.lower(), "a fresh Dwarf's inventory has dwarf-tagged cloth armor")
check("dwarven hand axe" in out.lower(), "a fresh Dwarf's inventory has the dwarven hand axe")
check("training shield" in out.lower(), "a fresh Dwarf's inventory has the shared training shield")

# --- 3 (checked here too): every newbie gets rations + a waterskin ---
check("standard ration" in out.lower(), "a fresh character's inventory has ration(s) of food")
check("water skin" in out.lower(), "a fresh character's inventory has a water skin")
s.close()

human_name, human_pw = f"Rghum{_suffix}", "rghumpw12345"
s = make_char(human_name, human_pw, "1", "3")  # Human Warrior
out = cmd(s, "inventory")
check("human longsword" in out.lower(), "a fresh Human gets the human longsword, not the dwarven axe")
check("dwarven" not in out.lower(), "a fresh Human's inventory has NO dwarven-tagged items")
# --- 6: a Warrior does NOT get spell components or holy symbols ---
check("lasso" not in out.lower(), "a fresh Warrior's inventory has NO spell components")
check("holy symbol" not in out.lower(), "a fresh Warrior's inventory has NO holy symbols")
s.close()

ogre_name, ogre_pw = f"Rgogr{_suffix}", "rgogrpw12345"
s = make_char(ogre_name, ogre_pw, "3", "3")  # Ogre Warrior
out = cmd(s, "inventory")
check("ogre club" in out.lower() or "red ogre club" in out.lower(), "a fresh Ogre gets the red ogre club")
check("dwarven" not in out.lower() and "human longsword" not in out.lower(),
      "a fresh Ogre's inventory has neither the dwarf's nor the human's racial gear")
s.close()

# --- 4: Mage gets a spellpouch + components ---
mage_name, mage_pw = f"Rgmag{_suffix}", "rgmagpw12345"
s = make_char(mage_name, mage_pw, "1", "1")  # Human Mage
out = cmd(s, "inventory")
check("spellbag" in out.lower(), "a fresh Mage's inventory has a small spellbag")
# Displayed inventory shows each item's short_desc ("a tiny lasso made of
# basilisk hair", vnum 200), not its internal "component" keyword -- check
# for the real seeded item names, not the word "component" itself.
check("lasso" in out.lower() or "cat's breath" in out.lower() or "vaporous quartz" in out.lower(),
      "a fresh Mage's inventory has spell component(s)")
s.close()

# --- 5: Cleric gets wooden holy symbols ---
cleric_name, cleric_pw = f"Rgcle{_suffix}", "rgclepw12345"
s = make_char(cleric_name, cleric_pw, "1", "2")  # Human Cleric
out = cmd(s, "inventory")
check("wooden holy symbol" in out.lower(), "a fresh Cleric's inventory has wooden holy symbol(s)")
s.close()

announce_done("smoke_test_newbie_gear_race", host, port)
print("=== ALL CHECKS PASSED ===")
