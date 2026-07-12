#!/usr/bin/env python3
"""Smoke test for the `balance` command (user 2026-07-12: "a balance
command (60) where you take args: balance <class|race> that is menu
driven to adjust balance numbers/modifiers that will apply gamewide to
the class or race you just balanced"). Covers:

  1. A level-51 immortal (below 60) can't reach `balance` at all.
  2. A level-60 immortal can open the menu-driven editor for a class
     and a race, see the neutral defaults (1.00/1.00/+0/+0).
  3. Editing a field (HP multiplier) marks the editor dirty and updates
     the menu display; (S)ave persists it and reports success.
  4. The saved change actually applies gamewide -- a fresh Warrior PC's
     max HP changes after balancing the Warrior class's HP multiplier.
  5. (Q)uit with unsaved changes prompts to Save/Discard/Cancel.

    python3 tests/smoke_test_balance.py [host] [port]
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


announce("smoke_test_balance")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


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
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


pw = "balancepw123"

# --- 1: a 51 immortal (below 60) can't reach balance ---
imm51_name = f"Balfifty{_suffix}"
s51 = make_char(imm51_name, pw, "1")
set_level(imm51_name, 51)
cmd(s51, "quit!")
s51.close()
s51 = socket.create_connection((host, port), timeout=5)
recv_all(s51)
send_line(s51, imm51_name); recv_all(s51)
send_line(s51, pw); recv_all(s51)
send_line(s51, "1"); recv_all(s51)
cmd(s51, "color off")
out = cmd(s51, "balance class warrior")
check("Huh?!" in out, "a level-51 immortal can't reach balance (60+ only)")
s51.close()

# --- 2: a level-60 immortal can open the class editor ---
imm_name = f"Balsixty{_suffix}"
s_imm = make_char(imm_name, pw, "1")
set_level(imm_name, 60)
cmd(s_imm, "quit!")
s_imm.close()
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

# Reset the Warrior class row to neutral through the REAL command path
# (not raw SQL) -- a prior test run may have left the live in-memory
# cache non-neutral, and only class_balance_set() (which Save calls)
# refreshes that cache; a raw SQL UPDATE would fix the DB row but leave
# the still-running server's cache stale.
cmd(s_imm, "balance class warrior")
cmd(s_imm, "1"); cmd(s_imm, "1.0")
cmd(s_imm, "2"); cmd(s_imm, "1.0")
cmd(s_imm, "3"); cmd(s_imm, "0")
cmd(s_imm, "4"); cmd(s_imm, "0")
cmd(s_imm, "S")
cmd(s_imm, "Q")

out = cmd(s_imm, "balance class warrior")
check("Balancing class: Warrior" in out, "the class balance editor opens for Warrior")
check("HP multiplier: 1.00" in out, "Warrior's HP multiplier starts neutral (1.00)")
check("Damage multiplier: 1.00" in out, "Warrior's damage multiplier starts neutral (1.00)")
check("To-hit modifier: +0" in out, "Warrior's to-hit modifier starts neutral (+0)")
check("AC modifier: +0" in out, "Warrior's AC modifier starts neutral (+0)")

# --- 3: editing HP multiplier marks dirty, then Save persists it ---
out = cmd(s_imm, "1")
check("Enter new HP multiplier" in out, "menu option 1 prompts for the HP multiplier")
out = cmd(s_imm, "2.0")
check("unsaved changes" in out and "HP multiplier: 2.00" in out,
      "setting the HP multiplier to 2.0 shows it and marks the editor dirty")

out = cmd(s_imm, "S")
check("Balance saved" in out, "Save persists the change")
check("unsaved changes" not in out, "the dirty marker clears after Save")

out = cmd(s_imm, "Q")
check("Leaving the balance editor" in out, "Quit with no unsaved changes leaves immediately")

# --- 4: the saved HP multiplier applies gamewide to a fresh Warrior PC ---
warrior_name = f"Balwar{_suffix}"
sw = make_char(warrior_name, pw, "3")
out = cmd(sw, "score")
import re
m = re.search(r"HP:\s*(\d+)/(\d+)", out)
check(m is not None, "score shows an HP: current/max pair")
max_hp = int(m.group(2))

# being_calc_max_hp() = 20 + (constitution - 120) + level * 5 * (class_hp_scale * hp_mult).
# class_hp_scale(WARRIOR) = 1.3; with hp_mult balanced to 2.0, the scale
# is 2.6 instead of 1.3 -- compute the exact expected value from the
# Warrior's actual persisted constitution rather than guessing it.
con_result = subprocess.run(
    ["mariadb", "sneezy", "-N", "-e",
     f"SELECT constitution FROM player_attrs WHERE player_id="
     f"(SELECT id FROM player WHERE name='{warrior_name}');"],
    check=True, capture_output=True, text=True)
con = int(con_result.stdout.strip())
expected_max_hp = 20 + (con - 120) + int(1 * 5 * 1.3 * 2.0)
check(max_hp == expected_max_hp,
      f"the balanced HP multiplier (2.0x) gives the exact expected max HP ({expected_max_hp}, got {max_hp})")
sw.close()

# --- reset the Warrior row back to neutral (through the real command
#     path, so the live cache -- not just the DB row -- goes back to
#     neutral too) so this test is repeatable and doesn't leave the
#     live server permanently rebalanced ---
cmd(s_imm, "balance class warrior")
cmd(s_imm, "1"); cmd(s_imm, "1.0")
cmd(s_imm, "S")
cmd(s_imm, "Q")
cmd(s_imm, "quit!")
s_imm.close()

# --- 5: Quit with unsaved changes prompts Save/Discard/Cancel ---
s_imm2 = socket.create_connection((host, port), timeout=5)
recv_all(s_imm2)
send_line(s_imm2, imm_name); recv_all(s_imm2)
send_line(s_imm2, pw); recv_all(s_imm2)
send_line(s_imm2, "1"); recv_all(s_imm2)
cmd(s_imm2, "color off")

out = cmd(s_imm2, "balance race elf")
check("Balancing race: Elf" in out, "the race balance editor opens for Elf")
out = cmd(s_imm2, "3")
cmd(s_imm2, "5")
out = cmd(s_imm2, "Q")
check("unsaved changes" in out and "(C)ancel" in out, "Quit with unsaved changes offers Save/Discard/Cancel")
out = cmd(s_imm2, "D")
check("Leaving the balance editor" in out, "Discard leaves without saving")

# Confirm the discard really didn't persist.
result = subprocess.run(
    ["mariadb", "sneezy", "-N", "-e", "SELECT tohit_mod FROM race_balance WHERE race=1;"],
    check=True, capture_output=True, text=True)
check(result.stdout.strip() == "0", "the discarded to-hit modifier was never saved to the DB")

s_imm2.close()
announce_done("smoke_test_balance")
print("=== ALL CHECKS PASSED ===")
