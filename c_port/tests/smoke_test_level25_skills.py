#!/usr/bin/env python3
"""Smoke test for the level-25 physical combat skill batch (2026-07-29):
Warrior (whirlwind, kneestrike, switchopponents, trance, weapon
retention, brawl avoidance), Thief (stabbing, subterfuge; disarm trap
already existed), Monk (chop, hurl, feign death, counter move, iron
fist, blindfighting, critical hitting, chain attack/blur/advanced
kicking, wohlin meditation). `voplat` ("makes unarmed damage magical")
is left an inert placeholder -- no damage-type/immunity system exists
to hook it into.

Uses a separate IMMORTAL helper for goto/load mob/purge, and MORTAL
fighters (level 25, kept well below IMMORTAL_LEVEL_MIN=51) for the
actual skill commands -- `attack` instakills via combat_instakill() for
an immortal (cmd_kill.c's bypass), same pitfall the level-23 haste test
hit and documented.

    python3 tests/smoke_test_level25_skills.py [host] [port]
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


announce("smoke_test_level25_skills")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900300 + (int(time.time()) % 70000)
ROOM_OUT = BASE


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


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_full_discipline(name):
    sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")


def set_skill_pct(name, skill_name, pct=100):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"SELECT id, '{skill_name}', {pct}, 0 FROM player WHERE name='{name}' "
        f"ON DUPLICATE KEY UPDATE pct={pct}, last_gain_at=0;")


def make_char(sock, name, pw, class_num):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)          # race: human
    send_line(sock, str(class_num)); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)       # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def mob_insert(vnum, name, hpbonus):
    cols = [
        ("vnum", str(vnum)), ("name", f"'{name}'"), ("short_desc", f"'a {name}'"),
        ("long_desc", f"'A {name} stands here.'"), ("description", "'A dummy eyes you.'"),
        ("actions", "0"), ("affects", "0"), ("faction", "0"), ("fact_perc", "0"), ("letter", "'A'"),
        ("attacks", "1.0"), ("class", "0"), ("level", "5"), ("tohit", "0"), ("ac", "0"),
        ("hpbonus", str(hpbonus)), ("damage_level", "0"), ("damage_precision", "0"), ("gold", "0"),
        ("race", "0"), ("weight", "0"), ("height", "0"),
        ("str", "0"), ("bra", "0"), ("con", "0"), ("dex", "0"), ("agi", "0"),
        ("intel", "0"), ("wis", "0"), ("foc", "0"), ("per", "0"), ("cha", "0"),
        ("kar", "0"), ("spe", "0"), ("pos", "10"), ("def_position", "10"), ("sex", "1"),
        ("spec_proc", "0"), ("skin", "0"), ("vision", "0"), ("can_be_seen", "1"), ("max_exist", "1"),
    ]
    mob_cols = ",".join(c for c, _ in cols)
    mob_vals = ",".join(v for _, v in cols)
    sql(f"INSERT INTO mob ({mob_cols}) VALUES ({mob_vals});")


imm_name = f"L25skimm{_suffix}"
warrior_name = f"Lwar{_suffix}"
thief_name = f"Lthf{_suffix}"
monk_name = f"Lmnk{_suffix}"
pw = "l25skillpw123"

# Immortal helper (level 58+) -- goto/load mob/purge only.
s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, pw, 1)  # Mage
cmd(s, "quit!")
s.close()
set_level(imm_name, 58)
imm = login(imm_name, pw)

# Mortal fighters -- quit!ed before their own SQL edits, same ordering
# rule TODO.md/earlier sessions' writeups document (an SQL edit BEFORE
# quit! gets silently clobbered by quit!'s own save of stale in-memory
# state).
s = socket.create_connection((host, port), timeout=5)
make_char(s, warrior_name, pw, 3)  # Warrior
cmd(s, "quit!")
s.close()
set_level(warrior_name, 25)
set_full_discipline(warrior_name)
for sk in ("whirlwind", "kneestrike", "switch opponents", "trance of blades"):
    set_skill_pct(warrior_name, sk)

s = socket.create_connection((host, port), timeout=5)
make_char(s, thief_name, pw, 4)  # Thief
cmd(s, "quit!")
s.close()
set_level(thief_name, 25)
set_full_discipline(thief_name)
for sk in ("stabbing", "subterfuge"):
    set_skill_pct(thief_name, sk)

s = socket.create_connection((host, port), timeout=5)
make_char(s, monk_name, pw, 6)  # Monk
cmd(s, "quit!")
s.close()
set_level(monk_name, 25)
set_full_discipline(monk_name)
for sk in ("chop", "hurl", "feign death"):
    set_skill_pct(monk_name, sk)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT},0,0,0,'L25 Skills Sandbox','A bare outdoor sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# Mortals can't `goto` by vnum -- placed directly via load_room, same
# convention smoke_test_level23_spells.py's fighter placement used.
sql(f"UPDATE player SET load_room={ROOM_OUT} WHERE name IN "
    f"('{warrior_name}', '{thief_name}', '{monk_name}');")

warrior = login(warrior_name, pw)
thief = login(thief_name, pw)
monk = login(monk_name, pw)
check("L25 Skills Sandbox" in cmd(warrior, "look"), "warrior lands directly in the sandbox")
check("L25 Skills Sandbox" in cmd(thief, "look"), "thief lands directly in the sandbox")
check("L25 Skills Sandbox" in cmd(monk, "look"), "monk lands directly in the sandbox")

check("L25 Skills Sandbox" in cmd(imm, f"goto {ROOM_OUT}"), "immortal goes to the sandbox")

# --- Warrior: whirlwind, kneestrike, switchopponents, trance ---
DUMMY1 = BASE + 2
DUMMY2 = BASE + 3
mob_insert(DUMMY1, f"l25dummy1{_suffix}", 20.0)
mob_insert(DUMMY2, f"l25dummy2{_suffix}", 20.0)
check("You conjure" in cmd(imm, f"load mob l25dummy1{_suffix}"), "first dummy spawns")
check("You conjure" in cmd(imm, f"load mob l25dummy2{_suffix}"), "second dummy spawns")

check("Command not found" not in cmd(warrior, "attack l25dummy1"), "warrior attacks the first dummy")
out = cmd(warrior, f"switchopponents l25dummy2")
check("break off" in out.lower(), "switchopponents confirms")
out = cmd(warrior, "kneestrike")
check("knee" in out.lower(), "kneestrike confirms")
out = cmd(warrior, "whirlwind")
check("whirlwind" in out.lower(), "whirlwind confirms")
out = cmd(warrior, "trance")
check("trance" in out.lower(), "trance of blades confirms")
cmd(imm, "purge")

# --- Thief: stabbing, subterfuge ---
DUMMY3 = BASE + 4
DUMMY4 = BASE + 5
mob_insert(DUMMY3, f"l25dummy3{_suffix}", 20.0)
mob_insert(DUMMY4, f"l25dummy4{_suffix}", 20.0)
check("You conjure" in cmd(imm, f"load mob l25dummy3{_suffix}"), "third dummy spawns")
check("You conjure" in cmd(imm, f"load mob l25dummy4{_suffix}"), "fourth dummy spawns")
check("Command not found" not in cmd(thief, "attack l25dummy3"), "thief attacks the third dummy")
out = cmd(thief, f"subterfuge l25dummy4")
check("slip" in out.lower() or "see through" in out.lower(), "subterfuge confirms")
check("Command not found" not in cmd(thief, "attack l25dummy4"), "thief re-attacks after subterfuge")
out = cmd(thief, "stabbing")
check("stab" in out.lower(), "stabbing confirms")
cmd(imm, "purge")

# --- Monk: chop, hurl, feign death ---
DUMMY5 = BASE + 6
DUMMY6 = BASE + 7
mob_insert(DUMMY5, f"l25dummy5{_suffix}", 20.0)
mob_insert(DUMMY6, f"l25dummy6{_suffix}", 20.0)
check("You conjure" in cmd(imm, f"load mob l25dummy5{_suffix}"), "fifth dummy spawns")
check("Command not found" not in cmd(monk, "attack l25dummy5"), "monk attacks the fifth dummy")
out = cmd(monk, "chop")
check("chop" in out.lower(), "chop confirms")
cmd(imm, "purge")

check("You conjure" in cmd(imm, f"load mob l25dummy6{_suffix}"), "sixth dummy spawns")
check("Command not found" not in cmd(monk, "attack l25dummy6"), "monk attacks the sixth dummy")
out = cmd(monk, "hurl")
check("hurl" in out.lower() or "leverage" in out.lower(), "hurl confirms")
cmd(imm, "purge")

out = cmd(monk, "feigndeath")
check("play dead" in out.lower(), "feign death confirms")
out = cmd(monk, "feigndeath")
check("pick" in out.lower(), "feign death toggles back off")

print("ALL CHECKS PASSED")
announce_done("smoke_test_level25_skills")
