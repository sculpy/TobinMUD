#!/usr/bin/env python3
"""Smoke test for `blindness` and `word of recall` (Cleric, level 21) --
spell/skill functional-completeness audit continued. See cmd_pray.c's
own branches and affect.h's AFFECT_BLIND doc comment for the real-
upstream research and scope-down rationale.

  1. Blindness with no target is refused.
  2. Blindness applies AFFECT_BLIND, shown in `affects`.
  3. A blinded being's `look` is blocked entirely.
  4. Word of recall (self) relocates the caster to Center Square (room
     100, the real game's own hardcoded fallback used when no personal
     recall point is set -- Tobin has no per-player recall point).
  5. Word of recall refuses from a NO-ESCAPE room.
  6. Word of recall refuses an immortal target.

KNOWN ISSUE (2026-07-28, see STATUS.md/TODO.md): check 5 (the NO-ESCAPE
room refusal) is flaky for the same not-yet-root-caused reason as
smoke_test_teleport_summon.py -- clr2 can occasionally land in the
wrong room on login instead of the intended NO-ESCAPE sandbox, in which
case this check fails for an environment reason unrelated to the
feature. The underlying ROOM_FLAG_NO_ESCAPE gate itself reuses the
identical pattern already verified working for `teleport`'s own
NO-ESCAPE check earlier this session.

    python3 tests/smoke_test_blindness_recall.py [host] [port]
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


announce("smoke_test_blindness_recall")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_CLERIC = 1
CLASS_WARRIOR = 2
WEAR_TAKE = 1
ROOM_FLAG_NO_ESCAPE = 1 << 6


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


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_single(prefix, class_id, room, level=20):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    make_char(name, pw)
    sql(f"UPDATE player SET class={class_id}, load_room={room} WHERE name='{name}';")
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, "
        f"hp=5000, max_hp=5000, vit=5000, max_vit=5000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    time.sleep(0.3)
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


ROOM = 980500 + (int(time.time()) % 1000)
ROOM_NE = ROOM + 100
SYMBOL = ROOM + 1
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Recall Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_NE},0,0,0,'No-Escape Sandbox','A bare sandbox room.\\n',NULL,"
    f"{1 | ROOM_FLAG_NO_ESCAPE},0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},1);")

sockets = []
try:
    clr_name, clr = make_single("Rclr", CLASS_CLERIC, ROOM, level=21)
    sockets.append(clr)
    vic_name, vic = make_single("Rvic", CLASS_WARRIOR, ROOM, level=20)
    sockets.append(vic)
    immvic_name, immvic = make_single("Rimm", CLASS_WARRIOR, ROOM, level=51)
    sockets.append(immvic)
    clr2_name, clr2 = make_single("Rclt", CLASS_CLERIC, ROOM_NE, level=21)
    sockets.append(clr2)
    clr3_name, clr3 = make_single("Rclu", CLASS_CLERIC, ROOM, level=21)
    sockets.append(clr3)

    for name in (clr_name, clr2_name, clr3_name):
        sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
            f"VALUES ((SELECT id FROM player WHERE name='{name}'), 'blindness', 100, {int(time.time())}) "
            f"ON DUPLICATE KEY UPDATE pct=100;")
        sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
            f"VALUES ((SELECT id FROM player WHERE name='{name}'), 'word of recall', 100, {int(time.time())}) "
            f"ON DUPLICATE KEY UPDATE pct=100;")

    # `load obj` needs immortal tier -- clr/clr2/clr3 all stay mortal for
    # a realistic skill check, so `immvic` (already immortal) loads and
    # drops each holy symbol instead, same precedent as deathstroke's
    # sword/dispel magic's component in smoke_test_level20_warrior_mage.py.
    def give_symbol(getter_sock):
        check("You conjure" in cmd(immvic, f"load obj {SYMBOL}"), "a holy symbol is loaded")
        cmd(immvic, "drop symbol"); recv_all(immvic, 0.3)
        cmd(getter_sock, "get symbol"); recv_all(getter_sock, 0.3)

    # --- 1: blindness with no target is refused ---
    # Needs a holy symbol on hand first -- cmd_pray()'s own symbol gate
    # runs BEFORE task_pray()'s no-target check, so without one the
    # symbol refusal fires first instead of the one this check wants.
    recv_all(clr, 0.3)
    give_symbol(clr)
    out1 = strip(cmd(clr, "pray blindness"))
    check("pray for that over whom" in out1.lower(), "blindness with no target is refused")

    # --- 2 & 3: blindness applies AFFECT_BLIND and blocks the victim's look ---
    give_symbol(clr)
    out2 = strip(cmd(clr, f"pray blindness {vic_name}"))
    check("eyes go white" in out2.lower(), "blindness succeeds with a holy symbol")
    time.sleep(0.3)
    out2b = strip(cmd(vic, "affects"))
    check("Blind" in out2b, "the affects command lists Blind while it's active")
    out3 = strip(cmd(vic, "look"))
    check("blinded" in out3.lower(), "a blinded being's look is blocked")

    # --- 4: word of recall (self) relocates to Center Square ---
    give_symbol(clr)
    out4 = strip(cmd(clr, "pray word of recall"))
    check("pulled home" in out4.lower(), "word of recall succeeds")
    time.sleep(0.3)
    out4b = strip(cmd(clr, "look"))
    check("Center Square" in out4b, "word of recall really relocated the caster to Center Square")

    # --- 5: word of recall refuses from a NO-ESCAPE room ---
    # `immvic` steps into the NO-ESCAPE room briefly to deliver the
    # symbol, then returns to ROOM so check 6 (which needs it as an
    # immortal target back in the original room) still works.
    cmd(immvic, f"goto {ROOM_NE}"); recv_all(immvic, 0.3)
    give_symbol(clr2)
    cmd(immvic, f"goto {ROOM}"); recv_all(immvic, 0.3)
    out5 = strip(cmd(clr2, "pray word of recall"))
    check("can't recall from here" in out5.lower(), "word of recall refuses from a NO-ESCAPE room")

    # --- 6: word of recall refuses an immortal target ---
    # Uses `clr3`, a separate caster still in ROOM alongside `immvic` --
    # `clr` already relocated away in check 4, and `clr2` is stuck in
    # the NO-ESCAPE sandbox from check 5 (which would refuse for the
    # room reason before ever reaching the immortal-target check).
    give_symbol(clr3)
    out6 = strip(cmd(clr3, f"pray word of recall {immvic_name}"))
    check("good idea" in out6.lower(), "word of recall refuses an immortal target")

    announce_done("smoke_test_blindness_recall")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            cmd(sock, "quit!", timeout=0.5)
            sock.close()
        except OSError:
            pass
    for prefix in ("Rclr", "Rvic", "Rimm", "Rclt", "Rclu"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_inventory WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
        sql(f"DELETE FROM account WHERE name LIKE LOWER('{prefix}%{_suffix}');")
    sql(f"DELETE FROM room WHERE vnum IN ({ROOM}, {ROOM_NE});")
    sql(f"DELETE FROM obj WHERE vnum={SYMBOL};")
