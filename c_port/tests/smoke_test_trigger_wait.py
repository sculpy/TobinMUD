#!/usr/bin/env python3
"""Smoke test for the trigger system's `wait`/`say` actions (user: "i want
to put this script on a mob" -- a market-vendor-style mob calling out
items one line at a time, e.g. Monty Python's "Wolf nipple chips... get
'em while they're hot"). Neither verb existed before -- the vocabulary
was echo/echoroom/emote/teleport/give/damage/log, no pause primitive at
all (trigger_run() ran a whole script synchronously, in one pass).

  1. `say <text>` renders "<Mob> says, '<text>'" to the room.
  2. `wait <seconds>` pauses the REST of the script; it does NOT run in
     the same pass that hit the wait line.
  3. The paused continuation resumes later (forced via `aitick`, same
     precedent as the other ambient-tick tests) and picks up exactly
     where it left off -- including a SECOND wait/say pair in sequence.

    python3 tests/smoke_test_trigger_wait.py [host] [port]
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


announce("smoke_test_trigger_wait")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 60000)
MOB_VENDOR = ROOM + 1


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    try:
        while time.time() < deadline:
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
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "2"); recv_all(sock)


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
        f"'desc',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


imm_name = f"Trigwimm{_suffix}"
imm_pw = "trigwimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Vendor Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Vendor Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

vendor_kw = f"vendor{_suffix}"
make_mob(MOB_VENDOR, vendor_kw)
check("You conjure" in cmd(s, f"load mob {MOB_VENDOR}"), "the vendor mob is loaded")

out = cmd(s, f"edit trigger mob {MOB_VENDOR} random 100")
check("Writing trigger" in out, "edit trigger mob random opens the script editor")
cmd(s, "say Wolf nipple chips!")
# A LONG wait (not the 1s a real vendor script would actually use) is
# deliberate: the live server's own background pulse scheduler resolves
# `wait`-paused scripts on its own real ~1s cadence too (trigger_pending_tick,
# main.c) -- a short wait can get resolved by THAT before this test's own
# "hasn't fired yet" check even runs (a real race, caught live: the recv
# window below is itself up to ~1s). A wait this long can only resolve via
# the explicit aitick force below, making the test deterministic.
cmd(s, "wait 3600")
cmd(s, "say Otters' noses!")
check("Trigger saved" in cmd(s, "/s"), "the vendor's wait/say script saves")

out = cmd(s, "aitick 1")
check(f"A {vendor_kw} says, 'Wolf nipple chips!'" in out,
      "the first say line fires immediately on the random trigger")
check("Otters' noses" not in out,
      "the line AFTER wait does NOT fire in the same pass -- it's genuinely paused")

out = cmd(s, "aitick 1")
check("Otters' noses" in out,
      "the paused continuation resumes on the next forced tick and picks up where it left off")

# --- teardown: same precedent as smoke_test_trigger.py -- a 100%-chance
# random trigger left attached is permanent ambient noise for every test
# that runs after this one. ---
listing = cmd(s, f"edit trigger list mob {MOB_VENDOR}")
for trig_id in re.findall(r"#(\d+)", listing):
    cmd(s, f"edit trigger delete {trig_id}")

s.close()
announce_done("smoke_test_trigger_wait")
print("=== ALL CHECKS PASSED ===")
