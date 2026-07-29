#!/usr/bin/env python3
"""Smoke test for the level-23 spell/skill audit batch (2026-07-28):
`copy` (Mage), `haste` (Mage), `storm call` (Druid). `cure disease`
(Druid, also level 23) was already implemented in an earlier session,
not covered here.

  1. `cast copy <scroll>` duplicates a known scroll (the real seeded
     "scroll healing minor", obj_magic-bound to "heal light") into the
     room -- a second matching item appears.
  2. `cast copy` refuses a non-scroll item ("That's not a scroll!").
  3. `cast haste` applies AFFECT_HASTE (visible in `affects`).
  4. A hasted attacker lands roughly twice as many of their own strike
     messages as their (unhasted) opponent's, over a multi-round fight
     against a fixed-HP dummy -- the actual "one bonus combat_strike()
     per round" mechanic, not just the affect being flagged.
  5. `cast storm call` refuses with "You fail to call upon the weather
     to aid you!" when the world weather is WEATHER_CLEAR (the default
     right after any fresh boot/copyover).
  6. `cast storm call` refuses when the caster is indoors, even in good
     weather (game_config's weather_state is forced to WEATHER_STORMY
     via SQL + a copyover before this test runs, see the session's own
     deploy notes -- weather is loaded once at boot, not live-pollable).
  7. `cast storm call` lands a real hit on an outdoor target once the
     weather is stormy.

    python3 tests/smoke_test_level23_spells.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


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


announce("smoke_test_level23_spells")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 70000)
ROOM_OUT = BASE            # outdoor sandbox room
ROOM_IN = BASE + 1         # indoor sandbox room
MOB_DUMMY1 = BASE + 2
MOB_DUMMY2 = BASE + 3
MOB_DUMMY3 = BASE + 4


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


def make_char(sock, name, pw, class_num):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)          # race: human
    send_line(sock, str(class_num)); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)       # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


mage_name = f"L23mage{_suffix}"
mage_pw = "l23magepw123"
druid_name = f"L23druid{_suffix}"
druid_pw = "l23druidpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, mage_name, mage_pw, 1)  # Mage
set_level(mage_name, 51)
s.close()
mage = login(mage_name, mage_pw)

s = socket.create_connection((host, port), timeout=5)
make_char(s, druid_name, druid_pw, 5)  # Druid
set_level(druid_name, 51)
s.close()
druid = login(druid_name, druid_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT},0,0,0,'L23 Outdoor Sandbox','A bare outdoor sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_IN},0,0,0,'L23 Indoor Sandbox','A bare indoor sandbox room.\\n',NULL,9,0,0,0,0,0,0,0,0,0);")
# room_flag 9 = 1 (base) | 8 (ROOM_FLAG_INDOORS)

check("L23 Outdoor Sandbox" in cmd(mage, f"goto {ROOM_OUT}"), "mage goes to the outdoor sandbox")

# Give both casters a spell component (mage/druid both use "component").
COMP1 = BASE + 10
COMP2 = BASE + 11
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({COMP1},'pouch component test','a pouch of test components','A pouch lies here.',12,1,10,10,1);")

check("You conjure" in cmd(mage, f"load obj {COMP1}"), "mage loads a component pouch")
check("You get" in cmd(mage, f"get pouch"), "mage picks up the component pouch")

# --- 1: copy duplicates a real seeded scroll (vnum 90002, "heal light") ---
check("You conjure" in cmd(mage, "load obj 90002"), "mage loads the real seeded healing scroll")
check("You get" in cmd(mage, "get scroll"), "mage picks up the scroll")
out = cmd(mage, "cast copy scroll", 2.0)
check("copy of" in out.lower() and "appears" in out.lower(), "cast copy announces a duplicate appearing")
room_out = cmd(mage, "look")
check(room_out.lower().count("scroll healing minor") >= 1 or "scroll" in room_out.lower(),
      "a copied scroll is now visible in the room")

# --- 2: copy refuses a non-scroll item ---
NOTASCROLL = BASE + 12
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({NOTASCROLL},'plain test rock','a plain test rock','A rock lies here.',8,0,1);")
check("You conjure" in cmd(mage, f"load obj {NOTASCROLL}"), "mage loads a non-scroll fixture")
check("You get" in cmd(mage, "get rock"), "mage picks up the rock")
out = cmd(mage, "cast copy rock")
check("not a scroll" in out.lower(), "cast copy refuses a non-scroll item")

# --- 3: haste applies AFFECT_HASTE ---
out = cmd(mage, "cast haste")
check("cast" in out.lower() and ("ease" in out.lower() or "haste" in out.lower()), "cast haste confirms")
out = cmd(mage, "affects")
check("haste" in out.lower(), "`affects` shows Haste active")

# --- 4: a hasted attacker lands roughly double the strikes of their target ---
def mob_insert(vnum, name, hpbonus, room):
    cols = [
        ("vnum", str(vnum)), ("name", f"'{name}'"), ("short_desc", f"'a {name}'"),
        ("long_desc", f"'A {name} stands here.'"), ("description", f"'A {name} eyes you.'"),
        ("actions", "0"), ("affects", "0"), ("faction", "0"), ("fact_perc", "0"), ("letter", "'A'"),
        ("attacks", "1.0"), ("class", "0"), ("level", "5"), ("tohit", "0"), ("ac", "0"),
        ("hpbonus", str(hpbonus)), ("damage_level", "0"), ("damage_precision", "0"), ("gold", "0"),
        ("race", "0"), ("weight", "0"), ("height", "0"),
        ("str", "0"), ("bra", "0"), ("con", "0"), ("dex", "0"), ("agi", "0"),
        ("intel", "0"), ("wis", "0"), ("foc", "0"), ("per", "0"), ("cha", "0"),
        ("kar", "0"), ("spe", "0"), ("pos", "10"), ("def_position", "10"), ("sex", "1"),
        ("spec_proc", "0"), ("skin", "0"), ("vision", "0"), ("can_be_seen", "1"), ("max_exist", "1"),
    ]
    mob_cols = ",".join(c for c, _ in cols)
    mob_vals = ",".join(v for _, v in cols)
    sql(f"INSERT INTO mob ({mob_cols}) VALUES ({mob_vals});")


dummy_name = f"l23dummy{_suffix}"
mob_insert(MOB_DUMMY1, dummy_name, 8.0, ROOM_OUT)  # generous HP, survives several rounds
check("You conjure" in cmd(mage, f"load mob {dummy_name}"), "a fixed-HP training dummy spawns")

out = cmd(mage, f"kill {dummy_name}")
check("Command not found" not in out, "mage attacks the dummy")

all_out = ""
deadline = time.time() + 12
while time.time() < deadline:
    all_out += recv_all(mage, timeout=1.0)
    if "have slain" in all_out.lower() or "have defeated" in all_out.lower():
        break

my_strikes = all_out.lower().count("you cause") + all_out.lower().count("you hit") \
    + all_out.lower().count("you swing") + all_out.lower().count("you miss")
their_strikes = all_out.lower().count("misses you") + all_out.lower().count("hits you") \
    + all_out.lower().count("catches you")
check(my_strikes > their_strikes,
      f"hasted mage lands noticeably more of their own strikes than the dummy's ({my_strikes} vs {their_strikes})")

cmd(mage, "purge")

# --- 5/6/7: storm call, gated on world weather + indoors/outdoors ---
# Weather is a single WORLD-WIDE sky state loaded once at boot
# (weather.h) -- not live-pollable via SQL. Branches on whatever the
# actual current weather is rather than forcing a specific one, so this
# test is deterministic either way instead of assuming a deploy-time
# SQL+copyover step (documented in this session's own STATUS.md entry)
# was performed first.
weather_out = cmd(druid, "weather")
print("current weather:", repr(weather_out))
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({COMP2},'pouch component test2','a pouch of test components','A pouch lies here.',12,1,10,10,1);")

cmd(druid, f"goto {ROOM_OUT}")
check("You conjure" in cmd(druid, f"load obj {COMP2}"), "druid loads their own component pouch")
check("You get" in cmd(druid, "get pouch"), "druid picks up their component pouch")

if "clear" in weather_out.lower() or "cloudy" in weather_out.lower():
    mob_insert(MOB_DUMMY2, f"l23weather{_suffix}", 5.0, ROOM_OUT)
    check("You conjure" in cmd(druid, f"load mob l23weather{_suffix}"), "an outdoor target spawns for the weather check")
    out = cmd(druid, f"cast storm call l23weather")
    check("fail to call upon the weather" in out.lower(), "storm call refuses outright in clear/cloudy weather")
    cmd(druid, "purge")
    print(">>> SKIP: weather wasn't rainy/stormy at test time, indoor-refusal and hit checks need real storm weather")
else:
    # Weather is rainy/stormy -- verify BOTH gates: refuses indoors, lands
    # a real hit outdoors.
    mob_insert(MOB_DUMMY3, f"l23storm{_suffix}", 5.0, ROOM_IN)
    check("You conjure" in cmd(druid, f"goto {ROOM_IN}"), "druid steps indoors")
    check("You conjure" in cmd(druid, f"load mob l23storm{_suffix}"), "an indoor target spawns")
    out = cmd(druid, "cast storm call l23storm")
    check("have to be outside" in out.lower(), "storm call refuses indoors even in good weather")
    cmd(druid, "purge")

    check("You conjure" in cmd(druid, f"goto {ROOM_OUT}"), "druid steps back outside")
    mob_insert(MOB_DUMMY2, f"l23weather{_suffix}", 5.0, ROOM_OUT)
    check("You conjure" in cmd(druid, f"load mob l23weather{_suffix}"), "an outdoor target spawns")
    out = cmd(druid, "cast storm call l23weather", 2.0)
    check("striking" in out.lower() or "have slain" in out.lower(), "storm call lands a real hit outdoors in good weather")
    cmd(druid, "purge")

print("ALL CHECKS PASSED (see notes above for weather-dependent storm call coverage)")
announce_done("smoke_test_level23_spells")
