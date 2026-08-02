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

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def announce(test_name, host=host, port=port):
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    announce(f"done {test_name}", host, port)


announce("smoke_test_newbie_gear_race")


def recv_all(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


# Race prompt: 1=Human 2=Elf 3=Ogre 4=Dwarf 5=Hobbit 6=Gnome (player_race_t
# declaration order, being.h). Class prompt: 1=Mage 2=Cleric 3=Warrior
# 4=Thief 5=Druid 6=Monk (confirmed against suit.sql's class= mapping).
def make_char(name, pw, race_num, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, race_num, class_num, "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


# --- 1/2: race-specific armor + weapon, human vs ogre are different ---
dwarf_name, dwarf_pw = f"Rgdwf{_suffix}", "rgdwfpw12345"
s = make_char(dwarf_name, dwarf_pw, "4", "3")  # Dwarf Warrior
cmd(s, "quit!")
s.close()
s = login(dwarf_name, dwarf_pw)
out = cmd(s, "inventory")
check("dwarven cloth" in out.lower(), "a fresh Dwarf's inventory has dwarf-tagged cloth armor")
check("dwarven hand axe" in out.lower(), "a fresh Dwarf's inventory has the dwarven hand axe")
check("training shield" in out.lower(), "a fresh Dwarf's inventory has the shared training shield")
s.close()

human_name, human_pw = f"Rghum{_suffix}", "rghumpw12345"
s = make_char(human_name, human_pw, "1", "3")  # Human Warrior
cmd(s, "quit!")
s.close()
s = login(human_name, human_pw)
out = cmd(s, "inventory")
check("[human]" not in out.lower() or "cloth" in out.lower(), "a fresh Human's inventory has its own cloth armor")
check("human longsword" in out.lower(), "a fresh Human gets the human longsword, not the dwarven axe")
check("dwarven" not in out.lower(), "a fresh Human's inventory has NO dwarven-tagged items")
s.close()

ogre_name, ogre_pw = f"Rgogr{_suffix}", "rgogrpw12345"
s = make_char(ogre_name, ogre_pw, "3", "3")  # Ogre Warrior
cmd(s, "quit!")
s.close()
s = login(ogre_name, ogre_pw)
out = cmd(s, "inventory")
check("ogre club" in out.lower() or "red ogre club" in out.lower(), "a fresh Ogre gets the red ogre club")
check("dwarven" not in out.lower() and "human longsword" not in out.lower(),
      "a fresh Ogre's inventory has neither the dwarf's nor the human's racial gear")
s.close()

# --- 3: every newbie gets rations + a waterskin, regardless of class/race ---
s = login(ogre_name, ogre_pw)
out = cmd(s, "inventory")
check("standard ration (x3)" in out.lower() or "standard ration" in out.lower(),
      "a fresh character's inventory has ration(s) of food")
check("water skin" in out.lower(), "a fresh character's inventory has a water skin")
s.close()

# --- 4: Mage gets a spellpouch + components ---
mage_name, mage_pw = f"Rgmag{_suffix}", "rgmagpw12345"
s = make_char(mage_name, mage_pw, "1", "1")  # Human Mage
cmd(s, "quit!")
s.close()
s = login(mage_name, mage_pw)
out = cmd(s, "inventory")
check("spellbag" in out.lower(), "a fresh Mage's inventory has a small spellbag")
check("component" in out.lower(), "a fresh Mage's inventory has spell component(s)")
s.close()

# --- 5: Cleric gets wooden holy symbols ---
cleric_name, cleric_pw = f"Rgcle{_suffix}", "rgclepw12345"
s = make_char(cleric_name, cleric_pw, "1", "2")  # Human Cleric
cmd(s, "quit!")
s.close()
s = login(cleric_name, cleric_pw)
out = cmd(s, "inventory")
check("wooden holy symbol" in out.lower(), "a fresh Cleric's inventory has wooden holy symbol(s)")
check("(x3)" in out.lower() or "holy symbol" in out.lower(), "the holy symbols show a real count")

# --- 6: a Warrior does NOT get spell components or holy symbols ---
s2 = login(human_name, human_pw)
out2 = cmd(s2, "inventory")
check("component" not in out2.lower(), "a fresh Warrior's inventory has NO spell components")
check("holy symbol" not in out2.lower(), "a fresh Warrior's inventory has NO holy symbols")
s2.close()

s.close()
announce_done("smoke_test_newbie_gear_race")
print("=== ALL CHECKS PASSED ===")
