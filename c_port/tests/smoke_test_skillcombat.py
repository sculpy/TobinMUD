#!/usr/bin/env python3
"""Smoke test for skill-based combat (Sneezy -> Tobin feature audit,
"Skill-based combat (bash, kick, disarm, parry)"). Checked Sneezy's own
cmd/cmd_bash.cc, cmd_kick.cc, cmd_disarm.cc, and disc/disc_warrior_
dueling.cc first -- see cmd_bash.c/cmd_kick.c/cmd_disarm.c/combat.c's own
header comments for the full scope-down rationale versus the real,
much heavier originals. bash/kick/disarm are each gated by a single
skill_roll_success() roll (deterministic at 100%/0% seeded proficiency,
same trick smoke_test_mount.py's riding checks use); parry has no
command at all (Sneezy's own is a disabled stub) -- it's a passive
per-incoming-hit roll wired into combat_strike(), checked probabilistically
across many rounds instead.

Covers:
  1. bash: 100%-proficiency success knocks the DEFENDER into a wait-state
     too (their next command is blocked), and the attacker is laggy.
     0%-proficiency always fails (no defender wait-state).
  2. kick: 100%-proficiency success lands ("solid kick" message), attacker
     laggy either way. 0%-proficiency always fails (dodge message).
  3. disarm: 100%-proficiency success knocks the defender's wielded
     weapon onto the room floor. 0%-proficiency always fails (weapon
     stays in hand).
  4. parry (passive, no command): a Warrior with 100% seeded "parry"
     proficiency eventually parries an incoming hit within a bounded
     number of forced combat rounds.

    python3 tests/smoke_test_skillcombat.py [host] [port]
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


announce("smoke_test_skillcombat")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_WARRIOR = 2
CLASS_THIEF = 3

WEAR_TAKE = 1
WEAR_HOLD = 16384  # obj.c's WEAR_HOLD -- wear_slot_for_flag() maps this to WEAR_SLOT_HELD (wield/hold)
TYPE_WEAPON = 5  # matches OBJ_CAT_WEAPON's raw seeded itemTypeT bucket


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
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_combat_disc(name, pct):
    sql(f"UPDATE player_progress SET combat_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


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
    send_line(s, "1"); recv_all(s)  # class: mage (overridden via SQL below)
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_pair(prefix, cls, room=None):
    """Two same-class characters, both PK-opted-in, both padded HP so
    neither dies from ordinary combat while a skill-combat check runs.
    Optionally teleported to a specific sandbox `room` (quit!-then-SQL-
    then-relog, never SQL-then-quit! -- quit!'s own player_save() would
    otherwise clobber the SQL change with stale pre-SQL in-memory state)."""
    nameA, pwA = f"{prefix}a{_suffix}", f"{prefix}apw12345"
    nameB, pwB = f"{prefix}b{_suffix}", f"{prefix}bpw12345"
    sA0 = make_char(nameA, pwA)
    sB0 = make_char(nameB, pwB)
    cmd(sA0, "quit!"); sA0.close()
    cmd(sB0, "quit!"); sB0.close()
    for nm in (nameA, nameB):
        set_class(nm, cls)
        set_hp(nm, 5000, 5000)
        set_combat_disc(nm, 100)
        if room is not None:
            sql(f"UPDATE player SET load_room={room} WHERE name='{nm}';")
    sA = relog(nameA, pwA)
    sB = relog(nameB, pwB)
    cmd(sA, "toggle pk")
    cmd(sB, "toggle pk")
    return (nameA, sA), (nameB, sB)


# =================== 1. bash (Warrior) ===================
(nameA, sA), (nameB, sB) = make_pair("Bshw", CLASS_WARRIOR)
seed_proficiency(nameA, "bash", 100)
cmd(sA, f"attack {nameB}")
out = strip(cmd(sA, f"bash {nameB}"))
check("knocking" in out.lower(), "100%-proficiency bash succeeds and lands")

out_a = strip(cmd(sA, "look"))
check("still recovering" in out_a.lower(), "the attacker is laggy after a successful bash")
out_b = strip(cmd(sB, "look"))
check("still recovering" in out_b.lower(),
      "a successful bash also costs the DEFENDER a round (Sneezy's own 'prevent skill-use' effect)")
sA.close(); sB.close()

(nameC, sC), (nameD, sD) = make_pair("Bshw0", CLASS_WARRIOR)
seed_proficiency(nameC, "bash", 0)
cmd(sC, f"attack {nameD}")
out = strip(cmd(sC, f"bash {nameD}"))
check("twist out of the way" in out.lower(), "0%-proficiency bash always fails")
out_d = strip(cmd(sD, "look"))
check("still recovering" not in out_d.lower(),
      "a failed bash does NOT cost the defender a round")
sC.close(); sD.close()

# =================== 2. kick (Thief) ===================
(nameE, sE), (nameF, sF) = make_pair("Kckt", CLASS_THIEF)
seed_proficiency(nameE, "kick", 100)
cmd(sE, f"attack {nameF}")
out = strip(cmd(sE, f"kick {nameF}"))
check("solid kick" in out.lower(), "100%-proficiency kick succeeds and lands")
out_e = strip(cmd(sE, "look"))
check("still recovering" in out_e.lower(), "the attacker is laggy after a kick attempt")
sE.close(); sF.close()

(nameG, sG), (nameH, sH) = make_pair("Kckt0", CLASS_THIEF)
seed_proficiency(nameG, "kick", 0)
cmd(sG, f"attack {nameH}")
out = strip(cmd(sG, f"kick {nameH}"))
check("dodge out of the way" in out.lower(), "0%-proficiency kick always fails")
sG.close(); sH.close()

# =================== 3. disarm (Warrior) ===================
# A dedicated sandbox room (rather than discovering wherever the
# default starting room happens to be) so the immortal can `goto` it
# directly -- a MORTAL's own `look` never shows a room vnum bracket
# (that's immortal-only, cmd_look.c), so there's no reliable way to
# discover a mortal's current room from their own output anyway.
ROOM_DISARM = 970000 + (int(time.time()) % 20000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_DISARM},0,0,0,'Skillcombat Disarm Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name, imm_pw = f"Dsmwimm{_suffix}", "dsmwimmpw1"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM_DISARM}")

(nameI, sI), (nameJ, sJ) = make_pair("Dsmw", CLASS_WARRIOR, ROOM_DISARM)
seed_proficiency(nameI, "disarm", 100)

WEAPON_VNUM = ROOM_DISARM + 1
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
    f"VALUES ({WEAPON_VNUM},'sword','a steel sword','A steel sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},5,1);")
cmd(si, f"load obj {WEAPON_VNUM}")
cmd(sJ, "get sword")
cmd(sJ, "wield sword")

cmd(sI, f"attack {nameJ}")
out = strip(cmd(sI, f"disarm {nameJ}"))
check("knock" in out.lower() and "grip" in out.lower(), "100%-proficiency disarm succeeds")

out_room = strip(cmd(si, "look"))
check("steel sword" in out_room.lower(), "the disarmed weapon lands on the room floor")
sI.close(); sJ.close()

(nameK, sK), (nameL, sL) = make_pair("Dsmw0", CLASS_WARRIOR, ROOM_DISARM)
seed_proficiency(nameK, "disarm", 0)

WEAPON_VNUM2 = WEAPON_VNUM + 1
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
    f"VALUES ({WEAPON_VNUM2},'dagger','a rusty dagger','A rusty dagger is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},2,1);")
cmd(si, f"load obj {WEAPON_VNUM2}")
cmd(sL, "get dagger")
cmd(sL, "wield dagger")

cmd(sK, f"attack {nameL}")
out = strip(cmd(sK, f"disarm {nameL}"))
check("can't get a grip" in out.lower(), "0%-proficiency disarm always fails")
si.close()
sK.close(); sL.close()

# =================== 4. parry (Warrior, passive, no command) ===================
(nameM, sM), (nameN, sN) = make_pair("Pryw", CLASS_WARRIOR)
seed_proficiency(nameM, "parry", 100)
cmd(sN, f"attack {nameM}")  # N attacks M -- M is the defender each round, the parry-eligible side

parried = False
full = ""
for _ in range(20):
    time.sleep(1.3)  # ~one COMBAT_ROUND_PULSES round (~1.2s)
    out = strip(recv_all(sM, 0.3)) + strip(recv_all(sN, 0.3))
    full += out
    if "parry" in out.lower() or "parries" in out.lower():
        parried = True
        break
check(parried, "a 100%-proficiency Warrior eventually parries an incoming hit within 20 forced rounds")

sM.close(); sN.close()

announce_done("smoke_test_skillcombat")
print("=== ALL CHECKS PASSED ===")
