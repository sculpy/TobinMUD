#!/usr/bin/env python3
"""Smoke test for "Sign language" (Sneezy -> Tobin feature audit). Checked
the real upstream doc first (docs/systems/important/communication-
system.md's "Sign Language Reception" section, misc/talk.cc's doSign()):
silent, room-only speech that only a fellow SKILL_SIGN holder actually
reads -- everyone else sees a generic "makes funny motions with hands"
line, except a Thief signer (a stealth-class exemption: anyone reads a
Thief's hand-signs regardless of their own skill). Ported as a new
universal `sign` skill (every class gets it, same "riding" precedent) and
`sign <message>` command (cmd_sign.c). Covers:

  1. A fresh (level 1, no discipline) character can't sign at all.
  2. Two characters who both know sign: the receiver sees the real
     message, tagged "signs".
  3. A bystander who does NOT know sign sees the generic "funny motions"
     line instead, when the signer is NOT a Thief.
  4. A Thief signer's message is read by EVERYONE regardless of their own
     sign knowledge (the stealth-class exemption).
  5. Gating: empty message, fighting, hands full (wielding), a badly hurt
     arm, and asleep all refuse with their own specific message.

    python3 tests/smoke_test_sign.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_sign", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 945000 + (int(time.time()) % 30000)
WEAPON_VNUM = ROOM + 1

CLASS_WARRIOR = 2
CLASS_THIEF = 3


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage (overridden via SQL below)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    s.close()


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def grant_sign(name, level=15):
    """`sign` is CLASS-tier for every class -- being_knows_skill() gates
    it on level >= min_level (10) AND basic_disc_pct > 0, no player_skill
    row needed (cmd_sign.c never rolls proficiency, just checks
    knowledge)."""
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name, imm_pw = f"Sgnimm{_suffix}", "signimmpw123"
a_name, a_pw = f"Sgna{_suffix}", "signapw12345"
b_name, b_pw = f"Sgnb{_suffix}", "signbpw12345"
c_name, c_pw = f"Sgnc{_suffix}", "signcpw12345"  # never learns sign
t_name, t_pw = f"Sgnt{_suffix}", "signtpw12345"  # thief signer

sockets = []

try:
    make_char(imm_name, imm_pw)
    sql(f"UPDATE player_progress SET level=59 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{imm_name}');")

    for nm, pw, cls in ((a_name, a_pw, CLASS_WARRIOR), (b_name, b_pw, CLASS_WARRIOR),
                        (c_name, c_pw, CLASS_WARRIOR), (t_name, t_pw, CLASS_THIEF)):
        make_char(nm, pw)
        set_class(nm, cls)
    # A and B and T know sign; C deliberately does not (stays level 1,
    # basic_disc_pct 0 -- the fresh-character default).
    grant_sign(a_name)
    grant_sign(b_name)
    grant_sign(t_name)
    # A/B need to survive a real combat exchange for the "fighting blocks
    # sign" check below -- grant_sign() only bumps `level` directly (not
    # a real level-up), so their HP is still whatever a fresh level-1
    # character starts with, easily a one-hit kill that would end the
    # fight (clearing `fighting`) before the check even runs.
    set_hp(a_name, 999, 999)
    set_hp(b_name, 999, 999)

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Sign Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
        f"VALUES ({WEAPON_VNUM},'sword','a steel sword','A steel sword is lying here.',"
        f"5,16401,5,1);")  # WEAR_TAKE|WEAR_HOLD

    imm = login(imm_name, imm_pw); sockets.append(imm)
    a = login(a_name, a_pw); sockets.append(a)
    b = login(b_name, b_pw); sockets.append(b)
    c = login(c_name, c_pw); sockets.append(c)
    t = login(t_name, t_pw); sockets.append(t)

    check("Sign Sandbox" in cmd(imm, f"goto {ROOM}"), "goto lands in the sandbox room")
    for nm, sock in ((a_name, a), (b_name, b), (c_name, c), (t_name, t)):
        cmd(imm, f"transfer {nm} {ROOM}")
    for sock in (imm, a, b, c, t):
        recv_all(sock)  # drain arrival notices

    # --- 1: a fresh character (no discipline) can't sign at all ---
    out = cmd(c, "sign hello there")
    check("don't know sign language" in out.lower(), "a character with no discipline can't sign at all")

    # --- 2: empty message ---
    out = cmd(a, "sign")
    check("WHAT do you want to sign" in out, "an empty sign is refused")

    # --- 3: two signers -- the receiver reads the real message ---
    out_a = cmd(a, "sign the eagle flies at midnight")
    check("You sign" in out_a and "eagle flies at midnight" in out_a, "the signer sees their own message echoed")
    out_b = recv_all(b, 1.0)
    check(f"{a_name} signs" in out_b and "eagle flies at midnight" in out_b,
          "a fellow signer reads the real signed message")

    # --- 4: a non-signing bystander sees the generic line instead ---
    out_c = recv_all(c, 0.5)
    check("funny motions" in out_c.lower() and "eagle flies at midnight" not in out_c,
          "a non-signing bystander sees the generic motions line, not the real content")

    # --- 5: a Thief signer is read by EVERYONE, signer or not ---
    out_t = cmd(t, "sign the shipment arrives friday")
    check("You sign" in out_t, "the Thief signer sees their own message")
    out_c2 = recv_all(c, 1.0)
    check(f"{t_name} signs" in out_c2 and "shipment arrives friday" in out_c2,
          "a Thief's signed message is read by a non-signing bystander too (stealth-class exemption)")

    # --- 7: fighting blocks it ---
    # Mortal-vs-mortal PC combat needs BOTH sides opted into PK
    # (combat_pk_allowed(), combat.c) or combat_find_room_target() just
    # skips the target entirely ("They aren't here.") -- same
    # "PK-opted-in" requirement smoke_test_skillcombat.py's make_pair()
    # already documents. `attack` itself then sets COMBAT_ROUND_PULSES
    # (1.2s) of wait on the ATTACKER at initiation (cmd_attack.c) --
    # trying `sign` immediately after collides with that same lag and
    # cmd_dispatch()'s own global wait-state gate rejects it before
    # cmd_sign.c's own fighting check ever runs. Sleep it off first.
    cmd(a, "toggle pk")
    cmd(b, "toggle pk")
    cmd(a, f"attack {b_name}")
    time.sleep(1.3)
    out = cmd(a, "sign nope")
    check("can't spare your hands" in out.lower() or "fighting" in out.lower(), "can't sign while fighting")
    cmd(imm, f"kill {a_name}")  # clears the fight cleanly (instaslay) so later checks aren't mid-combat
    time.sleep(0.3)
    a.close()
    a = login(a_name, a_pw)
    cmd(imm, f"transfer {a_name} {ROOM}")
    recv_all(a)

    # --- 8: hands full (wielding) blocks it ---
    cmd(imm, f"load obj {WEAPON_VNUM}")
    cmd(imm, "drop sword")
    cmd(a, "get sword")
    cmd(a, "wield sword")
    out = cmd(a, "sign nope")
    check("hands are full" in out.lower(), "can't sign with hands full")
    cmd(a, "remove sword")  # (wielded weapons come off via remove too, freeing both hands)

    # --- 9: a badly hurt arm blocks it ---
    cmd(imm, f"crit {a_name} leftarm 1")
    out = cmd(a, "sign nope")
    check("too hurt" in out.lower(), "can't sign with a badly hurt arm")
    cmd(imm, f"crit {a_name} leftarm 999")

    # --- 10: asleep blocks it ---
    cmd(a, "sleep")
    out = cmd(a, "sign nope")
    check("asleep" in out.lower(), "can't sign while asleep")
    cmd(a, "wake")

    for sock in sockets:
        sock.close()
    sockets = []

    announce_done("smoke_test_sign", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name IN "
        f"('{imm_name}','{a_name}','{b_name}','{c_name}','{t_name}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name IN "
        f"('{imm_name}','{a_name}','{b_name}','{c_name}','{t_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}','{a_name}','{b_name}','{c_name}','{t_name}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum={WEAPON_VNUM};")
