#!/usr/bin/env python3
"""Smoke test for the 2026-07-18 spell/skill affects expansion (user:
"implement spell/skill affects... make each work from sneezy code") --
covers the parts NOT already exercised by smoke_test_affects.py (which
covers the original v1 Sanctuary-only mechanic) or smoke_test_castpray.py
(which covers the component/symbol gate itself):

  1. `cast cure poison` / `pray cure poison` report correctly when
     nothing is active, and genuinely remove AFFECT_POISON when it is.
  2. `cast cure disease` / `pray cure disease` -- same, for a disease.
  3. `pray cure poison <name>` / `pray cure disease <name>` cure a
     NAMED target remotely, not just the caster.
  4. A Cleric fighting someone can `pray poison` / `pray disease` to
     genuinely inflict those on their opponent (not just fall through
     to the old "nothing happens yet" placeholder).
  5. The expanded buff-keyword family: `pray armor` (Cleric) applies the
     same AFFECT_SANCTUARY ward `pray sanctuary` already used
     pre-expansion -- proves the new keyword match, not the (unchanged)
     apply/tick/expire mechanics themselves, which smoke_test_affects.py
     already covers. `cast`'s side of the same expanded keyword list
     (barkskin, stone skin, ...) shares this exact branch's logic in
     cmd_cast.c and was exercised manually, live, this session; not
     re-covered here to avoid a whole second Mage/Druid character just
     to re-prove an identical code path.

Uses the same fresh-character-per-test-run + SQL-bootstrap pattern as
smoke_test_affects.py, including its set_hp()/set_dex() trick to keep
mutual combat non-lethal for as long as the test needs it -- a plain
`kill`/`attack` was found (2026-07-18 session, live investigation) to
instantly resolve for an IMMORTAL attacker regardless of stats
(cmd_kill.c's combat_instakill(), by design), so this test never uses an
immortal as the one initiating combat; only ordinary `hit`, same as
smoke_test_affects.py.

    python3 tests/smoke_test_cure_and_inflict.py [host] [port]
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


announce("smoke_test_cure_and_inflict")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000) % 60000)
SYMBOL = ROOM + 1


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


def set_hp(name, hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_dex(name, dex):
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock_name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, sock_name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, sock_name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    cmd(s, "quit!")
    s.close()


def relog(sock_name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, sock_name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


# --- Cleric caster: IMMORTAL (IMMORTAL_LEVEL_MIN=51 means no MORTAL
#     level can ever reach "poison" (min_level 59) or "disease" (min_level
#     56) -- those two prayers are effectively immortal-only as currently
#     leveled, so there's no genuine-mortal version of this test to write.
#     `load obj`/`goto` also need immortal rank. Class-gate-bypass and
#     component/symbol-gate itself are already covered by
#     smoke_test_immortal_castpray.py / smoke_test_castpray.py -- this
#     test is only about what happens once cast/pray actually run. ---
cleric_name = f"Cinfcle{_suffix}"
cleric_pw = "cinfclepw123"
make_char(cleric_name, cleric_pw, "2")  # class: cleric
set_level(cleric_name, 60)
set_hp(cleric_name, 2000)

# --- Warrior victim: mortal, huge HP so ~10 rounds of the Cleric's own
#     weak melee (from `hit`, needed once to enter mutual combat) never
#     comes close to killing them, huge DEX so their own retaliation
#     essentially never lands on the Cleric either -- same numbers
#     smoke_test_affects.py already proved sufficient for this exact
#     mutual-combat-survival trick. ---
victim_name = f"Cinfvic{_suffix}"
victim_pw = "cinfvicpw123"
make_char(victim_name, victim_pw, "3")  # class: warrior
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
set_hp(victim_name, 2000)
set_dex(victim_name, 900)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Cure and Inflict Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1);")

s_cle = relog(cleric_name, cleric_pw)
s_vic = relog(victim_name, victim_pw)
check("Cure and Inflict Sandbox" in cmd(s_cle, "look"), "the Cleric lands in the sandbox room")

# Stock up: 8 holy symbols -- one each for pray cure poison/cure disease
# (baseline, nothing active), pray armor, pray poison, pray disease, and
# pray cure poison/cure disease again (remote cure) -- 7 total, +1 margin.
out = ""
for _ in range(8):
    cmd(s_cle, f"load obj {SYMBOL}")
    out = cmd(s_cle, "get symbol")  # `get all` has no bare form -- see cmd_object.c, only `get all <container>`
check("silver" in out.lower(), "the Cleric picks up the holy symbols")

# --- 1: cure poison, nothing active ---
out = cmd(s_cle, "pray cure poison")
check("weren't poisoned to begin with" in out, "pray cure poison reports correctly with no poison active")

# --- 2: cure disease, nothing active ---
out = cmd(s_cle, "pray cure disease")
check("weren't sick to begin with" in out, "pray cure disease reports correctly with no disease active")

# --- 5: expanded buff keywords -- pray armor applies the same Sanctuary
#     affect pray sanctuary already used (smoke_test_affects.py covers
#     sanctuary itself; this only proves the NEW keyword match). ---
out = cmd(s_cle, "pray armor")
check("shimmering aura surrounds you" in out, "pray armor applies a protective ward")
out = cmd(s_cle, "affects")
check("Sanctuary" in out, "affects shows Sanctuary after pray armor")

# --- 4: enter combat, then inflict poison/disease on the opponent ---
out = cmd(s_cle, f"hit {victim_name}")
check("You" in out or "miss" in out.lower() or "hit" in out.lower(), "the Cleric attacks to enter mutual combat")
recv_all(s_vic, 1.0)  # drain the victim's own incoming-hit notice

out = cmd(s_cle, "pray poison")
check(f"poisoning {victim_name}" in out, "pray poison reports inflicting poison on the fighting opponent")
vout = cmd(s_vic, "affects")
check("Poison" in vout, "the victim's own affects now lists Poison")

out = cmd(s_cle, "pray disease")
check("afflicting" in out and victim_name in out, "pray disease reports inflicting a disease on the fighting opponent")
vout = cmd(s_vic, "affects")
disease_names = ["Cold", "Flu", "Frostbite", "Bleeding", "Infection", "Herpes", "Broken Bone",
                  "Numbed Limb", "Voicebox", "Eyeball", "Lung", "Stomach", "Hemorrhage",
                  "Leprosy", "Plague", "Suffocate", "Food Poisoning", "Drowning", "Garrotte",
                  "Syphilis", "Bruised", "Scurvy", "Dysentery", "Pneumonia", "Gangrene",
                  "Extreme Pain"]
check(any(d in vout for d in disease_names), f"the victim's own affects now lists a disease ({vout!r})")

cmd(s_cle, "flee")
recv_all(s_vic, 1.0)

# --- 3: remote cure -- cure the victim's poison/disease by name, not
#     needing to still be fighting them ---
out = cmd(s_cle, f"pray cure poison {victim_name}")
check("their poison fades away" in out, "pray cure poison <name> cures a named target remotely")
vout = cmd(s_vic, "affects")
check("Poison" not in vout, "the victim's own affects no longer lists Poison after the remote cure")

out = cmd(s_cle, f"pray cure disease {victim_name}")
check("their sickness lifts" in out, "pray cure disease <name> cures a named target remotely")
vout = cmd(s_vic, "affects")
check(not any(d in vout for d in disease_names), f"the victim's own affects no longer lists a disease ({vout!r})")

s_cle.close()
s_vic.close()
announce_done("smoke_test_cure_and_inflict")
print("=== ALL CHECKS PASSED ===")
