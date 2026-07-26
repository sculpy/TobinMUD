#!/usr/bin/env python3
"""Smoke test for "Drug tracking" (Sneezy -> Tobin feature audit).
Checked the real upstream first (docs/systems/informational/
drug-tracking.md): consumption applies a real temporary stat effect,
tracked for addiction (lifetime average rate) and withdrawal (a real
penalty once overdue). Tobin's simplified 6-stat system has no BRA/AGI/
FOC/SPE/PER/KAR (the original's own drug effects use those), so every
effect is a deliberate remap onto STR/DEX/CON/INT/WIS/CHA -- see drug.h/
drug.c's own header comments for the exact mapping and the deliberately-
NOT-ported original opium bug / frogslime garble effect. Covers:

  1. Smoking pipeweed as a non-Hobbit applies a real multi-stat penalty.
  2. A second dose consolidates rather than stacking (still just one
     dose's worth of penalty, not double).
  3. Forcing `aitick` past the dose's real duration reverts the penalty.
  4. A Hobbit gets a genuine bonus instead of the penalty.
  5. A low-charge item is destroyed once its last charge is spent.
  6. Withdrawal: a player with enough lifetime consumption and a
     last-use timestamp far enough in the past gets a real penalty the
     next tick, entirely without waiting -- last_use is SQL-set directly
     before login, so the real-time comparison is genuine, not faked.

    python3 tests/smoke_test_drugs.py [host] [port]
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


announce("smoke_test_drugs")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 950000 + (int(time.time()) % 20000)
PIPEWEED_VNUM = ROOM + 1
LOWCHARGE_VNUM = ROOM + 2

RACE_HOBBIT = 4


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


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "done", "done"):
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


def stats_from_score(out):
    d = {}
    for label, key in (("Strength", "str"), ("Dexterity", "dex"), ("Constitution", "con"),
                       ("Intelligence", "int"), ("Wisdom", "wis"), ("Charisma", "cha")):
        m = re.search(rf"{label}:\s+(-?\d+)", out)
        d[key] = int(m.group(1)) if m else None
    return d


imm_name, imm_pw = f"Drgimm{_suffix}", "drugimmpw123"
a_name, a_pw = f"Drga{_suffix}", "drugapw12345"     # non-Hobbit smoker
h_name, h_pw = f"Drgh{_suffix}", "drughpw12345"      # Hobbit
w_name, w_pw = f"Drgw{_suffix}", "drugwpw12345"      # withdrawal case

sockets = []

try:
    make_char(imm_name, imm_pw)
    sql(f"UPDATE player_progress SET level=59 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")

    make_char(a_name, a_pw)
    make_char(h_name, h_pw)
    sql(f"UPDATE player SET race={RACE_HOBBIT} WHERE name='{h_name}';")
    make_char(w_name, w_pw)
    # Withdrawal case: enough lifetime consumption at a real average
    # rate above pipeweed's own 2.0/hour addiction threshold, and a
    # last_use timestamp already 30 real mud-hours (30*240=7200s) in the
    # past -- comfortably past pipeweed's 24-hour withdrawal onset.
    # total_consumed=100 over hours_since_first=31 -> rate ~= 3.23/hour,
    # comfortably clearing the 2.0/hour bar (60 only cleared ~1.94/hour --
    # too close to the threshold, verified against a live run).
    now = int(time.time())
    sql(f"INSERT INTO player_drug (player_id, drug_type, first_use, last_use, total_consumed) "
        f"VALUES ((SELECT id FROM player WHERE name='{w_name}'), 0, {now - 7440}, {now - 7200}, 100) "
        f"ON DUPLICATE KEY UPDATE first_use=first_use;")

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Drug Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,val2,can_be_seen,decay) "
        f"VALUES ({PIPEWEED_VNUM},'pouch pipeweed drug','a pouch of pipeweed',"
        f"'A pouch of pipeweed is here.',56,1,0,10,10,1,-1);")
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,val2,can_be_seen,decay) "
        f"VALUES ({LOWCHARGE_VNUM},'vial frogslime drug','a vial of frogslime',"
        f"'A vial of frogslime is here.',56,1,3,1,1,1,-1);")

    imm = login(imm_name, imm_pw); sockets.append(imm)
    a = login(a_name, a_pw); sockets.append(a)
    h = login(h_name, h_pw); sockets.append(h)

    check("Drug Sandbox" in cmd(imm, f"goto {ROOM}"), "goto lands in the sandbox room")
    cmd(imm, f"transfer {a_name} {ROOM}")
    cmd(imm, f"transfer {h_name} {ROOM}")
    for sock in (imm, a, h):
        recv_all(sock)

    # --- 1: smoking pipeweed as a non-Hobbit applies a real penalty ---
    before = stats_from_score(cmd(a, "score"))
    cmd(imm, f"load obj {PIPEWEED_VNUM}")
    cmd(imm, "drop pouch")
    cmd(a, "get pouch")
    out = cmd(a, "smoke pouch")
    check("pipeweed" in out.lower(), "smoke confirms the pipeweed dose")
    after = stats_from_score(cmd(a, "score"))
    check(after["dex"] == before["dex"] - 2 and after["int"] == before["int"] - 2
          and after["wis"] == before["wis"] - 2 and after["cha"] == before["cha"] - 2,
          "a non-Hobbit smoker takes a real DEX/INT/WIS/CHA penalty")

    # --- 2: a second dose consolidates, doesn't stack ---
    cmd(a, "smoke pouch")
    after2 = stats_from_score(cmd(a, "score"))
    check(after2["dex"] == after["dex"] and after2["int"] == after["int"],
          "a second dose consolidates instead of stacking a second penalty")

    # --- 3: forcing aitick past the dose's real duration reverts it ---
    cmd(imm, "aitick 5")
    reverted = stats_from_score(cmd(a, "score"))
    check(reverted["dex"] == before["dex"] and reverted["int"] == before["int"]
          and reverted["wis"] == before["wis"] and reverted["cha"] == before["cha"],
          "the penalty reverts once the dose's real duration expires")

    # --- 4: a Hobbit gets a genuine bonus instead ---
    h_before = stats_from_score(cmd(h, "score"))
    cmd(imm, f"load obj {PIPEWEED_VNUM}")
    cmd(imm, "drop pouch")
    cmd(h, "get pouch")
    cmd(h, "smoke pouch")
    h_after = stats_from_score(cmd(h, "score"))
    check(h_after["int"] == h_before["int"] + 9, "a Hobbit smoker gets a real INT bonus instead of a penalty")

    # --- 5: a low-charge item is destroyed on its last dose ---
    cmd(imm, f"load obj {LOWCHARGE_VNUM}")
    cmd(imm, "drop vial")
    cmd(a, "get vial")
    out = cmd(a, "smoke vial")
    check("spent" in out.lower(), "the last charge destroys the item, with a real message")
    check("vial" not in cmd(a, "inventory").lower(), "the spent item is actually gone from inventory")

    # --- 6: withdrawal fires for an overdue, sufficiently-addicted player ---
    w = login(w_name, w_pw); sockets.append(w)
    cmd(imm, f"transfer {w_name} {ROOM}")
    recv_all(w)
    w_before = stats_from_score(cmd(w, "score"))
    out = cmd(imm, "aitick 1")
    out_w = recv_all(w, 1.0)
    check("withdrawal" in out_w.lower(), "an overdue, addicted player gets a real withdrawal notice")
    w_after = stats_from_score(cmd(w, "score"))
    check(w_after["str"] < w_before["str"] or w_after["con"] < w_before["con"],
          "withdrawal actually lowers STR/CON, not just a cosmetic message")

    for sock in sockets:
        sock.close()
    sockets = []

    announce_done("smoke_test_drugs")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name IN "
        f"('{imm_name}','{a_name}','{h_name}','{w_name}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name IN "
        f"('{imm_name}','{a_name}','{h_name}','{w_name}'));")
    sql(f"DELETE FROM player_drug WHERE player_id IN (SELECT id FROM player WHERE name IN "
        f"('{imm_name}','{a_name}','{h_name}','{w_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}','{a_name}','{h_name}','{w_name}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum IN ({PIPEWEED_VNUM}, {LOWCHARGE_VNUM});")
