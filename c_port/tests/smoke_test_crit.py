#!/usr/bin/env python3
"""Smoke test for the crit-hit/decapitation system (Session 42, user:
"copy sneezys crit hit system, complete with object creation upon
decapitation"). Covers:
  1. A non-head limb (e.g. an arm) reaching 0% HP severs it: a lootable
     "X's severed <limb>" object drops in the room, the victim is told,
     and the victim SURVIVES (still playing, not at the account menu).
  2. Genitalia specifically is severable into an object too (user spec --
     it's not a wearable slot, it's a thing that gets created on a crit).
  3. The HEAD reaching 0% HP is a decapitation: an instant kill (routed
     through the normal combat_defeat() "slain" path) plus a severed-head
     object in the room.
  4. Scope check (user 2026-07-09): this is PCs only -- a mob's head/limb
     reaching 0 HP does NOT sever/decapitate (mobs die the plain way).

Deterministic by design: rather than waiting on combat RNG to land a hit on
a specific limb, this uses the immortal-only `hurtlimb <target> <limb> <hp>`
debug command (cmd_hurtlimb.c) to set a limb's HP directly and observe the
exact same trigger logic a real combat hit would run.

    python3 tests/smoke_test_crit.py [host] [port]
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


announce("smoke_test_crit")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
MOB = ROOM + 1


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
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


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
    send_line(sock, "done"); recv_all(sock)


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Crittest{_suffix}"
imm_pw = "crittestpw123"
victim_name = f"Critvic{_suffix}"
victim_pw = "critvicpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Crit Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Crit Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
check("Crit Sandbox" in cmd(sv, "look"), "the victim lands directly in the sandbox room")

# --- 1: a non-head limb reaching 0 HP severs it (survives) ---
out = cmd(s, f"hurtlimb {victim_name} leftarm 0")
check("Limb HP set" in out, "hurtlimb confirms (not a decapitation)")

outVictim = recv_all(sv, timeout=1.0)
check("your left arm is severed clean off" in outVictim.lower(), "the victim is told their arm was severed")

outRoom = cmd(s, "look")
check(f"{victim_name}'s severed left arm lies here" in outRoom, "a severed-arm object drops in the room")

check("DEAD" not in cmd(sv, "score"), "the victim survives a non-head sever (still playing)")

# --- 2: genitalia specifically is severable (user spec) ---
out = cmd(s, f"hurtlimb {victim_name} genitalia 0")
check("Limb HP set" in out, "hurtlimb accepts genitalia as a limb name")
outRoom = cmd(s, "look")
check(f"{victim_name}'s severed genitalia lies here" in outRoom, "a severed-genitalia object drops in the room")
check("DEAD" not in cmd(sv, "score"), "the victim survives this sever too")

# --- 3: the head reaching 0 HP decapitates (instant kill) ---
out = cmd(s, f"hurtlimb {victim_name} head 0")
check("Decapitated" in out, "hurtlimb reports the decapitation")

outVictim = recv_all(sv, timeout=1.0)
check("your head is severed clean off" in outVictim.lower(), "the victim is told their head was severed")
check(f"you have been slain by {imm_name}".lower() in outVictim.lower(), "decapitation kills via the normal slain path")
check("You are DEAD!" in outVictim, "the victim sees the DEAD message")
check("Your characters" in outVictim, "the victim is dropped at the account menu, not respawned in-world")

outRoom = cmd(s, "look")
check(f"{victim_name}'s severed head lies here" in outRoom, "a severed-head object drops in the room")

# --- 4: scope check -- mobs do NOT sever/decapitate (PCs only) ---
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'critdummy{_suffix}','a crit test dummy','A crit test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
check("You conjure" in cmd(s, f"load mob {MOB}"), "a fresh mob dummy is loaded for the scope check")
out = cmd(s, f"hurtlimb critdummy{_suffix} head 0")
check("Limb HP set" in out, "hurtlimb accepts a mob target without decapitating it")
check("crit test dummy is here" in cmd(s, "look").lower(), "the mob is still alive/present -- no decapitation fired for a mob")
check(f"critdummy{_suffix}'s severed head" not in cmd(s, "look"), "no severed-head object drops for a mob's destroyed limb")

s.close()
sv.close()
announce_done("smoke_test_crit")
print("=== ALL CHECKS PASSED ===")
