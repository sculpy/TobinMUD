#!/usr/bin/env python3
"""Smoke test for weapon-aware combat messaging + hit/dam bonuses (user,
Session 43 continued: "when in combat wielded items should modify
messaging for example wield sword, you slice instead of hit. This should
apply to all weapon types and add or subtract any hit bonuses placed on
the weapon"). Covers:

  1. Bare-handed combat still says "hit" (no behavior change for the
     unarmed case).
  2. Wielding a weapon whose name contains "sword" changes the verb to
     "slice" (attacker's own message) / "slices" (defender's message,
     third person).
  3. `objaffect` hitroll/damroll bonuses (type 15/16, see obj_repo.c's
     obj_load_combat_mods()) on the wielded weapon's vnum are actually
     applied -- an absurdly large hitroll bonus makes every swing land,
     and an absurdly large damroll bonus makes every hit's damage far
     exceed the un-bonused 1-6ish baseline, both easy to tell apart from
     ordinary RNG variance.

The attacker MUST be a mortal: `attack`/`kill` both route to cmd_kill.c,
which instant-slays for an immortal caller (bypassing combat_strike()
entirely) and only falls through to real multi-round combat for a mortal.
An immortal is still used to bootstrap the sandbox room/mob/weapon (needs
goto/load), but a separate mortal character does the actual attacking.

    python3 tests/smoke_test_weapon_messaging.py [host] [port]
"""
import re
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


announce("smoke_test_weapon_messaging")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
MOB1 = ROOM + 1
MOB2 = ROOM + 2
SWORD = ROOM + 3

WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_WEAPON = 5


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


imm_name = f"Wpntestimm{_suffix}"
imm_pw = "wpntestimmpw123"
mort_name = f"Wpntestmor{_suffix}"
mort_pw = "wpntestmorpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
s.close()
set_level(imm_name, 51)
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Weapon Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Weapon Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# The attacker is immortal (not actually "mortal" despite the variable
# name/character name) -- damage numbers are now hidden from mortal
# viewers (user 2026-07-12), and this file's own checks below need to
# read the actual damage dealt (verifying the damroll bonus), so this
# attacker has to be an immortal like `s` to keep seeing them.
sv = socket.create_connection((host, port), timeout=5)
make_char(sv, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sv, "quit!")
sv.close()
set_level(mort_name, 51)
sv = login(mort_name, mort_pw)
check("Weapon Sandbox" in cmd(sv, "look"), "the (immortal) attacker lands directly in the sandbox room")

def make_mob(vnum, keyword):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'{keyword}','a weapon test dummy','A weapon test dummy stands here.',"
        f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")

make_mob(MOB1, f"wpndummy1{_suffix}")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SWORD},'sword testsword','a test sword','A test sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},1);")

# --- 1: bare-handed still says "hit" ---
check("You conjure" in cmd(s, f"load mob {MOB1}"), "the first weapon-test dummy is loaded")
out = cmd(sv, f"hit wpndummy1{_suffix}")
check("You attack" in out, "attack initiated bare-handed")

found_bare_hit = False
for _ in range(6):
    time.sleep(1.5)
    out = recv_all(sv, timeout=0.5)
    # The mob's display name ("a weapon test dummy") is multi-word, so
    # \w+ can't span it -- .+? (lazy) matches the whole name instead.
    if re.search(r"You hit .+?'s .+? for \d+ damage!", out):
        found_bare_hit = True
        print("=== bare-handed hit message ===")
        print(out)
        break
    if "slain" in out.lower() or "defeated" in out.lower():
        break
check(found_bare_hit, "bare-handed combat still uses the plain 'hit' verb")

cmd(sv, "flee")
recv_all(sv, timeout=1.0)

# --- 2 & 3: wielding a "sword"-keyword weapon with absurd hit/dam bonuses ---
make_mob(MOB2, f"wpndummy2{_suffix}")
check("You conjure" in cmd(s, f"load mob {MOB2}"), "the second weapon-test dummy is loaded")
check("You conjure" in cmd(s, f"load obj {SWORD}"), "the test sword is loaded")
cmd(s, "drop sword")

out = cmd(sv, "get sword")
check("you get" in out.lower(), "the mortal attacker picks up the test sword")
out = cmd(sv, "wield sword")
check("wield" in out.lower(), "wield equips the test sword")

sql(f"INSERT INTO objaffect (vnum, type, mod1, mod2) VALUES "
    f"({SWORD}, 15, 1000, 0), ({SWORD}, 16, 500, 0);")  # APPLY_HITROLL, APPLY_DAMROLL

out = cmd(sv, f"hit wpndummy2{_suffix}")
check("You attack" in out, "attack initiated while wielding the sword")

found_slice = False
found_big_damage = False
# The absurd hitroll/damroll bonuses can kill the dummy in the very first
# combat round, which may resolve fast enough to land in this same initial
# response -- check it (and each subsequent chunk) as it comes in.
chunks = [out]
for _ in range(6):
    time.sleep(1.5)
    chunk = recv_all(sv, timeout=0.5)
    if chunk:
        chunks.append(chunk)
    check("You miss" not in chunk, "the hitroll bonus (1000) means this attacker never misses")
    combined = "".join(chunks)
    m = re.search(r"You slice .+?'s .+? for (\d+) damage!", combined)
    if m:
        found_slice = True
        dmg = int(m.group(1))
        print(f"=== slice message, damage={dmg} ===")
        print(combined)
        check(dmg > 50, "the damroll bonus (500) pushes damage far past the un-bonused baseline (~1-11)")
        found_big_damage = True
    if "slain" in combined.lower() or "defeated" in combined.lower():
        break
    if found_slice:
        break

check(found_slice, "wielding a sword-keyword weapon changes the verb to 'slice'")
check(found_big_damage, "the damroll bonus was actually applied to the hit")

s.close()
sv.close()
announce_done("smoke_test_weapon_messaging")
print("=== ALL CHECKS PASSED ===")
