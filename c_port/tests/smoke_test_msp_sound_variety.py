#!/usr/bin/env python3
"""Smoke test for pick_hit_sound() (combat.c): class-specific MSP sound
pools override the generic weapon-type pool. The sound is sent to the
DEFENDER's client but the pool is chosen by the ATTACKER's class, so
this has the immortal (created as class "1" = Mage) attack a Cleric
directly (immortal-vs-mortal PvP; landing a hit on an immortal deals 0
damage, but an immortal attacker still deals real damage to a mortal
defender -- combat.c's own immortal-damage-immunity comment) and
checks the CLERIC's own received output for one of the Mage spell
pool's three files. No mob/dummy needed. Same proven-safe pattern as
smoke_test_gmcp_msdp_msp.py (single continuous connection per
character, no reconnect, goto <player> instead of a custom sandbox)
-- see that file's own module docstring for why.

    python3 tests/smoke_test_msp_sound_variety.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

IAC, WILL, DO, SB, SE = 255, 251, 253, 250, 240
MSP = 90
SPELL_POOL = ("spell.wav", "spell2.wav", "spell_fireball.wav")


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


announce("smoke_test_msp_sound_variety", host, port)

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
name = f"Snd{_suffix}"
pw = "sndtestpw123"
recv_all(sw)
send_line(sw, name); recv_all(sw)
send_line(sw, "y"); recv_all(sw)
send_line(sw, pw); recv_all(sw)
send_line(sw, pw); recv_all(sw)
create_character(sw, name, send_line, recv_all, char_class="2")  # cleric
cmd(sw, "color off")
sql(f"UPDATE player_progress SET level=10 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")

imm_name, imm_pw = f"SndImm{_suffix}", "sndimmpw123"
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
s.sendall(bytes([IAC, DO, MSP]))
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

goto_out = cmd(s, f"goto {name}")
check("no such" not in goto_out.lower(), "the immortal goto's to the cleric's own room")

# --- Immortal (Mage class) hits the cleric directly. Damage still
# lands normally: an immortal ATTACKER isn't damage-immune, only an
# immortal DEFENDER is (combat.c's own comment) -- so this is real
# damage on the cleric, real MSP push to the cleric's own connection,
# picked from the ATTACKER's (immortal/Mage) class pool. ---
send_line(s, f"hit {name}")
hit_out = recv_for(sw, 4.0)  # read from the CLERIC's connection, not the immortal's
found = any(f"!!SOUND({t} V=100)" in hit_out for t in SPELL_POOL)
check(found, "a Mage attacker's hit sound is one of the class-specific spell pool files, not generic hit.wav")

send_line(s, "flee"); recv_for(s, 2.0)
sw.close()
s.close()

sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player WHERE name='{name}';")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"DELETE FROM player WHERE name='{imm_name}';")

announce_done("smoke_test_msp_sound_variety", host, port)
print("=== ALL CHECKS PASSED ===")
