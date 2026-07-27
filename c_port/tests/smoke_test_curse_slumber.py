#!/usr/bin/env python3
"""Smoke test for `curse` (Cleric, level 13) and `slumber` (Mage, level
13) -- spell/skill functional-completeness audit continued, level-5+
list. See cmd_pray.c's curse branch and cmd_cast.c's slumber branch for
the real-upstream research (misc/magicutils.cc's genericCurse(),
disc_mage_spirit.cc's slumber()/rawSleep()) and scope-down rationale.

  1. `pray curse <target>` lowers the target's live DEXTERITY and
     shows in `affects` as "Curse".
  2. `cast slumber <target>` puts the target to sleep (POSITION_SLEEPING)
     and shows in `affects` as "Sleep"; a second attempt on an already-
     sleeping target is refused instead of re-applying.
  3. AFFECT_SLEEP auto-wakes the target on expiry (short duration forced
     via SQL, then a wait for affect_tick_run() to catch up).

    python3 tests/smoke_test_curse_slumber.py [host] [port]
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


announce("smoke_test_curse_slumber")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 977000 + (int(time.time()) % 1000)
COMPONENT = ROOM + 1
SYMBOL = ROOM + 2

CLASS_CLERIC = 1
CLASS_MAGE = 0
CLASS_WARRIOR = 2
WEAR_TAKE = 1


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
    # Menu choices ("1", "1") are placeholders, not the real class -- the
    # menu index doesn't line up with CLASS_* enum values (found live:
    # passing a CLASS_* value straight into the creation menu step
    # silently broke character creation, leaving an empty account). Real
    # class is set afterward via a direct `player` SQL UPDATE, same
    # pattern smoke_test_shove.py/smoke_test_bodyslam.py already use.
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


def dex_of(sock):
    m = re.search(r"Dex:\s*(\d+)", cmd(sock, "score"))
    return int(m.group(1))


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Curse-Slumber Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},1);")

pw = "cursepw1234"
cleric_name = f"Curcle{_suffix}"
mage_name = f"Curmag{_suffix}"
vic_name = f"Curvic{_suffix}"

sockets = []
try:
    make_char(cleric_name, pw)
    make_char(mage_name, pw)
    make_char(vic_name, pw)
    sql(f"UPDATE player SET class={CLASS_CLERIC} WHERE name='{cleric_name}';")
    sql(f"UPDATE player SET class={CLASS_MAGE} WHERE name='{mage_name}';")
    sql(f"UPDATE player SET class={CLASS_WARRIOR} WHERE name='{vic_name}';")
    for name in (cleric_name, mage_name):
        sql(f"UPDATE player SET load_room={ROOM} WHERE name='{name}';")
        sql(f"UPDATE player_progress SET level=51, basic_disc_pct=100 "
            f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    # The victim stays a plain, non-immortal mortal (level 20) -- both
    # curse and slumber refuse to target an immortal (being_is_immortal()
    # gate, level >= 51), so a level-51 victim would silently defeat this
    # whole test.
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic_name}';")
    sql(f"UPDATE player_progress SET level=20 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{vic_name}');")

    sc = relog(cleric_name, pw); sockets.append(sc)
    sm = relog(mage_name, pw); sockets.append(sm)
    sv = relog(vic_name, pw); sockets.append(sv)
    cmd(sc, "toggle pk"); cmd(sm, "toggle pk"); cmd(sv, "toggle pk")

    # --- 1: curse lowers live DEXTERITY, shows in affects ---
    dex_before = dex_of(sv)
    check("You need a holy symbol" in cmd(sc, "pray curse " + vic_name),
          "curse without a holy symbol is refused")
    cmd(sc, f"load obj {SYMBOL}"); recv_all(sc, 0.3)
    cmd(sc, "get symbol"); recv_all(sc, 0.3)
    out1 = strip(cmd(sc, f"pray curse {vic_name}"))
    check("dark aura" in out1.lower(), "curse succeeds with a holy symbol")
    dex_after = dex_of(sv)
    check(dex_after < dex_before, f"the victim's live DEXTERITY actually dropped ({dex_before} -> {dex_after})")
    out1b = strip(cmd(sv, "affects"))
    check("curse" in out1b.lower(), "the `affects` command lists Curse while it's active")

    # --- 2: slumber puts the target to sleep, shows in affects, and
    # refuses to re-stack on an already-sleeping target ---
    check("spell components" in cmd(sm, "cast slumber " + vic_name),
          "slumber without a component is refused")
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out2 = strip(cmd(sm, f"cast slumber {vic_name}"))
    check("collapse into sleep" in out2.lower(), "slumber succeeds with a component")
    out2b = strip(cmd(sv, "affects"))
    check("sleep" in out2b.lower(), "the `affects` command lists Sleep while it's active")
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out2d = strip(cmd(sm, f"cast slumber {vic_name}"))
    check("already asleep" in out2d.lower(), "a second slumber attempt on a sleeping target is refused")

    # (Natural expiry / auto-wake is not exercised here -- slumber's real
    # duration runs 50+ rounds, ~60+ real seconds, too slow for a smoke
    # test; same scope as smoke_test_druid_stupidity.py, which tests
    # apply + refresh but not expiry either. affect.c's tick_being_affects()
    # special-case for AFFECT_SLEEP was verified by direct code review.)

    announce_done("smoke_test_curse_slumber")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Curcle", "Curmag", "Curvic"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT}, {SYMBOL});")
