#!/usr/bin/env python3
"""Smoke test for `continue` + targeted/breaking holy symbols (user
2026-07-12: "add a continue command so clerics that heal <target> can
continue automatically until the target is fully healed or thier holy
symbol breaks (holy symbols should use the same logic as components for
mages and druids)"). Covers:

  1. `pray heal light <target>` heals someone else in the room, not the
     caster, and the caster's own self-heal ("pray heal light" with no
     target) still works unchanged.
  2. `continue` with no prior heal is refused.
  3. `continue` repeats the last heal-type prayer on the same target,
     each round consuming one more holy symbol, until either the target
     is fully healed or the caster's holy symbols run out.
  4. Once continue reports "fully healed" (or symbols run out), a
     second `continue` call is refused again (state cleared).

    python3 tests/smoke_test_continue.py [host] [port]
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


announce("smoke_test_continue")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
GM_CLERIC = ROOM + 1
SYMBOL_BASE = ROOM + 100


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


def make_guildmaster(vnum, keyword, class_mask):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'guildmaster {keyword}','a guildmaster of {keyword}',"
        f"'A guildmaster of {keyword} stands here.',"
        f"'desc',0,0,0,0,'A',1.0,{class_mask},1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


# --- Immortal setup: sandbox room, guildmaster, symbols, damaged target mob ---
imm_name = f"Contimm{_suffix}"
imm_pw = "contimmpw123"
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
set_level(imm_name, 51)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Continue Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Continue Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

make_guildmaster(GM_CLERIC, f"clerics{_suffix}", 2)
check("You conjure" in cmd(s_imm, f"load mob {GM_CLERIC}"), "the Cleric guildmaster is loaded")

for i in range(6):
    vnum = SYMBOL_BASE + i
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({vnum},'symbol holy silver','a tarnished silver holy symbol',"
        f"'A tarnished silver holy symbol is lying here.',12,1,1);")

# --- Two Cleric characters: healer and a wounded patient ---
pw = "continuepw123"
healer_name = f"Conheal{_suffix}"
sh = make_char(healer_name, pw, "2")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{healer_name}';")
cmd(sh, "quit!")
sh.close()
set_level(healer_name, 40)  # mortal, above "heal light"'s min_level (1)

patient_name = f"Conpat{_suffix}"
sp = make_char(patient_name, pw, "2")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{patient_name}';")
# Wound the patient (a fraction of max_hp) BEFORE the final reconnect --
# same rule as level: a raw SQL change to an already-connected
# descriptor's row never reaches its live being_t, only a fresh login
# picks it up. Wounded (not full) so `continue`'s repeated-heal loop
# actually has to run more than zero rounds to reach "fully healed".
result = subprocess.run(
    ["mariadb", "tobin", "-N", "-e",
     f"SELECT max_hp FROM player_progress WHERE player_id="
     f"(SELECT id FROM player WHERE name='{patient_name}');"],
    check=True, capture_output=True, text=True)
patient_max_hp = int(result.stdout.strip())
cmd(sp, "quit!")
sp.close()
sql(f"UPDATE player_progress SET hp={max(1, patient_max_hp // 5)} WHERE player_id="
    f"(SELECT id FROM player WHERE name='{patient_name}');")

sh = socket.create_connection((host, port), timeout=5)
recv_all(sh)
send_line(sh, healer_name); recv_all(sh)
send_line(sh, pw); recv_all(sh)
send_line(sh, "1"); recv_all(sh)
cmd(sh, "color off")

sp = socket.create_connection((host, port), timeout=5)
recv_all(sp)
send_line(sp, patient_name); recv_all(sp)
send_line(sp, pw); recv_all(sp)
send_line(sp, "1"); recv_all(sp)
cmd(sp, "color off")

# Give the healer Basic discipline (practice at the Cleric guildmaster).
for _ in range(10):
    cmd(sh, "practice basic")

# --- 1: continue with nothing to continue is refused ---
out = cmd(sh, "continue")
check("aren't healing anyone" in out, "continue with no prior heal is refused")

# --- 2: pray heal light with no target heals self; unaffected by this change ---
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL_BASE}"), "a holy symbol is loaded for the healer")
out = cmd(sh, "get symbol")
check("you get" in out.lower(), "the healer picks up a holy symbol")

out = cmd(sh, "pray heal light")
check("You pray for heal light and feel restored" in out, "self-pray (no target) still works unchanged")

# --- 3: pray heal light <target> heals someone else ---
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL_BASE + 1}"), "a second holy symbol is loaded for the healer")
out = cmd(sh, "get symbol")
check("you get" in out.lower(), "the healer picks up a second holy symbol")

out = cmd(sh, f"pray heal light {patient_name.lower()}")
check("is restored" in out, "pray heal light <target> reports the target being restored")
recv_all(sp, 0.3)  # drain the patient's private heal notification (not independently asserted here)

# --- 4: continue repeats the targeted heal until symbols run out ---
for i in range(2, 6):
    check("You conjure" in cmd(s_imm, f"load obj {SYMBOL_BASE + i}"), f"holy symbol #{i+1} is loaded for the healer")
    cmd(sh, "get symbol")

out = cmd(sh, "continue")
check("holy symbol breaks" in out or "fully healed" in out,
      "continue repeats the heal until symbols run out or the target is fully healed")

# --- 5: continue again afterward is refused (state cleared either way) ---
out = cmd(sh, "continue")
check("aren't healing anyone" in out, "continue after finishing is refused again")

s_imm.close()
sh.close()
sp.close()
announce_done("smoke_test_continue")
print("=== ALL CHECKS PASSED ===")
