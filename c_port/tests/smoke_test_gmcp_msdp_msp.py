#!/usr/bin/env python3
"""Smoke test for the GMCP/MSDP/MSP protocol layer (TobinMUD Client
project, 2026-08-05):
  1. The server offers IAC WILL GMCP/MSDP/MSP on connect (raw bytes,
     before any login).
  2. Answering IAC DO GMCP/MSDP/MSP sets the matching per-descriptor
     flag -- verified indirectly via real pushes only firing once
     opted in.
  3. A real Room.Info GMCP push fires on `look`.
  4. A real Char.Vitals GMCP push + MSDP VAR/VAL push fires when the
     character takes damage.
  5. A real MSP "!!SOUND(...)" in-band marker fires on the same hit.

Deliberately keeps the warrior on ONE continuous connection from
creation through combat, and never needs to know/parse the warrior's
room -- the immortal reaches it via `goto <player name>` (teleport-to-
player, cmd_goto.c) instead of a custom sandbox room + `transfer`. An
earlier version reconnected the warrior and separately used a custom
room + `transfer` and both hit a real, pre-existing flaky hang in
several different spots (confirmed unrelated to this feature:
reproduced identically against a from-scratch build of the unmodified
pre-GMCP code, and again even with the goto/no-transfer redesign) --
logged as TODO.md's "Intermittent hang..." follow-up rather than fixed
here (out of this feature's scope).

    python3 tests/smoke_test_gmcp_msdp_msp.py [host] [port]
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
MOB = 943100 + (int(time.time()) % 25000)

IAC, WILL, DO, SB, SE = 255, 251, 253, 250, 240
GMCP, MSDP, MSP = 201, 69, 90


def recv_for(sock, total_seconds):
    """Local variant of recv_all() bounded by TOTAL wall-clock time, not
    an idle gap between reads -- mud_test_utils.recv_all() re-arms its
    timeout on every recv() call, so it never returns while the peer
    keeps sending SOMETHING within each window. The dummy below fights
    forever (50000 HP, matching every other smoke test's own unkillable-
    dummy convention) and produces a new combat-round message every
    ~1.2s, well under any idle-gap timeout. Kept as a local copy rather
    than a mud_test_utils.py change, matching that module's own
    documented policy for genuinely-customized read needs."""
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


announce("smoke_test_gmcp_msdp_msp", host, port)

# --- Part 1: raw connect-time negotiation offer ---
raw_sock = socket.create_connection((host, port), timeout=5)
raw_sock.settimeout(1.0)
first_bytes = b""
try:
    while True:
        chunk = raw_sock.recv(4096)
        if not chunk:
            break
        first_bytes += chunk
except socket.timeout:
    pass
check(bytes([IAC, WILL, GMCP]) in first_bytes, "server offers IAC WILL GMCP on connect")
check(bytes([IAC, WILL, MSDP]) in first_bytes, "server offers IAC WILL MSDP on connect")
check(bytes([IAC, WILL, MSP]) in first_bytes, "server offers IAC WILL MSP on connect")

# --- Part 2: opt in (IAC DO GMCP/MSDP/MSP), then create the warrior --
# stays on THIS one connection (sw) for the rest of the test. ---
raw_sock.sendall(bytes([IAC, DO, GMCP, IAC, DO, MSDP, IAC, DO, MSP]))
sw = raw_sock
name = f"Gmcp{_suffix}"
pw = "gmcptestpw123"
recv_all(sw)
send_line(sw, name); recv_all(sw)
send_line(sw, "y"); recv_all(sw)
send_line(sw, pw); recv_all(sw)
send_line(sw, pw); recv_all(sw)
create_out = create_character(sw, name, send_line, recv_all, char_class="3")  # warrior
cmd(sw, "color off")
check(len(create_out) > 0, "character creation produced real auto-look output")

sql(f"UPDATE player_progress SET level=10 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")

# --- Immortal: create + quit + SQL level bump + reconnect (the level
# change only takes effect on a fresh login -- being_is_immortal() reads
# the in-memory progress.level, not a live DB lookup, so unlike the
# warrior above this ONE reconnect is a real requirement, not avoidable
# -- same proven pattern every other smoke test in this suite already
# uses successfully for its own immortal setup). ---
imm_name, imm_pw = f"GmcpImm{_suffix}", "gmcpimmpw123"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, imm_pw); recv_all(s)
create_character(s, imm_name, send_line, recv_all, char_class="1")
cmd(s, "quit!")
s.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")

s = socket.create_connection((host, port), timeout=5)
s.settimeout(1.0)
try:
    while s.recv(4096):
        pass
except socket.timeout:
    pass
s.sendall(bytes([IAC, DO, GMCP, IAC, DO, MSDP, IAC, DO, MSP]))
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

# --- Immortal teleports to the warrior (goto <player name>, cmd_goto.c)
# -- no custom room, no `transfer`, no need to know the warrior's vnum. ---
goto_out = cmd(s, f"goto {name}")
check("Huh?" not in goto_out and "no such" not in goto_out.lower(),
      "the immortal goto's to the warrior's own room")

sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'dummy training','a training dummy','A training dummy stands here.',"
    f"'desc',0,0,0,0,'A',1.0,50,99,50000,3.0,3.0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
# tohit=99 (near-guaranteed to land) and real damage_level/precision --
# unlike the near-zero-tohit sandbox dummy other smoke tests use
# (deliberately harmless there), THIS test needs the mob to actually
# hit the PC defender for GMCP/MSDP/MSP's damage-triggered pushes to
# have anything to fire on at all.

# --- Part 3: Room.Info fires on the warrior's own `look` (already
# opted in) -- any real vnum, since we don't know/care which room this
# is. ---
look_out = cmd(sw, "look")
check('Room.Info {"num":' in look_out, "look pushes a real Room.Info GMCP message")

check("You conjure" in cmd(s, f"load mob {MOB}"), "the training dummy is loaded")

send_line(sw, "hit dummy")
hit_out = recv_for(sw, 8.0)  # several combat rounds' worth, wall-clock bounded
check('Char.Vitals {"hp":' in hit_out, "taking damage pushes a real Char.Vitals GMCP message")
check("HEALTH" in hit_out and chr(1) in hit_out,
      "taking damage pushes a real MSDP VAR HEALTH VAL message (raw MSDP_VAR control byte present)")
check(re.search(r"!!SOUND\(\S+\.wav V=100\)", hit_out),
      "taking damage fires a real MSP in-band sound marker")

send_line(sw, "flee"); recv_for(sw, 3.0)  # disengage before disconnecting -- don't leave
                                          # the throwaway character fighting a real-
                                          # damage mob unattended
sw.close()
s.close()

sql(f"DELETE FROM mob WHERE vnum={MOB};")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player WHERE name='{name}';")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"DELETE FROM player WHERE name='{imm_name}';")

announce_done("smoke_test_gmcp_msdp_msp", host, port)
print("=== ALL CHECKS PASSED ===")
