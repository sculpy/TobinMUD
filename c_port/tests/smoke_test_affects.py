#!/usr/bin/env python3
"""Smoke test for the Affects system (buffs/debuffs/status) -- user
2026-07-11's backlog item, self-sequenced after trap mechanics. New
generic active_affect_t infrastructure (affect.h/affect.c), proven with
one real flagship effect: the Cleric spell "sanctuary" ("a strong aura
that reduces incoming damage") now actually halves damage taken via
combat_strike(), instead of falling into the placeholder "isn't
implemented yet" branch every other still-unimplemented spell hits.
Covers:

  1. A fresh character's `affects` shows "(none)".
  2. `pray sanctuary` applies it -- `affects` then shows "Sanctuary"
     with a round count.
  3. Average incoming damage over real hits is meaningfully lower with
     Sanctuary active than without (statistical, immortal attacker so
     the mortal 1.2s post-swing cooldown doesn't slow this down).
  4. Sanctuary wears off on its own after enough rounds, reporting
     "Your Sanctuary wears off." and leaving `affects` empty again.

    python3 tests/smoke_test_affects.py [host] [port]
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


announce("smoke_test_affects")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000) % 60000)
SYMBOL = ROOM + 1
SAMPLES = 20


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


def set_hp(name, hp):
    # Mutual combat (combat_process_run() strikes both ways every round)
    # means BOTH sides need enough overall HP -- and, since a fresh
    # player_load() now recomputes limb HP as a share of max_hp (fixed
    # 2026-07-12, player_repo.c), a big set_hp here also gives each limb
    # real toughness once the character reconnects (quit!-then-relogin,
    # not a raw socket close -- see smoke_test_weapon_depth.py's set_hp).
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_dex(name, dex):
    # A huge DEX gap clamps the OTHER side's to-hit modifier to its -44
    # floor (~6% hit chance) -- used here so the Cleric's automatic
    # retaliation essentially never lands on the immortal attacker,
    # keeping the attacker alive for the whole sampling window.
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def damages_from(text):
    # Only the ATTACKER's own outgoing hit lines start with "You" --
    # the target's own connection sees "X hits your Y for N damage!"
    # instead, which is what we actually want to measure here (damage
    # taken, not dealt).
    dmgs = []
    for line in text.splitlines():
        m = re.search(r"for (\d+) damage", line)
        if m and line.startswith("You "):
            continue
        if m:
            dmgs.append(int(m.group(1)))
    return dmgs


def average_incoming(attacker_sock, target_sock, target_name, n):
    # Returns the raw text seen too -- Sanctuary's 12-round duration can
    # expire mid-sampling (20 hits can take longer than 12 rounds), and
    # its "wears off" line would otherwise be silently consumed here
    # before a caller's own later poll ever sees it.
    cmd(attacker_sock, f"hit {target_name}")
    dmgs = []
    seen = ""
    polls = 0
    while len(dmgs) < n and polls < n * 3:
        polls += 1
        out = recv_all(target_sock, 1.5)
        seen += out
        dmgs.extend(damages_from(out))
    check(len(dmgs) >= n, f"collected at least {n} incoming hits on {target_name} ({len(dmgs)} got)")
    return sum(dmgs) / len(dmgs), seen


pw = "affectspw123"

# --- Cleric immortal (casts sanctuary on the target, and separately
#     stands in as the attacker so both sides bypass mortal wait
#     states -- neither's own class/level matters for the attacker
#     role, only for who casts sanctuary). ---
cleric_name = f"Affcle{_suffix}"
cleric_pw = "affclepw123"
s_cle = socket.create_connection((host, port), timeout=5)
recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, "y"); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, "new"); recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, "done"); recv_all(s_cle)
send_line(s_cle, "1"); recv_all(s_cle)
send_line(s_cle, "2"); recv_all(s_cle)  # class: cleric
send_line(s_cle, "2"); recv_all(s_cle)
set_level(cleric_name, 51)  # immortal -- bypasses sanctuary's Advanced-tier discipline gate
set_hp(cleric_name, 2000)   # survive ~40-60 rounds of mutual combat unscathed
cmd(s_cle, "quit!")  # a raw socket close reconnects to the still-linkdead
                      # live being_t (skipping player_load() entirely) --
                      # quit! frees it so the next login does a real DB load.
s_cle.close()
s_cle = socket.create_connection((host, port), timeout=5)
recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, "1"); recv_all(s_cle)
cmd(s_cle, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Affects Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Affects Sandbox" in cmd(s_cle, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")
check("You conjure" in cmd(s_cle, f"load obj {SYMBOL}"), "the holy symbol is loaded")
out = cmd(s_cle, "get symbol")
check("you get" in out.lower(), "the Cleric picks up the holy symbol")

# --- 1: a fresh character's affects list is empty ---
out = cmd(s_cle, "affects")
check("(none)" in out, "affects shows (none) before casting anything")

# --- Attacker immortal, transferred into the sandbox ---
imm_name = f"Affimm{_suffix}"
imm_pw = "affimmpw123"
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
send_line(s_imm, "3"); recv_all(s_imm)  # class: warrior
send_line(s_imm, "2"); recv_all(s_imm)
set_level(imm_name, 51)
set_hp(imm_name, 2000)
set_dex(imm_name, 900)  # the Cleric's automatic retaliation should essentially never land
cmd(s_imm, "quit!")
s_imm.close()
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")
cmd(s_cle, f"transfer {imm_name}")
check("Affects Sandbox" in cmd(s_imm, "look"), "the attacker immortal is in the sandbox")

# --- 3a: baseline incoming damage average, no Sanctuary yet ---
baseline_avg, _ = average_incoming(s_imm, s_cle, cleric_name, SAMPLES)

# --- 2: pray sanctuary applies the affect ---
out = cmd(s_cle, "pray sanctuary")
check("shimmering aura surrounds you" in out, "pray sanctuary applies the affect")
out = cmd(s_cle, "affects")
check("Sanctuary" in out, "affects now lists Sanctuary")
m = re.search(r"Sanctuary\s+(\d+) round", out)
check(m is not None and int(m.group(1)) > 0, "affects shows a positive round count for Sanctuary")

# --- 3b: incoming damage average drops noticeably with Sanctuary active ---
sanctuary_avg, seen_during_sampling = average_incoming(s_imm, s_cle, cleric_name, SAMPLES)
check(sanctuary_avg < baseline_avg - 0.4,
      f"Sanctuary's damage reduction is clearly visible ({baseline_avg:.2f} -> {sanctuary_avg:.2f})")

# --- 4: Sanctuary wears off on its own -- its 12-round duration can
#     expire mid-sampling above, so check the accumulated sampling text
#     too, not just a fresh poll after the fact. ---
out = seen_during_sampling
waited = 0
while "wears off" not in out and waited < 20:
    out += recv_all(s_cle, 1.5)
    waited += 1
check("Your Sanctuary wears off" in out, "Sanctuary reports wearing off on its own")
out = cmd(s_cle, "affects")
check("(none)" in out, "affects is empty again after Sanctuary expires")

s_cle.close()
s_imm.close()
announce_done("smoke_test_affects")
print("=== ALL CHECKS PASSED ===")
