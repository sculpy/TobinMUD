#!/usr/bin/env python3
"""Smoke test for combat_music_tick() (combat.c): a real MSP
`!!MUSIC(<track> L=-1)` marker fires once combat starts, and a real
`!!MUSIC(Off)` fires once it ends (flee). Same "one continuous
connection, no reconnect, goto <player name> instead of a custom
sandbox room" pattern smoke_test_gmcp_msdp_msp.py settled on, for the
same reason -- see that file's own module docstring.

    python3 tests/smoke_test_msp_music.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
MOB = 943200 + (int(time.time()) % 25000)

IAC, WILL, DO, SB, SE = 255, 251, 253, 250, 240
MSP = 90

# Filenames match combat.c's combat_music_tick() TRACKS[] -- match by
# pattern (musicN.wav) rather than an exact list, so a future re-pack
# doesn't go stale here again (found 2026-08-21: this list still had the
# pre-2026-08-07 sound-pack names, e.g. "adventure1.wav", long since
# renamed to music1.wav-music25.wav).
TRACK_RE = re.compile(r"!!MUSIC\(music\d+\.wav L=-1\)")


def recv_for(sock, total_seconds):
    """Wall-clock-bounded read -- see smoke_test_gmcp_msdp_msp.py's own
    copy of this helper for why (mud_test_utils.recv_all()'s idle-gap
    timeout never fires while a fight keeps producing new output)."""
    sock.settimeout(0.5)
    end = time.time() + total_seconds
    chunks = []
    while time.time() < end:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            continue
    return b"".join(chunks).decode(errors="replace")


announce("smoke_test_msp_music", host, port)

raw_sock = socket.create_connection((host, port), timeout=5)
raw_sock.settimeout(1.0)
try:
    while True:
        if not raw_sock.recv(4096):
            break
except socket.timeout:
    pass
raw_sock.sendall(bytes([IAC, DO, MSP]))
sw = raw_sock
name = f"Msp{_suffix}"
pw = "msptestpw123"
recv_all(sw)
send_line(sw, name); recv_all(sw)
send_line(sw, "y"); recv_all(sw)
send_line(sw, pw); recv_all(sw)
send_line(sw, pw); recv_all(sw)
create_character(sw, name, send_line, recv_all, char_class="3")
cmd(sw, "color off")
sql(f"UPDATE player_progress SET level=10 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")

imm_name, imm_pw = f"MspImm{_suffix}", "mspimmpw123"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, imm_pw); recv_all(s)
create_character(s, imm_name, send_line, recv_all, char_class="1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")

s = socket.create_connection((host, port), timeout=5)
s.settimeout(1.0)
try:
    while s.recv(4096):
        pass
except socket.timeout:
    pass
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

goto_out = cmd(s, f"goto {name}")
check("no such" not in goto_out.lower(), "the immortal goto's to the fighter's own room")

sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'dummy training','a training dummy','A training dummy stands here.',"
    f"'desc',0,0,0,0,'A',1.0,50,0,50000,0.0,0.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
# tohit=0 (never lands a hit) -- the standard harmless-sandbox-dummy
# convention every other smoke test uses (smoke_test_pursuit.py,
# smoke_test_kick_starts_fight.py, etc). combat_music_tick() (combat.c)
# runs on its own COMBAT_ROUND_PULSES pulse, separate from per-swing
# damage in combat_process_run() -- it just needs the fight to still be
# going by the time that pulse fires. The gmcp_msdp_msp.py test's
# tohit=99/50000-HP "guaranteed lethal hit" dummy was copy-pasted in
# here first, but that dummy can one-shot even an HP-boosted PC via a
# limb-destruction crit (found live 2026-08-21: "Your body is destroyed"
# -- unrelated to raw HP total) before the tick ever fires. Wrong dummy
# for this test's purpose; swapped for the harmless one instead of
# further HP-boosting a lethal one.
check("You conjure" in cmd(s, f"load mob {MOB}"), "the training dummy is loaded")

send_line(sw, "hit dummy")
fight_out = recv_for(sw, 3.0)
check(TRACK_RE.search(fight_out), "combat start pushes a real MSP MUSIC marker for one of the real tracks")

# flee (cmd_flee.c) has a real ~33% failure chance for an unproficient
# character -- keep retrying (not looping the assert itself) until it
# actually breaks off the fight or a generous deadline passes.
flee_out = ""
deadline = time.time() + 15.0
while "!!MUSIC(Off)" not in flee_out and time.time() < deadline:
    send_line(sw, "flee")
    flee_out += recv_for(sw, 2.0)
check("!!MUSIC(Off)" in flee_out, "combat end (flee) pushes a real MSP MUSIC(Off) stop marker")

sw.close()
s.close()

sql(f"DELETE FROM mob WHERE vnum={MOB};")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player WHERE name='{name}';")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"DELETE FROM player WHERE name='{imm_name}';")

announce_done("smoke_test_msp_music", host, port)
print("=== ALL CHECKS PASSED ===")
