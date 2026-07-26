#!/usr/bin/env python3
"""Smoke test for Druid's `stupidity` spell (Full spell/skill/prayer
roster import, user 2026-07-26: 6 named Shaman spells ported onto
Druid). This is also the first real exercise of being_apply_stat_affect()
(affect.c) -- Tobin's first stat-MODIFYING affect (everything before this
was a plain flag/timer: poison, disease, sanctuary, ...).

  1. Casting `stupidity` on an opponent actually lowers their live
     Intelligence score by a real amount (not just a cosmetic message).
  2. The `affects` command lists "Stupidity" while it's active.
  3. Removing the affect (immortal `cast`-adjacent debug path -- reusing
     the same combat_debug tooling other limb tests use isn't available
     for affects, so this uses a second stupidity cast's own refresh path
     instead) doesn't double-apply the penalty -- INT after a second
     cast matches INT after the first, not a further, stacked drop.

    python3 tests/smoke_test_druid_stupidity.py [host] [port]
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


announce("smoke_test_druid_stupidity")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 974000 + (int(time.time()) % 20000)


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
    raw = recv_all(sock, timeout)
    return raw.split("\r\n", 1)[1] if "\r\n" in raw else raw


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", class_choice, "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


def int_of(sock):
    m = re.search(r"Int:\s*(\d+)", cmd(sock, "score"))
    return int(m.group(1))


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Stupidity Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

druid_name = f"Stupdru{_suffix}"
vic_name = f"Stupvic{_suffix}"
pw = "stuppw1234"
COMPONENT = ROOM + 1

make_char(druid_name, pw, "5")  # class 5 = Druid
make_char(vic_name, pw, "3")
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{druid_name}');")
sql(f"UPDATE player_progress SET basic_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{druid_name}');")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,1,1);")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, druid_name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")
cmd(s, f"goto {ROOM}")

sv = socket.create_connection((host, port), timeout=5)
recv_all(sv)
send_line(sv, vic_name); recv_all(sv)
send_line(sv, pw); recv_all(sv)
send_line(sv, "1"); recv_all(sv)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic_name}';")
send_line(sv, "quit!"); recv_all(sv)
sv.close()
sv = socket.create_connection((host, port), timeout=5)
recv_all(sv)
send_line(sv, vic_name); recv_all(sv)
send_line(sv, pw); recv_all(sv)
send_line(sv, "1"); recv_all(sv)
cmd(sv, "color off")

int_before = int_of(sv)

cmd(s, "toggle pk")
cmd(sv, "toggle pk")

cmd(s, f"load obj {COMPONENT}")
out = cmd(s, f"cast stupidity {vic_name}")
check("stupidity" in out.lower(), "casting stupidity at all works")

int_after = int_of(sv)
check(int_after < int_before,
      f"the victim's live Intelligence actually dropped ({int_before} -> {int_after})")

out = cmd(sv, "affects")
check("stupidity" in out.lower(), "the `affects` command lists Stupidity while it's active")

# Re-casting refreshes rather than stacking a second penalty on top.
cmd(s, f"load obj {COMPONENT}")
out2 = cmd(s, f"cast stupidity {vic_name}")
int_after2 = int_of(sv)
check(int_after2 == int_after,
      f"re-casting stupidity refreshes it instead of stacking a second penalty ({int_after} == {int_after2})")

sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{druid_name}', '{vic_name}'));")
sql(f"DELETE FROM player WHERE name IN ('{druid_name}', '{vic_name}');")
sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")

announce_done("smoke_test_druid_stupidity")
print("=== ALL CHECKS PASSED ===")
