#!/usr/bin/env python3
"""Smoke test for `practice` + the practice-points/discipline system
(redesigned 2026-07-17, superseding the original 2026-07-12 flat-+10%-
per-use version). Covers:

  1. Bare `practice` (no guildmaster, no points) shows 0% on all three
     disciplines and 0 practice points -- no refusal, works anywhere.
  2. `practice <discipline>` with NO count shows that discipline's skill
     listing (with per-skill proficiency) anywhere, no guildmaster
     needed -- a level-1 skill shows locked-by-discipline at 0%.
  3. `practice <discipline> <count>` (an explicit count) is the only form
     that spends points, and DOES require the matching guildmaster --
     refused with no guildmaster present, and refused at the WRONG
     class's guildmaster.
  4. Spending is refused with 0 practice points; `set <name> practices
     <n>` (this session's new `set` field) grants some.
  5. Spending raises the discipline percentage (random 1-2%/point,
     capped at 100, stops early when out of points or at the cap);
     re-spending at 100% is refused ("already mastered").
  6. `practice advanced <count>` is refused until Basic AND Combat both
     reach 100%.
  7. `practice <yourclassname>` is a synonym for `practice basic`.
  8. Per-skill proficiency (separate from the discipline gate) actually
     gates `pray` success: a spell forced to 100% proficiency succeeds
     reliably; one left at the 1% first-attempt floor fumbles far more
     often than not (statistical, small sample).
  9. `skills` shows each known skill's own proficiency in brackets.

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
GM_MAGE_BASIC = ROOM + 1
GM_CLERIC_BASIC = ROOM + 2
GM_CLERIC_COMBAT = ROOM + 3
GM_CLERIC_ADVANCED = ROOM + 4
SYMBOL = ROOM + 5


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


def cmd_paged(sock, line, timeout=1.0, max_pages=10):
    out = cmd(sock, line, timeout)
    pages = 0
    while "ENTER" in out and "more" in out and pages < max_pages:
        out += cmd(sock, "", timeout)
        pages += 1
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_skill_pct(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"SELECT id, '{skill_name}', {pct}, 0 FROM player WHERE name='{name}' "
        f"ON DUPLICATE KEY UPDATE pct={pct}, last_gain_at=0;")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


def make_guildmaster(vnum, keyword, class_mask, level):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'guildmaster {keyword}','a guildmaster of {keyword}',"
        f"'A guildmaster of {keyword} stands here.',"
        f"'desc',0,0,0,0,'A',1.0,{class_mask},{level},0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
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
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
s_imm.close()
set_level(imm_name, 58)  # 58+ needed for the `set` command used below (SET_MIN_LEVEL)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Practice Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Practice Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")

# --- Cleric test character ---
cleric_name = f"Praccle{_suffix}"
pw = "practicepw123"
sc = make_char(cleric_name, pw, "2")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
cmd(sc, "quit!")
sc.close()
set_level(cleric_name, 40)
sc = socket.create_connection((host, port), timeout=5)
recv_all(sc)
send_line(sc, cleric_name); recv_all(sc)
send_line(sc, pw); recv_all(sc)
send_line(sc, "1"); recv_all(sc)
cmd(sc, "color off")

# --- 1: bare `practice` works anywhere, no guildmaster, shows 0% status ---
out = cmd(sc, "practice")
check("Basic:" in out and "0%" in out and "Combat:" in out and "Advanced:" in out,
      "bare practice shows all three disciplines at 0% with no guildmaster present")
check("Practice points available:" in out and "0" in out.split("Practice points available:")[1][:5],
      "bare practice shows 0 practice points")
check("is here" not in out, "no guildmaster flavor line when none is present")

# --- 2: `practice basic` (no count) shows the listing anywhere ---
out = cmd_paged(sc, "practice basic")
check("Basic discipline: 0%" in out, "practice basic (no count) shows the 0% Basic discipline header")
check("practice this discipline to unlock" in out,
      "a level-1 Basic skill shows locked-by-discipline, not locked-by-level")

# --- 3: an explicit count DOES need a guildmaster; wrong class doesn't count ---
out = cmd(sc, "practice basic 1")
check("don't see a Basic guildmaster" in out, "practice basic <count> is refused with no guildmaster present")

make_guildmaster(GM_MAGE_BASIC, "mages", 1, 51)
check("You conjure" in cmd(s_imm, f"load mob {GM_MAGE_BASIC}"), "the Mage Basic guildmaster is loaded")
out = cmd(sc, "practice basic 1")
check("don't see a Basic guildmaster" in out, "a Mage guildmaster doesn't count for a Cleric")

# --- 4: right guildmaster, but no practice points yet ---
make_guildmaster(GM_CLERIC_BASIC, "clerics", 2, 51)
check("You conjure" in cmd(s_imm, f"load mob {GM_CLERIC_BASIC}"), "the Cleric Basic guildmaster is loaded")
out = cmd(sc, "practice basic 1")
check("no practice points" in out, "spending is refused with 0 practice points")

# --- 5: grant practice points via `set` (this session's new field), then spend ---
out = cmd(s_imm, f"set {cleric_name} practices 200")
check("practice points are now 200" in out, "`set <name> practices <n>` grants practice points")

out = cmd(sc, "practice basic 200")
check("Basic discipline: 100%" in out, "spending enough points reaches 100% Basic discipline")

out = cmd(sc, "practice basic 1")
check("already mastered your Basic discipline" in out, "spending again at 100% is refused")

# --- 6: Advanced is refused until Basic AND Combat are both 100% ---
make_guildmaster(GM_CLERIC_ADVANCED, "clerics advanced", 2, 100)
check("You conjure" in cmd(s_imm, f"load mob {GM_CLERIC_ADVANCED}"), "the Cleric Advanced guildmaster is loaded")
cmd(s_imm, f"set {cleric_name} practices 50")
out = cmd(sc, "practice advanced 1")
check("Master your Basic and Combat disciplines first" in out,
      "practice advanced is refused while Combat is still below 100%")

out = cmd(s_imm, f"set {cleric_name} combat 100")
check("combat discipline is now 100" in out, "`set <name> combat <pct>` raises Combat discipline live")
out = cmd(sc, "practice advanced 1")
check("Advanced discipline:" in out and "already mastered" not in out,
      "practice advanced succeeds once Basic and Combat both reached 100%")

# --- 7: class name is a synonym for `basic` ---
out = cmd_paged(sc, "practice cleric")
check("Basic discipline:" in out, "`practice <yourclass>` shows the same listing as `practice basic`")

# --- 8: per-skill proficiency actually gates pray success ---
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "a holy symbol is loaded")
out = cmd(sc, "get symbol")
check("you get" in out.lower(), "the cleric picks up a holy symbol")

set_skill_pct(cleric_name, "heal light", 100)
out = cmd(sc, "pray heal light")
check("You pray for heal light" in out, "a spell forced to 100% proficiency succeeds")

# "heal full" left untouched -- first attempt floors at 1% and is rolled
# against immediately, so it should fumble far more often than not over
# a handful of tries (statistical, not a guaranteed single-shot).
fumbles = 0
for _ in range(8):
    check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "a fresh holy symbol is loaded")
    cmd(sc, "get symbol")
    out = cmd(sc, "pray heal full")
    if "fumble" in out:
        fumbles += 1
check(fumbles >= 5, f"a freshly-floored (~1%) spell fumbles most attempts ({fumbles}/8 fumbled)")

# --- 9: skills shows each known skill's own proficiency ---
out = cmd_paged(sc, "skills")
check("[100%]" in out, "skills shows the forced 100% proficiency for heal light")

s_imm.close()
sc.close()
announce_done("smoke_test_practice")
print("=== ALL CHECKS PASSED ===")
