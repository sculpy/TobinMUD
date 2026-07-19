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
    # Damage numbers are hidden from a plain mortal viewer entirely
    # (user 2026-07-12) -- only an immortal still sees them, and only in
    # their OWN outgoing "You <verb> X's <limb> for N damage!" line. So
    # this measures damage dealt (from the immortal ATTACKER's own
    # connection), not damage taken from the target's -- same pattern
    # smoke_test_weapon_depth.py already uses for the same reason.
    dmgs = []
    for line in text.splitlines():
        if not line.startswith("You "):
            continue
        m = re.search(r"for (\d+) damage", line)
        if m:
            dmgs.append(int(m.group(1)))
    return dmgs


def average_incoming(attacker_sock, target_sock, target_name, n):
    # Returns the raw text seen (on the TARGET's own connection) too --
    # Sanctuary's 12-round duration can expire mid-sampling (20 hits can
    # take longer than 12 rounds), and its "wears off" line would
    # otherwise be silently consumed before a caller's own later poll
    # ever sees it. The damage numbers themselves are read from the
    # ATTACKER's connection instead (see damages_from()).
    cmd(attacker_sock, f"hit {target_name}")
    dmgs = []
    seen = ""
    polls = 0
    while len(dmgs) < n and polls < n * 3:
        polls += 1
        attacker_out = recv_all(attacker_sock, 1.5)
        seen += recv_all(target_sock, 0.1)
        dmgs.extend(damages_from(attacker_out))
    check(len(dmgs) >= n, f"collected at least {n} incoming hits on {target_name} ({len(dmgs)} got)")
    return sum(dmgs) / len(dmgs), seen


pw = "affectspw123"

# --- Attacker immortal, set up first (immortals can `goto`/`load obj`;
#     also gets the immortal damage-immunity treatment, 2026-07-12: "an
#     immortal character shouldnt be damaged by hits in a fight" --
#     combat_strike() now zeroes damage against an immortal DEFENDER, so
#     the Cleric below (whose incoming damage this test measures) has to
#     be an ordinary MORTAL instead of immortal like earlier revisions of
#     this test used; a huge set_dex() keeps their retaliation from ever
#     landing on the attacker anyway, and now any retaliation that DID
#     land would deal zero damage regardless, since the attacker here is
#     immortal). ---
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
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "3"); recv_all(s_imm)  # class: warrior
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "2"); recv_all(s_imm)
cmd(s_imm, "quit!")
s_imm.close()
set_level(imm_name, 51)
set_hp(imm_name, 2000)
set_dex(imm_name, 900)  # the Cleric's automatic retaliation should essentially never land
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Affects Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Affects Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")
check("You conjure" in cmd(s_imm, f"load obj {SYMBOL}"), "the holy symbol is loaded")

# --- Cleric MORTAL (a real defender now that immortals take zero damage)
#     -- basic_disc_pct/advanced_disc_pct set directly via SQL to satisfy
#     sanctuary's Advanced-tier discipline gate without immortal status;
#     level bumped to 25 too (2026-07-18 level-curve rescale: Advanced-
#     tier min_level now spans 25-50 per class, not universally 1). ---
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
send_line(s_cle, "1"); recv_all(s_cle)
send_line(s_cle, "2"); recv_all(s_cle)  # class: cleric
send_line(s_cle, "done"); recv_all(s_cle)
send_line(s_cle, "2"); recv_all(s_cle)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
cmd(s_cle, "quit!")
s_cle.close()
sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100, advanced_disc_pct=50 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{cleric_name}');")
set_level(cleric_name, 25)
set_hp(cleric_name, 8000)  # survive a long mutual-combat sampling window even
                            # if a few unlucky retaliation hits concentrate on
                            # one limb (Tobin's death check is per-limb, not
                            # the aggregate pool -- 2000 occasionally wasn't
                            # enough headroom, live-observed 2026-07-18)
s_cle = socket.create_connection((host, port), timeout=5)
recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, "1"); recv_all(s_cle)
cmd(s_cle, "color off")
check("Affects Sandbox" in cmd(s_cle, "look"), "the Cleric lands in the sandbox room")

out = cmd(s_cle, "get symbol")
check("you get" in out.lower(), "the Cleric picks up the holy symbol")

# --- 1: a fresh character's affects list is empty ---
out = cmd(s_cle, "affects")
check("(none)" in out, "affects shows (none) before casting anything")

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
