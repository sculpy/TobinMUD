#!/usr/bin/env python3
"""Smoke test for affect persistence (user 2026-07-26: the original
SneezyMUD round-trips active affects -- buffs/debuffs -- through every
login/logout via its charFile; Tobin's active_affect_t used to be
memory-only and lost everything on a deliberate quit!/relog). New
affect_repo.h/.c (player_affect table), wired into player_save()/
player_load() (player_repo.c). Covers:

  1. `pray sanctuary` applies the affect, `affects` shows it with a
     positive round count.
  2. `quit!` (a genuine save-then-destroy, not a raw socket close) then
     logging back in as the SAME character still shows Sanctuary active
     in `affects`, with a rounds_left no greater than before quitting
     (ticks may have run in between, but it must not have reset or
     vanished).
  3. The restored affect still does its real job: HP loss under sustained
     attack stays reduced after the reconnect, not just the display.
  4. Once it naturally expires post-reload, it wears off cleanly (no
     double-application/double-reversal bug from the reload) and
     `affects` goes back to "(none)".

    python3 tests/smoke_test_affect_persistence.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

COMBAT_ROUND_SECS = 1.2


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


announce("smoke_test_affect_persistence")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 910000 + (int(time.time() * 1000) % 60000)
SYMBOL = ROOM + 1


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            break
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


def hp_of(sock):
    # Combat messages carry NO raw damage number anymore (user 2026-07-12,
    # follow-up: "take out the damage number and use it to describe how
    # hard the hit was" -- combat.c's describe_dam() now emits a
    # qualitative word like "pathetically"/"very lightly" instead, for
    # BOTH mortals and immortals). `score` still reports the real live HP
    # directly from the being_t in memory, so HP loss over a fixed window
    # is used as the actual measurable signal instead of parsing hits.
    out = cmd(sock, "score")
    m = re.search(r"HP:\s*(-?\d+)\s*\(", out)
    return int(m.group(1)) if m else None


def hp_loss_over(attacker_sock, target_sock, target_name, rounds):
    cmd(attacker_sock, f"hit {target_name}")
    before = hp_of(target_sock)
    check(before is not None, "read the target's starting HP via score")
    time.sleep(rounds * COMBAT_ROUND_SECS)
    recv_all(attacker_sock, 0.3)  # drain buffered combat spam so it doesn't
    recv_all(target_sock, 0.3)    # bleed into the next command's response
    after = hp_of(target_sock)
    check(after is not None, "read the target's ending HP via score")
    return before - after


pw = "affpersistpw1"

# --- Immortal attacker, same setup precedent as smoke_test_affects.py ---
imm_name = f"Apimm{_suffix}"
imm_pw = "apimmpw12345"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
for step in (imm_name, "y", imm_pw, imm_pw, "new", imm_name, "1", "3", "done", "done"):
    send_line(s_imm, step)
    recv_all(s_imm)
cmd(s_imm, "quit!")
s_imm.close()
set_level(imm_name, 51)
set_hp(imm_name, 2000)
set_dex(imm_name, 900)
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Affect Persistence Sandbox','A bare sandbox room.\\n',"
    f"NULL,1,0,0,0,0,0,0,0,0,0);")
check("Affect Persistence Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen,decay) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,1,1,-1);")
# Workaround for the documented (2026-07-22, STATUS.md) "load obj lands in
# the loading immortal's own inventory, not the room" gap -- drop it back
# onto the floor explicitly, same pattern smoke_test_drugs.py already uses.
cmd(s_imm, f"load obj {SYMBOL}")
cmd(s_imm, "drop symbol")

# --- Cleric, same skill/discipline seeding precedent as smoke_test_affects.py ---
cleric_name = f"Apcle{_suffix}"
cleric_pw = "apclepw12345"
s_cle = socket.create_connection((host, port), timeout=5)
recv_all(s_cle)
for step in (cleric_name, "y", cleric_pw, cleric_pw, "new", cleric_name, "1", "2", "done", "done"):
    send_line(s_cle, step)
    recv_all(s_cle)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
cmd(s_cle, "quit!")
s_cle.close()
sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100, advanced_disc_pct=50 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{cleric_name}');")
set_level(cleric_name, 25)
sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
    f"SELECT id, 'sanctuary', 100, 0 FROM player WHERE name='{cleric_name}' "
    f"ON DUPLICATE KEY UPDATE pct=100;")
set_hp(cleric_name, 8000)
s_cle = socket.create_connection((host, port), timeout=5)
recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, "1"); recv_all(s_cle)
cmd(s_cle, "color off")
check("Affect Persistence Sandbox" in cmd(s_cle, "look"), "the Cleric lands in the sandbox room")

out = cmd(s_cle, "get symbol")
check("you get" in out.lower(), "the Cleric picks up the holy symbol")

# --- 1: pray sanctuary applies the affect ---
baseline_loss = hp_loss_over(s_imm, s_cle, cleric_name, 10)
check(baseline_loss > 0, f"the immortal's sustained attack does real damage before Sanctuary ({baseline_loss} HP)")

out = cmd(s_cle, "pray sanctuary")
check("shimmering aura surrounds you" in out, "pray sanctuary applies the affect")
out = cmd(s_cle, "affects")
m = re.search(r"Sanctuary\s+(\d+) round", out)
check(m is not None and int(m.group(1)) > 0, "affects shows a positive round count for Sanctuary")
rounds_before_quit = int(m.group(1))

# --- 2: quit! (real save-then-destroy) then relog as the SAME character --
cmd(s_cle, "quit!")
send_line(s_cle, "q")  # leave the account menu too, closing cleanly
recv_all(s_cle)
s_cle.close()

s_cle = socket.create_connection((host, port), timeout=5)
recv_all(s_cle)
send_line(s_cle, cleric_name); recv_all(s_cle)
send_line(s_cle, cleric_pw); recv_all(s_cle)
send_line(s_cle, "1"); recv_all(s_cle)
cmd(s_cle, "color off")

out = cmd(s_cle, "affects")
m = re.search(r"Sanctuary\s+(\d+) round", out)
check(m is not None, "Sanctuary is STILL listed in affects after quit!+relog")
rounds_after_relog = int(m.group(1))
check(0 < rounds_after_relog <= rounds_before_quit,
      f"rounds_left survived the reload sanely ({rounds_before_quit} -> {rounds_after_relog}, not reset/grown)")

# --- 3: the restored affect still actually reduces incoming HP loss ---
check("Affect Persistence Sandbox" in cmd(s_cle, "look"), "the Cleric is back in the sandbox room after relog")
baseline_rate = baseline_loss / 10
sanctuary_rounds = max(1, min(rounds_after_relog - 1, 8))  # 1 round of slack before natural expiry
sanctuary_loss = hp_loss_over(s_imm, s_cle, cleric_name, sanctuary_rounds)
sanctuary_rate = sanctuary_loss / sanctuary_rounds
check(sanctuary_rate < baseline_rate * 0.8,
      f"Sanctuary's damage reduction survived the reload "
      f"({baseline_rate:.2f} HP/round -> {sanctuary_rate:.2f} HP/round)")

# --- 4: it still wears off cleanly afterward (no reload-induced double bug) ---
waited = 0
out = ""
while "wears off" not in out and waited < 20:
    out += recv_all(s_cle, 1.5)
    waited += 1
check("Your Sanctuary wears off" in out, "Sanctuary still reports wearing off on its own after a reload")
out = cmd(s_cle, "affects")
check("(none)" in out, "affects is empty again after the reloaded Sanctuary expires")

s_cle.close()
s_imm.close()

sql(f"DELETE FROM player_affect WHERE player_id IN (SELECT id FROM player WHERE name IN "
    f"('{imm_name}','{cleric_name}'));")
sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name IN "
    f"('{imm_name}','{cleric_name}'));")
sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name IN "
    f"('{imm_name}','{cleric_name}'));")
sql(f"DELETE FROM player WHERE name IN ('{imm_name}','{cleric_name}');")
sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum={SYMBOL};")

announce_done("smoke_test_affect_persistence")
print("=== ALL CHECKS PASSED ===")
