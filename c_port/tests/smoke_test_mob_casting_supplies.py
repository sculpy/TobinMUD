#!/usr/bin/env python3
"""Smoke test for class-appropriate mob casting supplies (user, Session 92:
"Mage/Druid/Cleric mobs should load carrying class-and-level-appropriate
spell components (Cleric mobs specifically: holy symbols of their
level)"). Covers being_grant_class_casting_supplies() (being.c), called
from being_create_mob() so it applies to every mob spawn path (zone
resets, `load mob`, etc.):
  1. A Mage mob spawns carrying a "component"-keyword item.
  2. A Druid mob spawns carrying a "component"-keyword item.
  3. A Cleric mob spawns carrying a "symbol"-keyword item.
  4. A higher-level Cleric mob gets a higher (later-alphabetically-tiered)
     holy symbol material than a low-level one, per the wooden(vnum 500)
     -> mithril(vnum 514) ladder in the seed data.
  5. A Warrior mob (no cast/pray requirement at all) gets neither.

Verified by instakilling each mob (immortal `kill`) and looting its
corpse -- the simplest reliable way to inspect a mob's starting
inventory without a dedicated `stat mob <name>` inventory listing.

    python3 tests/smoke_test_mob_casting_supplies.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test. Self-contained (own socket, doesn't depend on this file's other
    helpers) so it can run at any point in the script."""
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


announce("smoke_test_mob_casting_supplies")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 70000)
ROOM = BASE


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


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Castsuptest{_suffix}"
imm_pw = "castsuppw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Casting Supplies Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Casting Supplies Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")


def mob_insert(vnum, name, mob_class, level):
    mob_columns_values = [
        ("vnum", str(vnum)),
        ("name", f"'{name}'"),
        ("short_desc", f"'a {name}'"),
        ("long_desc", f"'A {name} stands here.'"),
        ("description", f"'A {name} eyes you warily.'"),
        ("actions", "0"),
        ("affects", "0"),
        ("faction", "0"),
        ("fact_perc", "0"),
        ("letter", "'A'"),
        ("attacks", "1.0"),
        ("class", str(mob_class)),
        ("level", str(level)),
        ("tohit", "0"),
        ("ac", "0"),
        ("hpbonus", "0.3"),
        ("damage_level", "0"),
        ("damage_precision", "0"),
        ("gold", "0"),
        ("race", "0"),
        ("weight", "0"),
        ("height", "0"),
        ("str", "0"), ("bra", "0"), ("con", "0"), ("dex", "0"), ("agi", "0"),
        ("intel", "0"), ("wis", "0"), ("foc", "0"), ("per", "0"), ("cha", "0"),
        ("kar", "0"), ("spe", "0"),
        ("pos", "10"),
        ("def_position", "10"),
        ("sex", "1"),
        ("spec_proc", "0"),
        ("skin", "0"),
        ("vision", "0"),
        ("can_be_seen", "1"),
        ("max_exist", "1"),
    ]
    mob_cols = ",".join(c for c, _ in mob_columns_values)
    mob_vals = ",".join(v for _, v in mob_columns_values)
    sql(f"INSERT INTO mob ({mob_cols}) VALUES ({mob_vals});")


MOB_MAGE = BASE + 1
MOB_DRUID = BASE + 2
MOB_CLERIC_LOW = BASE + 3
MOB_CLERIC_HIGH = BASE + 4
MOB_WARRIOR = BASE + 5

mage_name = f"castmage{_suffix}"
druid_name = f"castdruid{_suffix}"
cleric_low_name = f"castclericlow{_suffix}"
cleric_high_name = f"castclerichigh{_suffix}"
warrior_name = f"castwarrior{_suffix}"

mob_insert(MOB_MAGE, mage_name, 1, 20)          # class 1 = Mage
mob_insert(MOB_DRUID, druid_name, 128, 20)      # class 128 = Druid (ranger lineage)
mob_insert(MOB_CLERIC_LOW, cleric_low_name, 2, 1)     # class 2 = Cleric, level 1
mob_insert(MOB_CLERIC_HIGH, cleric_high_name, 2, 57)  # class 2 = Cleric, level 57
mob_insert(MOB_WARRIOR, warrior_name, 4, 20)    # class 4 = Warrior


def load_kill_and_loot(mob_name):
    """Spawns a mob fixture, instakills it, and returns the text of `look
    in corpse` -- the corpse is a lootable container holding whatever the
    mob was carrying/wearing at death (see smoke_test_corpse.py)."""
    check("You conjure" in cmd(s, f"load mob {mob_name}"), f"{mob_name} spawns")
    out = cmd(s, f"kill {mob_name}")
    check("slain" in out.lower(), f"the immortal instakills {mob_name}")
    return cmd(s, "look in corpse")


# --- 1: Mage mob carries a component-keyword item ---
out = load_kill_and_loot(mage_name)
check("component" in out.lower(), "the Mage mob's corpse holds a component-keyword item")

# --- 2: Druid mob carries a component-keyword item ---
out = load_kill_and_loot(druid_name)
check("component" in out.lower(), "the Druid mob's corpse holds a component-keyword item")

# --- 3: Cleric mob carries a symbol-keyword item ---
out_low = load_kill_and_loot(cleric_low_name)
check("symbol" in out_low.lower(), "the low-level Cleric mob's corpse holds a symbol-keyword item")

# --- 4: a higher-level Cleric gets a better symbol material ---
out_high = load_kill_and_loot(cleric_high_name)
check("symbol" in out_high.lower(), "the high-level Cleric mob's corpse holds a symbol-keyword item")
check("wooden" in out_low.lower(), "level 1 Cleric gets the lowest (wooden) symbol tier")
check("mithril" in out_high.lower(), "level 57 Cleric gets the highest (mithril) symbol tier")

# --- 5: Warrior mob (no cast/pray gate) gets neither ---
out = load_kill_and_loot(warrior_name)
check("component" not in out.lower() and "symbol" not in out.lower(),
      "the Warrior mob's corpse holds no casting supplies at all")

print("ALL CHECKS PASSED")
announce_done("smoke_test_mob_casting_supplies")
