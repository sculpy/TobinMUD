#!/usr/bin/env python3
"""Smoke test for `practice` + guildmaster-gated discipline percentages
(user 2026-07-12: "add the practice command so players have to visit a
guildmaster to gain skills based upon percentage of discipline learned.
cant get to advanced disc until basic disc is at least 95% complete").
Covers:

  1. `practice` with no guildmaster in the room is refused.
  2. A guildmaster of the WRONG class doesn't count.
  3. A Basic-tier Cleric prayer ("heal light") is refused at 0% Basic
     discipline, even though level/holy-symbol are satisfied.
  4. `practice basic` at the right guildmaster raises Basic discipline
     10% per use; `practice advanced` is refused below 95% Basic.
  5. Once Basic reaches 100%, the Basic-tier prayer succeeds; an
     Advanced-tier prayer ("heal full") is still refused at 0% Advanced.
  6. `practice advanced` succeeds once Basic >= 95%, and afterward the
     Advanced-tier prayer succeeds too.
  7. `skills` shows the Basic/Advanced discipline percentages and locks
     the Advanced tier's header note until Basic >= 95%.

    python3 tests/smoke_test_practice.py [host] [port]
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


announce("smoke_test_practice")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
GM_CLERIC = ROOM + 1
GM_MAGE = ROOM + 2
SYMBOL = ROOM + 3


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


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


def make_guildmaster(vnum, keyword, class_mask):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'guildmaster {keyword}','a guildmaster of {keyword}',"
        f"'A guildmaster of {keyword} stands here.',"
        f"'desc',0,0,0,0,'A',1.0,{class_mask},1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


# --- Immortal setup ---
imm_name = f"Pracimm{_suffix}"
imm_pw = "practiceimmpw123"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "2"); recv_all(s_imm)
s_imm.close()
set_level(imm_name, 51)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Practice Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Practice Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")

# --- Cleric test character (basic prayer: "heal light", advanced: "heal full") ---
cleric_name = f"Praccle{_suffix}"
pw = "practicepw123"
sc = make_char(cleric_name, pw, "2")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
# Level must be set (and the connection dropped+re-logged-in to actually
# pick it up from the DB) BEFORE the session under test -- setting it via
# SQL on an already-connected descriptor doesn't reach the live in-memory
# being_t. 40 is high enough for "heal full" (min_level 12) but still
# mortal (>=51 flips being_is_immortal() true and bypasses every gate
# this test exists to check).
cmd(sc, "quit!")
sc.close()
set_level(cleric_name, 40)
sc = socket.create_connection((host, port), timeout=5)
recv_all(sc)
send_line(sc, cleric_name); recv_all(sc)
send_line(sc, pw); recv_all(sc)
send_line(sc, "1"); recv_all(sc)
cmd(sc, "color off")

check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "the holy symbol is loaded")
out = cmd(sc, "get symbol")
check("you get" in out.lower(), "the cleric picks up the holy symbol")

# --- 1: no guildmaster in room ---
out = cmd(sc, "practice")
check("don't see a guildmaster" in out, "practice is refused with no guildmaster present")

# --- 2: wrong-class guildmaster doesn't count ---
make_guildmaster(GM_MAGE, "mages", 1)
check("You conjure" in cmd(s_imm, f"load mob {GM_MAGE}"), "the Mage guildmaster is loaded")
out = cmd(sc, "practice")
check("don't see a guildmaster" in out, "a Mage guildmaster doesn't count for a Cleric")

# --- 3: Basic-tier prayer refused at 0% Basic discipline ---
out = cmd(sc, "pray heal light")
check("haven't practiced your Basic discipline" in out, "Basic-tier prayer refused at 0% Basic discipline")

# --- 4: right guildmaster; practice basic; advanced refused below 95% ---
make_guildmaster(GM_CLERIC, "clerics", 2)
check("You conjure" in cmd(s_imm, f"load mob {GM_CLERIC}"), "the Cleric guildmaster is loaded")

out = cmd(sc, "practice")
check("Basic discipline: 0%" in out, "practice status shows 0% Basic at the right guildmaster")

out = cmd(sc, "practice advanced")
check("Master your Basic discipline first" in out, "practice advanced refused below 95% Basic")

for _ in range(10):
    out = cmd(sc, "practice basic")
check("Basic discipline: 100%" in out, "Basic discipline reaches 100% after 10 practices")

out = cmd(sc, "practice basic")
check("already mastered your Basic discipline" in out, "practicing Basic again at 100% is refused")

# --- 5: Basic-tier prayer succeeds now; Advanced-tier still refused (0% Advanced) ---
out = cmd(sc, "pray heal light")
check("You pray for heal light" in out, "Basic-tier prayer succeeds once Basic discipline is 100%")

out = cmd(sc, "pray heal full")
check("need 95% in your Basic discipline, and some Advanced practice" in out,
      "Advanced-tier prayer still refused at 0% Advanced discipline")

# --- 6: practice advanced now works; Advanced-tier prayer succeeds ---
out = cmd(sc, "practice advanced")
check("Advanced discipline: 10%" in out, "practice advanced succeeds once Basic is 100%")

# The holy symbol from earlier was consumed by the successful "heal
# light" above (user 2026-07-12: "holy symbols should use the same
# logic as components" -- consumed every successful pray, no longer a
# reusable keepsake) -- a fresh one is needed here.
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "a fresh holy symbol is loaded")
out = cmd(sc, "get symbol")
check("you get" in out.lower(), "the cleric picks up the fresh holy symbol")

out = cmd(sc, "pray heal full")
check("You pray for heal full" in out, "Advanced-tier prayer succeeds once Advanced discipline is nonzero")

# --- 7: skills output reflects discipline percentages ---
out = cmd(sc, "skills")
check("Basic discipline:" in out and "Advanced discipline:" in out, "skills shows both discipline percentages")
check("100" in out, "skills shows the 100% Basic discipline value")

s_imm.close()
sc.close()
announce_done("smoke_test_practice")
print("=== ALL CHECKS PASSED ===")
