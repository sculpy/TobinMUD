#!/usr/bin/env python3
"""Smoke test for fights persisting across copyover (user 2026-08-03:
"fights should persist after copyover"). Previously cmd_copyover.c
unconditionally cleared every `fighting` pointer before writing the
recovery file, so ANY ongoing fight silently ended on every copyover.
Now the opponent's identity is recorded (by name for a PC opponent, by
room+vnum+ordinal for a mob one -- see cmd_copyover.c's
mob_ordinal_in_room()/game_loop.c's copyover_recover() deferred-fight
re-link) and both sides' `fighting` pointers are restored once
everyone/everything exists again post-exec.

Requires an immortal test account already promoted to level 59+ that can
run `copyover` -- reuses the session's established Cpovtitq pattern
(see smoke_test_copyover_state.py). This test IS destructive to the live
server (it triggers a real copyover), so it's meant to be run
deliberately, not as part of an unattended sweep.

Covers:
  1. Two PCs mid-fight both show "Position: Fighting" on `score` before
     copyover.
  2. After a real copyover, both PCs STILL show "Position: Fighting" --
     the fight survived, not silently ended.

    python3 tests/smoke_test_copyover_fight.py [host] [port]
"""
import re
import socket
import random
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

# Random, not just time-based (a same-second rerun after an earlier failed
# run -- this test's own copyover round-trip makes reruns common while
# debugging -- would otherwise collide on the exact same suffix and log
# back into a leftover half-configured character from the prior run
# instead of a fresh one, found live 2026-08-03).
_suffix = "".join(random.choice("abcdefghijklmnopqrstuvwxyz") for _ in range(4))


def recv_all(sock, timeout=1.5):
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


def cmd(sock, line, timeout=1.5):
    send_line(sock, line)
    return recv_all(sock, timeout)


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


IMM_NAME = "Cpovtitq"
IMM_PW = "copyoverpw1234"


def imm_login():
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, IMM_NAME); recv_all(s)
    send_line(s, IMM_PW); recv_all(s)
    send_line(s, "1"); recv_all(s, 1.0)
    cmd(s, "color off")
    return s


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # homeland: urban
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


def relog(name, pw, timeout=1.5):
    # `timeout` matters a lot here specifically: reconnecting to a
    # character that's mid-fight (this test's whole point, post-copyover)
    # means live combat rounds (every 1.2s) can interleave with the
    # login sequence's own reads -- a timeout >= that cadence makes
    # recv_all() busy-loop through login the same way it does for the
    # post-attack score checks above (see their comment). Callers
    # reconnecting a fighting character should pass something short.
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, timeout)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s, timeout)
    cmd(s, "color off", timeout)
    return s


def is_fighting(out):
    return "Position: Fighting" in out


def score_while_fighting(sock, tries=8, timeout=0.5):
    """`score` (like most commands) refuses with "You are still
    recovering!" while the caller is lag-locked from a swing
    (`wait_pulses`, ~1.2s after `attack`/each combat round) -- not a
    socket-timing issue, a real command gate. Retries past that lag
    window instead of taking the first (likely refused) response at
    face value."""
    for _ in range(tries):
        out = cmd(sock, "score", timeout)
        if "still recovering" not in out:
            return out
    return out


print("=== Copyover Fight Persistence Test ===\n")

name1, pw1 = f"Cpfa{_suffix}", "cpfapw123456"
name2, pw2 = f"Cpfb{_suffix}", "cpfbpw123456"
ROOM = 943000 + (int(time.time()) % 50000)

try:
    s_imm = imm_login()

    s1 = make_char(name1, pw1, "3")  # Warrior
    s2 = make_char(name2, pw2, "3")  # Warrior
    sql(f"UPDATE player_progress SET hp=5000, max_hp=5000 WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{name1}', '{name2}'));")
    s1.close(); s2.close()

    # A private sandbox room, not the default starting room -- real
    # ambient traffic there (other players, mob autoloot spam, room
    # echoes) kept steadily refilling the socket buffer and defeated
    # recv_all()'s "wait for a quiet gap" logic, so the "before copyover"
    # score check never fired until the WHOLE fight had already resolved
    # to a death in the background (found live 2026-08-03).
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Copyover Fight Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name IN ('{name1}', '{name2}');")

    s1 = relog(name1, pw1)
    s2 = relog(name2, pw2)
    cmd(s1, "toggle pk")
    cmd(s2, "toggle pk")

    # Short timeouts from here through the pre-copyover score checks
    # (NOT the default 1.5s): combat rounds fire every COMBAT_ROUND_PULSES
    # (1.2s, combat.c), which is FASTER than recv_all()'s default 1.5s
    # "wait for a quiet gap" window -- with the default, recv_all() never
    # sees a quiet gap once rounds start landing and just keeps draining
    # the socket in a busy loop until the fight naturally ends (one side
    # dies), which looks exactly like a hang. A short timeout well under
    # 1.2s reliably grabs just the immediate response instead (found live
    # 2026-08-03, cost real time to track down since it presented as a
    # silent hang with no traceback).
    out = cmd(s1, f"attack {name2}", 0.5)
    check("you attack" in out.lower() or "you engage" in out.lower(), "combat starts")

    out1 = score_while_fighting(s1)
    out2 = score_while_fighting(s2)
    check(is_fighting(out1), "the attacker shows Position: Fighting before copyover")
    check(is_fighting(out2), "the target shows Position: Fighting before copyover")

    cmd(s_imm, "copyover", 3.0)
    time.sleep(8)

    # Do NOT relog() here -- that was a real test bug, not a feature bug:
    # copyover's whole point is that the inherited fd survives the exec()
    # with the SAME connection still live and already playing (descriptor_
    # copyover_adopt() reconnects it synchronously as part of boot, no
    # fresh login needed or wanted). Logging in again on top of an already-
    # active session trips the "duplicate character instance" reclaim gate
    # (world_find_active_pc(), descriptor.c) -- it destroys the OLD being_t
    # (the one copyover had just carefully restored `fighting` onto) and
    # replaces it with a brand new plain DB load that has no `fighting` at
    # all, which is exactly why this looked like the feature was broken
    # (found live 2026-08-03: server logs showed "2 fight(s) re-linked"
    # every time, proving the feature itself worked, right up until the
    # test's own relog() silently discarded it). s1/s2 are still the same
    # live sockets from before the copyover.
    out1 = score_while_fighting(s1)
    out2 = score_while_fighting(s2)
    check(is_fighting(out1), "the attacker STILL shows Position: Fighting after copyover")
    check(is_fighting(out2), "the target STILL shows Position: Fighting after copyover")

    cmd(s1, "flee", 0.5)

    print("\n=== ALL CHECKS PASSED ===")
finally:
    for _sock_name in ("s1", "s2", "s_imm"):
        _sock = locals().get(_sock_name)
        if _sock is not None:
            try:
                _sock.close()
            except OSError:
                pass
    sql(f"DELETE FROM player_inventory WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{name1}', '{name2}'));")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{name1}', '{name2}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{name1}', '{name2}'));")
    sql(f"DELETE FROM player WHERE name IN ('{name1}', '{name2}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
