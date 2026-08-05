#!/usr/bin/env python3
"""Smoke test for class-appropriate mob casting supplies (user, Session 92:
"Mage/Druid/Cleric mobs should load carrying class-and-level-appropriate
spell components (Cleric mobs specifically: holy symbols of their
level)"; updated 2026-08-05 user request: Mage/Druid mobs now get a real
spellbag (321 small/322 medium/323 big, sized to the mob's own level)
loaded with a random real component, instead of the old flat generic
"pouch of spell components"). Covers being_grant_class_casting_supplies()
(being.c), called from being_create_mob() so it applies to every mob
spawn path (zone resets, `load mob`, etc.):
  1. A low-level Mage mob spawns carrying a small spellbag containing a
     "component"-keyword item.
  2. A low-level Druid mob spawns carrying a small spellbag containing a
     "component"-keyword item.
  3. A mid-level Mage mob gets a medium spellbag.
  4. A high-level Mage mob gets a large spellbag.
  5. A Cleric mob spawns carrying a "symbol"-keyword item (unchanged).
  6. A higher-level Cleric mob gets a higher (later-alphabetically-tiered)
     holy symbol material than a low-level one, per the wooden(vnum 500)
     -> mithril(vnum 514) ladder in the seed data.
  7. A Warrior mob (no cast/pray requirement at all) gets neither.

Verified by instakilling each mob (immortal `kill`) and looting its
corpse -- the simplest reliable way to inspect a mob's starting
inventory without a dedicated `stat mob <name>` inventory listing. The
spellbag is a nested container, so its contents need `get <bag> corpse`
+ `look in <bag>` rather than a flat `look corpse` substring check.

    python3 tests/smoke_test_mob_casting_supplies.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_mob_casting_supplies", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 70000)
ROOM = BASE


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
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "3"); recv_all(sock)  # class: warrior, so the immortal tester never carries its own starting spellbag to collide with a looted one
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
MOB_MAGE_MED = BASE + 6
MOB_MAGE_HIGH = BASE + 7

mage_name = f"castmage{_suffix}"
druid_name = f"castdruid{_suffix}"
cleric_low_name = f"castclericlow{_suffix}"
cleric_high_name = f"castclerichigh{_suffix}"
warrior_name = f"castwarrior{_suffix}"
mage_med_name = f"castmagemed{_suffix}"
mage_high_name = f"castmagehigh{_suffix}"

mob_insert(MOB_MAGE, mage_name, 1, 15)          # class 1 = Mage, level 15 -> small bag
mob_insert(MOB_DRUID, druid_name, 128, 15)      # class 128 = Druid (ranger lineage), level 15 -> small bag
mob_insert(MOB_CLERIC_LOW, cleric_low_name, 2, 1)     # class 2 = Cleric, level 1
mob_insert(MOB_CLERIC_HIGH, cleric_high_name, 2, 57)  # class 2 = Cleric, level 57
mob_insert(MOB_WARRIOR, warrior_name, 4, 20)    # class 4 = Warrior
mob_insert(MOB_MAGE_MED, mage_med_name, 1, 30)    # level 30 -> medium bag
mob_insert(MOB_MAGE_HIGH, mage_high_name, 1, 50)  # level 50 -> large bag


def load_kill_and_loot(mob_name):
    """Spawns a mob fixture, instakills it, and returns the text of `look
    corpse` -- the corpse is a lootable container holding whatever the
    mob was carrying/wearing at death (see smoke_test_corpse.py)."""
    check("You conjure" in cmd(s, f"load mob {mob_name}"), f"{mob_name} spawns")
    out = cmd(s, f"kill {mob_name}")
    check("slain" in out.lower(), f"the immortal instakills {mob_name}")
    corpse_out = cmd(s, "look corpse")
    return corpse_out


def loot_spellbag_contents(mob_name, corpse_out):
    """Given the just-looted corpse's `look corpse` text (still in the
    room), pulls the spellbag out and looks inside it."""
    check("spellbag" in corpse_out.lower(), f"{mob_name}'s corpse holds a spellbag")
    cmd(s, "get spellbag corpse")
    inside = cmd(s, "look in spellbag")
    return inside


def strip_color_tags(text):
    return re.sub(r"<[^<>]*>", "", text)


def component_short_descs():
    """The real seed-data pool pick_random_component_vnum() (being.c)
    draws from -- used to confirm the spellbag's contents is actually
    ONE of those real components, not just "something", and not a
    literal 'component' substring in player-facing flavor text (that's
    a keyword-list convention invisible in rendered output, see
    cmd_cast.c's find_keyword_item())."""
    raw = subprocess.run(
        ["mariadb", "tobin", "-N", "-e",
         "SELECT short_desc FROM obj WHERE name LIKE '%component mage%';"],
        check=True, capture_output=True, text=True).stdout
    return [strip_color_tags(line).strip().lower() for line in raw.splitlines() if line.strip()]


COMPONENT_DESCS = component_short_descs()


def holds_a_real_component(inside_text):
    text = inside_text.lower()
    return "nothing" not in text and any(desc in text for desc in COMPONENT_DESCS)


def cleanup():
    cmd(s, "purge")  # clears the corpse/spellbag/room before the next mob's fixture


# --- 1: Mage mob (low level) carries a small spellbag with a component ---
out = load_kill_and_loot(mage_name)
check("small spellbag" in out.lower(), "the level-15 Mage mob's corpse holds a SMALL spellbag")
inside = loot_spellbag_contents(mage_name, out)
check(holds_a_real_component(inside), "the Mage mob's spellbag holds one of the real seed-data components")
cleanup()

# --- 2: Druid mob (low level) carries a small spellbag with a component ---
out = load_kill_and_loot(druid_name)
check("small spellbag" in out.lower(), "the level-15 Druid mob's corpse holds a SMALL spellbag")
inside = loot_spellbag_contents(druid_name, out)
check(holds_a_real_component(inside), "the Druid mob's spellbag holds one of the real seed-data components")
cleanup()

# --- 3: mid-level Mage gets a medium spellbag ---
out = load_kill_and_loot(mage_med_name)
check("medium" in out.lower(), "the level-30 Mage mob's corpse holds a MEDIUM spellbag")
cleanup()

# --- 4: high-level Mage gets a large spellbag ---
out = load_kill_and_loot(mage_high_name)
check("large" in out.lower(), "the level-50 Mage mob's corpse holds a LARGE spellbag")
cleanup()

# --- 5: Cleric mob carries a symbol-keyword item (unchanged) ---
out_low = load_kill_and_loot(cleric_low_name)
check("symbol" in out_low.lower(), "the low-level Cleric mob's corpse holds a symbol-keyword item")
cleanup()

# --- 6: a higher-level Cleric gets a better symbol material ---
out_high = load_kill_and_loot(cleric_high_name)
check("symbol" in out_high.lower(), "the high-level Cleric mob's corpse holds a symbol-keyword item")
check("wooden" in out_low.lower(), "level 1 Cleric gets the lowest (wooden) symbol tier")
check("mithril" in out_high.lower(), "level 57 Cleric gets the highest (mithril) symbol tier")
cleanup()

# --- 7: Warrior mob (no cast/pray gate) gets neither ---
out = load_kill_and_loot(warrior_name)
check("component" not in out.lower() and "symbol" not in out.lower() and "spellbag" not in out.lower(),
      "the Warrior mob's corpse holds no casting supplies at all")
cleanup()

print("ALL CHECKS PASSED")
announce_done("smoke_test_mob_casting_supplies", host, port)
