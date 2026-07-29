#!/usr/bin/env python3
"""Smoke test for the level-25 castable-spell/prayer audit batch
(2026-07-29): Mage's detect invisibility/detect magic/bind/enhance-weapon-
adjacent new casts, Cleric's restore limb/knit bone/bleed/heroes' feast,
Druid's refresh. Physical combat skills (Warrior/Thief/Monk) are a
separate, not-yet-built batch -- not covered here.

    python3 tests/smoke_test_level25_spells.py [host] [port]
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


announce("smoke_test_level25_spells")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900200 + (int(time.time()) % 70000)
ROOM_OUT = BASE


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


mage_name = f"Lmg{_suffix}"
mage_pw = "l25magepw123"
cleric_name = f"Lcl{_suffix}"
cleric_pw = "l25clericpw123"
druid_name = f"Ldrx{_suffix}"
druid_pw = "l25druidpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, mage_name, mage_pw, 1)  # Mage
set_level(mage_name, 51)
s.close()
mage = login(mage_name, mage_pw)

s = socket.create_connection((host, port), timeout=5)
make_char(s, cleric_name, cleric_pw, 2)  # Cleric
set_level(cleric_name, 51)
s.close()
cleric = login(cleric_name, cleric_pw)

s = socket.create_connection((host, port), timeout=5)
make_char(s, druid_name, druid_pw, 5)  # Druid
set_level(druid_name, 51)
s.close()
druid = login(druid_name, druid_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT},0,0,0,'L25 Outdoor Sandbox','A bare outdoor sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

check("L25 Outdoor Sandbox" in cmd(mage, f"goto {ROOM_OUT}"), "mage goes to the outdoor sandbox")
check("L25 Outdoor Sandbox" in cmd(cleric, f"goto {ROOM_OUT}"), "cleric goes to the outdoor sandbox")
check("L25 Outdoor Sandbox" in cmd(druid, f"goto {ROOM_OUT}"), "druid goes to the outdoor sandbox")

COMP1 = BASE + 10
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({COMP1},'pouch component test','a pouch of test components','A pouch lies here.',12,1,10,10,1);")
check("You conjure" in cmd(mage, f"load obj {COMP1}"), "mage loads a component pouch")
check("You drop" in cmd(mage, "drop pouch"), "mage drops one for the cleric/druid")
check("You get" in cmd(cleric, "get pouch"), "cleric picks up a component pouch")

HSYM = BASE + 11
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({HSYM},'holy symbol test','a wooden holy symbol','A holy symbol lies here.',12,1,10,10,1);")
check("You conjure" in cmd(mage, f"load obj {HSYM}"), "mage loads a holy symbol for the cleric")
check("You drop" in cmd(mage, "drop symbol"), "mage drops the holy symbol")
check("You get" in cmd(cleric, "get symbol"), "cleric picks up their holy symbol")
check("You conjure" in cmd(mage, f"load obj {COMP1}"), "mage loads a second component pouch")
check("You drop" in cmd(mage, "drop pouch"), "mage drops one for the druid")
check("You get" in cmd(druid, "get pouch"), "druid picks up a component pouch")
check("You conjure" in cmd(mage, f"load obj {COMP1}"), "mage loads their own component pouch back")

# --- Mage: detect invisibility ---
out = cmd(mage, "cast detect invisibility")
check("tingle" in out.lower(), "cast detect invisibility confirms")
check("detect invisibility" in cmd(mage, "affects").lower(), "`affects` shows Detect Invisibility active")

# --- Mage: detect magic (flavor-only) ---
out = cmd(mage, "cast detect magic")
check("tingle" in out.lower(), "cast detect magic confirms")

# --- Mage: bind (opens combat, applies AFFECT_BIND, blocks movement) ---
DUMMY = BASE + 2
cols = [
    ("vnum", str(DUMMY)), ("name", f"'l25dummy{_suffix}'"), ("short_desc", f"'a l25dummy{_suffix}'"),
    ("long_desc", f"'A l25dummy{_suffix} stands here.'"), ("description", "'A dummy eyes you.'"),
    ("actions", "0"), ("affects", "0"), ("faction", "0"), ("fact_perc", "0"), ("letter", "'A'"),
    ("attacks", "1.0"), ("class", "0"), ("level", "5"), ("tohit", "0"), ("ac", "0"),
    ("hpbonus", "8.0"), ("damage_level", "0"), ("damage_precision", "0"), ("gold", "0"),
    ("race", "0"), ("weight", "0"), ("height", "0"),
    ("str", "0"), ("bra", "0"), ("con", "0"), ("dex", "0"), ("agi", "0"),
    ("intel", "0"), ("wis", "0"), ("foc", "0"), ("per", "0"), ("cha", "0"),
    ("kar", "0"), ("spe", "0"), ("pos", "10"), ("def_position", "10"), ("sex", "1"),
    ("spec_proc", "0"), ("skin", "0"), ("vision", "0"), ("can_be_seen", "1"), ("max_exist", "1"),
]
mob_cols = ",".join(c for c, _ in cols)
mob_vals = ",".join(v for _, v in cols)
sql(f"INSERT INTO mob ({mob_cols}) VALUES ({mob_vals});")
check("You conjure" in cmd(mage, f"load mob l25dummy{_suffix}"), "a training dummy spawns for bind")
out = cmd(mage, f"cast bind l25dummy")
check("web-like substance" in out.lower(), "cast bind confirms")
cmd(mage, "purge")

# --- Cleric: restore limb (after paralyze limb) ---
check("L25 Outdoor Sandbox" in cmd(cleric, f"goto {ROOM_OUT}"), "cleric confirms room")
out = cmd(cleric, "pray for paralyze limb self")
print("paralyze limb self attempt:", repr(out[:150]))
# `paralyze limb` is offensive (needs atk_target/an opponent) -- cast on a
# dummy instead, then have the SAME cleric restore it (restore limb has no
# offense requirement, works on anyone including self).
sql(f"INSERT INTO mob ({mob_cols}) VALUES ({mob_vals.replace(str(DUMMY), str(DUMMY+1), 1)});")
check("You conjure" in cmd(cleric, f"load mob l25dummy{_suffix}"), "a second training dummy spawns")
out = cmd(cleric, f"pray for paralyze limb l25dummy")
check("goes limp" in out.lower(), "paralyze limb lands on the dummy")
out = cmd(cleric, f"pray for restore limb l25dummy")
check("knits back together" in out.lower(), "restore limb repairs the paralyzed limb")
cmd(cleric, "purge")

# --- Cleric: knit bone (no-op path, since applying a real broken-bone
# affect requires the disease roll -- just confirm the "no broken bones"
# refusal path works, proving the branch is wired) ---
out = cmd(cleric, "pray for knit bone")
check("no broken bones" in out.lower(), "knit bone confirms the no-op refusal path")

# --- Cleric: bleed (offensive) ---
sql(f"INSERT INTO mob ({mob_cols}) VALUES ({mob_vals.replace(str(DUMMY), str(DUMMY+2), 1)});")
check("You conjure" in cmd(cleric, f"load mob l25dummy{_suffix}"), "a third training dummy spawns")
out = cmd(cleric, f"pray for bleed l25dummy")
check("bleeding wound" in out.lower(), "bleed opens a bleeding wound on the dummy")
cmd(cleric, "purge")

# --- Cleric: heroes' feast ---
out = cmd(cleric, "pray for heroes' feast")
check("heroes' feast" in out.lower(), "heroes' feast confirms")

# --- Druid: refresh (vitality restore) ---
out = cmd(druid, "cast refresh")
check("vit" in out.lower(), "cast refresh restores vitality")

print("ALL CHECKS PASSED")
announce_done("smoke_test_level25_spells")
