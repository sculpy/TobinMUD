#!/usr/bin/env python3
"""Smoke test for "Object maintenance" (Sneezy -> Tobin feature audit,
decay timers + combat-driven structure/durability -- the repair-shop
economy and per-class repair skills are separate, later tasks of this
same audit item and aren't covered here). Covers:

  1. Corpse decay: killing a mob leaves a corpse on the room floor;
     `aitick` past CORPSE_DECAY_TICKS (15) makes it decay away on its
     own, same real-tick-forcing convention as check_decay.py's earlier
     manual verification.
  2. Equipment structure damage: a fragile worn item (max_struct=5,
     cur_struct=1) eventually gets destroyed by ordinary melee combat
     landing on the limb it's worn on (combat_maybe_damage_equipment(),
     combat.c) -- the destroy message fires, the equipment slot clears,
     the item's stat/AC affects are reversed, and a "scraps of X" object
     is left behind in the room. Regression check for a test-design trap
     found live: only the DEFENDER's gear is ever damaged, and immortals
     always take dmg=0, so the item must be worn by a mortal TARGET, not
     the immortal attacker.

    python3 tests/smoke_test_object_maintenance.py [host] [port]
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


announce("smoke_test_object_maintenance")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 940000 + (int(time.time()) % 30000)
SHIRT_VNUM = ROOM + 1
CAP_VNUM = ROOM + 2
DUMMY_VNUM = ROOM + 3


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


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", class_choice, "done", "2"):
        send_line(s, step); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Objimma{_suffix}"
tgt_name = f"Objtgta{_suffix}"
pw = "objmaintpw123"
dkw = f"objdum{_suffix}"

try:
    make_char(imm_name, pw, "1")   # class doesn't matter, immortal bypasses class gates
    make_char(tgt_name, pw, "3")
    sql(f"UPDATE player_progress SET level=59 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")
    # Huge HP so combat runs long enough to sample several hits without the
    # target dying outright; dex=1 so the immortal (dex=900) lands often.
    sql(f"UPDATE player_progress SET hp=999999, max_hp=999999 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{tgt_name}');")
    sql(f"UPDATE player_attrs SET dexterity=900 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")
    sql(f"UPDATE player_attrs SET dexterity=1 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{tgt_name}');")

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Object Maintenance Sandbox','A bare sandbox room.\\n',"
        f"NULL,1,0,0,0,0,0,0,0,0,0);")

    # Two fragile items on two different limbs (body + head, wear_flag 9/25 =
    # TAKE|BODY and TAKE|HEAD) instead of just one -- roughly doubles the
    # chance that any given landed hit is on a geared limb, keeping the test
    # from needing an excessive number of real combat rounds.
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,"
        f"can_be_seen,max_struct,cur_struct,decay) VALUES "
        f"({SHIRT_VNUM},'shirt fragile test','a fragile test shirt',"
        f"'A fragile test shirt is lying here.',11,9,1,5,1,-1);")
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,"
        f"can_be_seen,max_struct,cur_struct,decay) VALUES "
        f"({CAP_VNUM},'cap fragile test','a fragile test cap',"
        f"'A fragile test cap is lying here.',11,25,1,5,1,-1);")

    imm = login(imm_name, pw)
    tgt = login(tgt_name, pw)

    check("Object Maintenance Sandbox" in cmd(imm, f"goto {ROOM}"),
          "goto lands in the sandbox room")
    cmd(imm, f"transfer {tgt_name} {ROOM}")
    recv_all(tgt); recv_all(imm)  # drain arrival notices

    # --- 1: corpse decay ---
    cols = {
        "vnum": DUMMY_VNUM, "name": f"'{dkw}'", "short_desc": f"'a {dkw}'",
        "long_desc": f"'A {dkw} stands here, unmoving.'", "description": "'desc'",
        "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
        "letter": "'A'", "attacks": 1.0,
        "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 0,
        "damage_level": 0, "damage_precision": 0, "gold": 5, "race": 0,
        "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
        "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
        "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
        "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
        "max_exist": 1,
    }
    col_names = ",".join(cols.keys())
    col_values = ",".join(str(v) for v in cols.values())
    sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")

    cmd(imm, f"load mob {DUMMY_VNUM}")
    out = cmd(imm, f"kill {dkw}")
    check("corpse" in cmd(imm, "look").lower(),
          "a killed mob leaves a corpse on the room floor")
    cmd(imm, "aitick 16")   # forces past CORPSE_DECAY_TICKS=15
    look_after = cmd(imm, "look")
    check("corpse" not in look_after.lower(),
          "the corpse decays away on its own once its countdown reaches 0")

    # --- 2: equipment structure damage ---
    # `load` now puts the item straight into the loading immortal's own
    # inventory (2026-07-22), not the room floor -- drop it explicitly so
    # `tgt` (a different character) can pick it up.
    cmd(imm, f"load obj {SHIRT_VNUM}")
    cmd(imm, "drop shirt")
    cmd(tgt, "get shirt"); cmd(tgt, "wear shirt")
    cmd(imm, f"load obj {CAP_VNUM}")
    cmd(imm, "drop cap")
    cmd(tgt, "get cap"); cmd(tgt, "wear cap")
    eq_before = cmd(tgt, "equipment")
    check("a fragile test shirt" in eq_before and "a fragile test cap" in eq_before,
          "both fragile test items are worn before combat starts")

    cmd(imm, f"hit {tgt_name}")
    start = time.time()
    destroyed = False
    out_all = ""
    while time.time() - start < 90 and not destroyed:
        out_all += recv_all(imm, 1.5)
        out_all += recv_all(tgt, 0.3)
        if "is destroyed" in out_all:
            destroyed = True
    check(destroyed,
          "ordinary melee combat eventually destroys a fragile worn item "
          "(combat_maybe_damage_equipment(), 30% chance per landed hit on "
          "a geared limb)")

    eq_after = cmd(tgt, "equipment")
    check("a fragile test shirt" not in eq_after or "a fragile test cap" not in eq_after,
          "the destroyed item's equipment slot is actually cleared, not just cosmetically broken")

    # Both `imm` and `tgt` are still mid-fight when this runs (the automatic
    # per-round exchange keeps going in the background), so their own
    # sockets are constantly interleaving fresh combat spam with whatever
    # `look` response we're trying to read -- the exact test-harness quirk
    # STATUS.md's Session 55 write-up already found and worked around
    # ("Confirming the destroy actually fired required a fresh spectator
    # connection... rather than trusting the original test sockets' own
    # captured output"). A second immortal-level connection (immortals are
    # exempt from the one-character-at-a-time multiplay gate) sidesteps it
    # entirely -- it never sees any combat traffic in the first place.
    spectator = login(imm_name, pw)
    cmd(spectator, f"goto {ROOM}")
    look_after2 = cmd(spectator, "look")
    check("scraps of" in look_after2.lower(),
          "a scrap object is left behind in the room after destruction")
    spectator.close()

    imm.close()
    tgt.close()

    announce_done("smoke_test_object_maintenance")
    print("=== ALL CHECKS PASSED ===")
finally:
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{tgt_name}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{tgt_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{tgt_name}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum IN ({SHIRT_VNUM}, {CAP_VNUM});")
    sql(f"DELETE FROM mob WHERE vnum={DUMMY_VNUM};")
