#!/usr/bin/env python3
"""Smoke test for the alignment stat + mob aggression reaction (Session 43
continued, user: "class Mobile_Attitude in sneezy should be implemented
into tobin. mobs should react to good vs evil and react accordingly").
Scoped down from the original's full Mobile_Attitude (suspicion/greed/
malice/anger, hate/fear lists, faction/hunting -- see sneezymud-master/
docs/systems/critical/14-monster-ai-behavior.md) to the identified
prerequisite (progress_t.alignment, -1000 evil .. +1000 good, 0 neutral
default) plus the one reaction described: an ACT_AGGRESSIVE mob backs off
a sufficiently good-aligned target.

  1. A fresh character starts neutral (score shows "Alignment: neutral").
  2. `set <name> alignment <value>` (58+) changes it, reflected in score
     with the right word tier, and rejects out-of-range values.
  3. An ACT_AGGRESSIVE mob attacks a neutral-aligned mortal within a
     forced batch of `aitick` calls (same determinism approach as
     smoke_test_mob_ai.py -- the real ~25%-per-~60s-tick chance is too
     slow to wait on).
  4. The same mob does NOT attack a good-aligned (>=350) mortal within
     the same forced batch.

    python3 tests/smoke_test_alignment.py [host] [port]
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


announce("smoke_test_alignment")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_1 = 900000 + (int(time.time()) % 70000)
ROOM_2 = ROOM_1 + 1
MOB_1 = ROOM_1 + 2
MOB_2 = ROOM_1 + 3

ACT_AGGRESSIVE = 32


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
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_mob(vnum, keyword):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'{keyword}','a {keyword}','A {keyword} stands here.',"
        f"'desc',{ACT_AGGRESSIVE},0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


# --- Part 1 & 2: alignment starts neutral, `set` changes/rejects it ---
mort_name = f"Alignmort{_suffix}"
mort_pw = "alignmortpw123"
sv = socket.create_connection((host, port), timeout=5)
make_char(sv, mort_name, mort_pw)

out = cmd(sv, "score")
check("Alignment:     neutral" in out, "a fresh character starts neutral")

imm_name = f"Alignimm{_suffix}"
imm_pw = "alignimmpw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 58)
s.close()
s = login(imm_name, imm_pw)

out = cmd(s, f"set {mort_name} alignment 500")
check("alignment is now 500 (good)" in out, "set alignment updates and names the tier")
out = cmd(sv, "score")
check("Alignment:     good" in out, "the mortal's own score reflects the new alignment")

out = cmd(s, f"set {mort_name} alignment 5000")
check("must be between -1000" in out, "an out-of-range alignment is rejected")

out = cmd(s, f"set {mort_name} alignment -500")
check("alignment is now -500 (evil)" in out, "set alignment accepts negative (evil) values too")

sv.close()

# --- Part 3: an aggressive mob attacks a neutral-aligned mortal ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_1},0,0,0,'Align Room 1','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_2},0,0,0,'Align Room 2','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

neutral_name = f"Alignneut{_suffix}"
neutral_pw = "alignneutpw123"
sn = socket.create_connection((host, port), timeout=5)
make_char(sn, neutral_name, neutral_pw)
sql(f"UPDATE player SET load_room={ROOM_1} WHERE name='{neutral_name}';")
cmd(sn, "quit!")
sn.close()
sn = login(neutral_name, neutral_pw)
check("Align Room 1" in cmd(sn, "look"), "the neutral mortal lands in room 1")

check("Align Room 1" in cmd(s, f"goto {ROOM_1}"), "immortal goes to room 1 to load the aggressive mob")
make_mob(MOB_1, f"aggrodummy1{_suffix}")
check("You conjure" in cmd(s, f"load mob {MOB_1}"), "aggressive dummy 1 loaded in room 1")

cmd(s, "aitick 30")
# combat messages only reach the two fighting parties directly (no room
# broadcast in this simplified combat model) -- the mob has no descriptor,
# so the tell() lands only on sn, not on the immortal who issued aitick.
out = recv_all(sn, timeout=0.5)
check("attacks you" in out.lower(), "the neutral-aligned mortal is told the mob attacked")
out = cmd(sn, "score")
check("Fighting" in out, "the neutral-aligned mortal is now in a fight (score shows Fighting)")

sn.close()

# --- Part 4: the same kind of mob leaves a good-aligned mortal alone ---
good_name = f"Aligngood{_suffix}"
good_pw = "aligngoodpw123"
sg = socket.create_connection((host, port), timeout=5)
make_char(sg, good_name, good_pw)
sql(f"UPDATE player SET load_room={ROOM_2} WHERE name='{good_name}';")
cmd(sg, "quit!")
sg.close()
sg = login(good_name, good_pw)
check("Align Room 2" in cmd(sg, "look"), "the good mortal lands in room 2")

out = cmd(s, f"set {good_name} alignment 500")
check("alignment is now 500 (good)" in out, "the second mortal is set to good alignment")

check("Align Room 2" in cmd(s, f"goto {ROOM_2}"), "immortal goes to room 2 to load a second aggressive mob")
make_mob(MOB_2, f"aggrodummy2{_suffix}")
check("You conjure" in cmd(s, f"load mob {MOB_2}"), "aggressive dummy 2 loaded in room 2")

cmd(s, "aitick 30")
out = recv_all(sg, timeout=0.5)
check("attacks you" not in out.lower(),
      "the good-aligned mortal is never told the mob attacked")
out = cmd(sg, "score")
check("Fighting" not in out, "the good-aligned mortal is left alone (score shows no Fighting)")

s.close()
sg.close()
announce_done("smoke_test_alignment")
print("=== ALL CHECKS PASSED ===")
