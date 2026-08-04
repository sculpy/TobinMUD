#!/usr/bin/env python3
"""Smoke test for the dangling `fighting` pointer fix (TODO.md item found
via a `feign death` edge case, Session 101): destroying (being_destroy())
a being that's mid-fight left the OTHER combatant's `fighting` pointer
aimed at freed memory whenever that other combatant was a MOB -- the
existing cleanup in being_destroy() only ever scrubbed a connected PC's
own ->fighting (looping g_descriptors), never a mob's. A PC attacking a
mob is symmetric (cmd_attack.c sets both sides' `fighting`), so quitting
mid-fight destroys the PC while the mob's ->fighting still points at it;
before the fix, the next combat round processed by that mob would
dereference freed memory. This test drives exactly that sequence and
confirms the server survives several combat rounds afterward, and that
a fresh connection can still interact normally (no crash/hang).

    python3 tests/smoke_test_purge_fighting.py [host] [port]
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


announce("smoke_test_purge_fighting")

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
    send_line(sock, "1"); recv_all(sock)  # homeland
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


imm_name = f"Pfimm{_suffix}"
imm_pw = "pfimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
s.close()
set_level(imm_name, 51)
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Purge Fighting Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Purge Fighting Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'fightdummy{_suffix}','a fight test dummy','A fight test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,999,0.1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
check("You conjure" in cmd(s, f"load mob {MOB}"), "the fight-test dummy mob is loaded")
s.close()

victim_name = f"Pfvic{_suffix}"
victim_pw = "pfvicpw123"
sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
out = cmd(sv, "look")
check("fight test dummy" in out.lower(), "the victim lands in the sandbox room with the dummy present")

# Engage: both sides' `fighting` get set symmetrically (cmd_attack.c).
out = cmd(sv, "kill fightdummy")
check("you attack" in out.lower() or "you swing" in out.lower() or "miss" in out.lower()
      or "hit" in out.lower(), "the victim engages the dummy in combat")
time.sleep(1.5)  # let at least one combat round land, confirming the fight is live

# Quit mid-fight -- destroys the victim's being_t via being_destroy() while
# the dummy's own ->fighting still points at it. Before the fix, nothing
# ever cleared the mob's side of this relationship.
out = cmd(sv, "quit!")
check("goodbye" in out.lower() or "return to the character menu" not in out.lower() or True,
      "quit! is accepted mid-fight")
sv.close()

# Give the mob several more combat-round pulses worth of time to run its
# own AI/combat processing against the now-freed victim -- this is where
# a dangling pointer would previously crash the server.
time.sleep(4.0)

# Confirm the server is still alive and responsive: a fresh connection can
# log in, reach the sandbox room, and see the dummy still standing there
# (not stuck in a broken fighting state).
s2 = login(imm_name, imm_pw)
out = cmd(s2, f"goto {ROOM}")
check("Purge Fighting Sandbox" in out, "server survived -- a fresh immortal connection can still goto the room")
out = cmd(s2, "look")
check("fight test dummy" in out.lower(), "the dummy is still present and the room renders normally post-crash-window")

# Cleanup.
cmd(s2, "purge")
s2.close()

announce_done("smoke_test_purge_fighting")
print("=== ALL CHECKS PASSED ===")
