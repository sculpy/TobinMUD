#!/usr/bin/env python3
"""Smoke test for the level-20 batch of the spell/skill functional-
completeness audit: `springleap` (Monk), `slam`/`deathstroke` (Warrior),
`dispel magic` (Mage). `riposte` (Warrior, passive) is NOT live-tested
here -- it only procs after a successful `parry`, itself only a passive
chance-based check, making a deterministic repro impractical; verified
by code review instead (combat.c's riposte_ready flag/consume shape,
reusing the already-proven parry mechanic). See each cmd_*.c file's own
header comment for the real-upstream research and scope-down rationale.

  1. springleap while standing is refused ("not in position").
  2. springleap while sitting succeeds, standing you up.
  3. slam refuses an immortal target.
  4. slam succeeds and deals damage to a mortal target.
  5. deathstroke refuses without a wielded weapon.
  6. deathstroke succeeds once a weapon is wielded.
  7. dispel magic strips an active affect.
  8. dispel magic says so when there's nothing to strip.

    python3 tests/smoke_test_level20_warrior_mage.py [host] [port]
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


announce("smoke_test_level20_warrior_mage")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_MAGE = 0
CLASS_WARRIOR = 2
CLASS_MONK = 5
WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_WEAPON = 5


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
    time.sleep(0.3)  # keeps rapid back-to-back logins from tripping the
                      # separate, not-yet-root-caused room-placement bug
                      # (TODO.md, "a freshly created character can land
                      # in the wrong room")
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


ROOM = 980000 + (int(time.time()) % 1000)
SWORD = ROOM + 1
COMPONENT = ROOM + 2
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Lvl20 Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SWORD},'sword testsword','a test sword','A test sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")

sockets = []
try:
    monkA_name, monkA = make_single("Lmk", CLASS_MONK, ROOM, level=20)
    sockets.append(monkA)
    warA_name, warA = make_single("Lwa", CLASS_WARRIOR, ROOM, level=20)
    sockets.append(warA)
    warB_name, warB = make_single("Lwb", CLASS_WARRIOR, ROOM, level=20)
    sockets.append(warB)
    # A live SQL level edit doesn't reach an ALREADY-connected character's
    # in-memory being_t -- immortal status has to be set before login, so
    # this is a separate, dedicated character rather than warB itself.
    warImm_name, warImm = make_single("Lwc", CLASS_WARRIOR, ROOM, level=51)
    sockets.append(warImm)
    mageA_name, mageA = make_single("Lma", CLASS_MAGE, ROOM, level=20)
    sockets.append(mageA)

    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{monkA_name}'), 'springleap', 100, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct=100;")
    for name in (warA_name, warB_name):
        sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
            f"VALUES ((SELECT id FROM player WHERE name='{name}'), 'slam', 100, {int(time.time())}) "
            f"ON DUPLICATE KEY UPDATE pct=100;")
        sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
            f"VALUES ((SELECT id FROM player WHERE name='{name}'), 'deathstroke', 100, {int(time.time())}) "
            f"ON DUPLICATE KEY UPDATE pct=100;")
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{mageA_name}'), 'dispel magic', 100, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct=100;")
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{mageA_name}'), 'gills of flesh', 100, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct=100;")

    # --- 1: springleap while standing is refused ---
    recv_all(monkA, 0.3)
    out1 = strip(cmd(monkA, "springleap"))
    check("not in position" in out1.lower(), "springleap while standing is refused")

    # --- 2: springleap while sitting succeeds ---
    cmd(monkA, "sit"); recv_all(monkA, 0.3)
    out2 = strip(cmd(monkA, "springleap"))
    check("spring off the ground" in out2.lower(), "springleap while sitting succeeds")
    time.sleep(1.3)  # wait out springleap's own being_set_wait() lag
    out2b = strip(cmd(monkA, "score"))
    check("Standing" in out2b, "springleap really stood the caster up")

    # --- 3: slam refuses an immortal target ---
    out3 = strip(cmd(warA, f"slam {warImm_name}"))
    check("bad idea" in out3.lower() or "immortal" in out3.lower(), "slam refuses an immortal target")

    # --- 4: slam succeeds against a mortal target ---
    out4 = strip(cmd(warA, f"slam {warB_name}"))
    check("considerable damage" in out4.lower(), "slam succeeds against a mortal target")
    time.sleep(2.6)
    recv_all(warA, 0.3)

    # --- 5: deathstroke refuses without a wielded weapon ---
    out5 = strip(cmd(warA, f"deathstroke {warB_name}"))
    check("need to hold a weapon" in out5.lower(), "deathstroke refuses without a wielded weapon")

    # --- 6: deathstroke succeeds once a weapon is wielded ---
    # `load` needs immortal tier -- warA stays mortal (level 20) for a
    # realistic skill check, so warImm (already immortal, level 51)
    # loads and drops the sword for warA to pick up, same precedent
    # smoke_test_weapon_messaging.py/smoke_test_spin.py use.
    check("You conjure" in cmd(warImm, f"load obj {SWORD}"), "the test sword is loaded")
    cmd(warImm, "drop sword"); recv_all(warImm, 0.3)
    cmd(warA, "get sword"); recv_all(warA, 0.3)
    cmd(warA, "wield sword"); recv_all(warA, 0.3)
    out6 = strip(cmd(warA, f"deathstroke {warB_name}"))
    check("vital organs" in out6.lower(), "deathstroke succeeds once a weapon is wielded")
    time.sleep(2.6)
    recv_all(warA, 0.3)

    # --- 7: dispel magic strips an active affect ---
    # mageA stays mortal for a realistic skill check -- warImm (already
    # immortal) loads+drops each component instead, same precedent as
    # deathstroke's sword above.
    check("You conjure" in cmd(warImm, f"load obj {COMPONENT}"), "a spell component is loaded")
    cmd(warImm, "drop pouch"); recv_all(warImm, 0.3)
    cmd(mageA, "get pouch"); recv_all(mageA, 0.3)
    out7a = strip(cmd(mageA, "cast gills of flesh"))
    check("gills split open" in out7a.lower(), "a setup buff (waterbreathing) is applied")
    check("You conjure" in cmd(warImm, f"load obj {COMPONENT}"), "a second spell component is loaded")
    cmd(warImm, "drop pouch"); recv_all(warImm, 0.3)
    cmd(mageA, "get pouch"); recv_all(mageA, 0.3)
    out7b = strip(cmd(mageA, "cast dispel magic"))
    check("stripping every active effect" in out7b.lower(), "dispel magic strips the active affect")
    out7c = strip(cmd(mageA, "affects"))
    check("(none)" in out7c, "the affects list is really empty afterward")

    # --- 8: dispel magic says so when there's nothing to strip ---
    cmd(warImm, f"load obj {COMPONENT}"); recv_all(warImm, 0.3)
    cmd(warImm, "drop pouch"); recv_all(warImm, 0.3)
    cmd(mageA, "get pouch"); recv_all(mageA, 0.3)
    out8 = strip(cmd(mageA, "cast dispel magic"))
    check("no active magical effects to strip" in out8.lower(),
          "dispel magic reports nothing to strip when nothing is active")

    announce_done("smoke_test_level20_warrior_mage")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            cmd(sock, "quit!", timeout=0.5)
            sock.close()
        except OSError:
            pass
    for prefix in ("Lmk", "Lwa", "Lwb", "Lma"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_inventory WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
        sql(f"DELETE FROM account WHERE name LIKE LOWER('{prefix}%{_suffix}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum IN ({SWORD}, {COMPONENT});")
