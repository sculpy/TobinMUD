#!/usr/bin/env python3
"""Smoke test for lit light sources being visible in a dark room (user
2026-08-03: "make torches and other light sources visible in the
dark"). Previously room_is_dark_for()'s early return in `look`
(cmd_look.c) suppressed the ENTIRE room description including
contents, so a lit torch carried by another player, or lying lit on
the ground, was completely invisible in the dark -- the same as
everything else. Now the darkness branch still scans room contents for
any lit OBJ_CAT_LIGHT object (dropped, rendered with its normal ground
line) or any other being carrying/wielding one (being_has_active_
light()), and lists just those, instead of a flat "you cannot see a
thing."

Covers:
  1. A plain dark room with NO light at all still shows the original
     "pitch black... you cannot see a thing." message unchanged.
  2. A lit torch lying on the ground IS visible in that same dark
     room, tagged "(lit)" same as always.
  3. Another player carrying/wielding a lit light source is shown as
     present ("<name> is here, carrying a light."), even though the
     rest of the room stays dark.

    python3 tests/smoke_test_dark_light_visible.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_dark_light_visible", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 942000 + (int(time.time()) % 50000)
TORCH_VNUM = 105  # real seeded OBJ_CAT_LIGHT, starts unlit, refuelable=no


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step); recv_all(s)
    cmd(s, "quit!")
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name, imm_pw = f"Dlvimm{_suffix}", "dlvimmpw123"
mort_a, pw_a = f"Dlva{_suffix}", "dlvapw123456"
mort_b, pw_b = f"Dlvb{_suffix}", "dlvbpw123456"

try:
    make_char(imm_name, imm_pw, "1")
    sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
    make_char(mort_a, pw_a, "3")
    make_char(mort_b, pw_b, "3")

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Dark Light Visible Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name IN ('{imm_name}', '{mort_a}', '{mort_b}');")

    si = relog(imm_name, imm_pw)
    sA = relog(mort_a, pw_a)
    sB = relog(mort_b, pw_b)

    # Force real night so the room is genuinely dark for anyone without
    # their own light (same bounded-polling technique smoke_test_weather.py
    # uses).
    for _ in range(10):
        out = cmd(si, "weather")
        if "dark" in out:
            break
        cmd(si, "aitick 10")
    check("dark" in out, "forced the game clock into the night window")

    # --- 1: a fully dark room with no light at all ---
    out = cmd(sA, "look")
    check(out.strip().endswith("It is pitch black... you cannot see a thing.") or
          "It is pitch black... you cannot see a thing.\r\n" in out,
          "with no light anywhere, look still shows the original plain darkness message")

    # --- 2: a lit torch on the ground is visible despite the dark ---
    cmd(si, f"load obj {TORCH_VNUM}")
    cmd(si, "light torch")
    cmd(si, "drop torch")
    out = cmd(sA, "look")
    check("pitch black" in out, "the room is still described as pitch black overall")
    check("torch" in out.lower() and "(lit)" in out,
          "a lit torch on the ground is visible, tagged (lit), despite the dark")

    # --- 3: another PC carrying a lit light source is visible ---
    cmd(si, f"load obj {TORCH_VNUM}")
    cmd(si, "drop torch")
    check("get" in cmd(sB, "get torch").lower() or "torch" in cmd(sB, "inventory").lower(),
          "the second player picks up their own torch")
    cmd(sB, "hold torch")
    cmd(sB, "light torch")

    out = cmd(sA, "look")
    check(mort_b.lower() in out.lower() and "carrying a light" in out,
          "another player carrying a lit light source is shown as present in the dark")

    announce_done("smoke_test_dark_light_visible", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    # Restore a neutral daytime hour (same shared-clock cleanup concern
    # smoke_test_weather.py's own cleanup documents).
    try:
        for _ in range(10):
            out = cmd(si, "weather")
            if "daylight" in out:
                break
            cmd(si, "aitick 10")
    except (NameError, OSError):
        pass
    for _sock_name in ("si", "sA", "sB"):
        _sock = locals().get(_sock_name)
        if _sock is not None:
            try:
                _sock.close()
            except OSError:
                pass
    sql(f"DELETE FROM player_inventory WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_a}', '{mort_b}'));")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_a}', '{mort_b}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_a}', '{mort_b}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{mort_a}', '{mort_b}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
