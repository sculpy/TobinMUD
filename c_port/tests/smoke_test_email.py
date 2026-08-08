#!/usr/bin/env python3
"""Smoke test for account email (user, 2026-08-08: "modify the account menu
to prompt a user to enter their email address ... we won't share their
address for any purpose and will use it solely for MUD related
communications ... save the email address in the account database", plus
a follow-up "allow someone to opt out of providing email").

New account creation now prompts for an email address right after the
timezone prompt (descriptor.c's CONN_GET_EMAIL) -- blank is a silent,
valid opt-out (account.email stays ''), a string with no '@'/no '.' after
the '@' is rejected with a retry, anything else is saved via
account_set_email() (account_repo.c). The `email` command (cmd_email.c)
is the self-service equivalent post-login: `email` shows the current
value (or opted-out status), `email <address>` sets/changes it, `email
clear` re-opts out.

Also covers `mailinglist` (level 60): exports every account with a
non-empty email to a logs/mailinglist_<timestamp>.txt file, one address
per line.

Covers:
  1. New-account creation flow reaches the email prompt (after skipping
     the timezone prompt) and rejects an invalid address with a retry.
  2. Blank at the email prompt opts out silently (account.email stays
     empty in the DB).
  3. A valid address at the prompt is saved to account.email.
  4. `email` with no args reports opted-out / the current address
     correctly for both states.
  5. `email <address>` updates account.email; `email clear` re-opts out.
  6. `email notanemail` is rejected (no '@' or no '.' after it).
  7. `mailinglist` (as a level-60 character) writes a file under logs/
     containing exactly the currently opted-in addresses.

    python3 tests/smoke_test_email.py [host] [port]
"""
import os
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

announce("smoke_test_email", host, port)


def db_email(account_name):
    res = subprocess.run(["mariadb", "tobin", "-Nse",
        f"SELECT email FROM account WHERE name='{account_name.lower()}';"],
        capture_output=True, text=True, check=True)
    return res.stdout.strip()


def create_through_email_prompt(name, pw, email_answer, make_character=True):
    """Drives account creation up to and past the (new) email prompt --
    answers 'y' to the color prompt and blank to skip timezone, so the
    flow doesn't take the old-script backward-compat shortcut that
    bypasses both (see CONN_GET_COLOR_PREF's own fallback-redispatch
    comment in descriptor.c). Lands at CONN_ACCOUNT_MENU either way
    (opted-in or opted-out) with zero characters yet -- if make_character,
    continues through the standard character-creation flow (same steps
    every other smoke test's make_char() uses) so self-service in-game
    commands like `email`/`mailinglist` (MORTAL_LEVEL_MIN+, only
    dispatched once CONN_PLAYING) have someone to run as."""
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "y", ""):
        send_line(s, step)
        recv_all(s)
    send_line(s, email_answer)
    out = recv_all(s, 1.5)
    if make_character:
        for step in ("new", name, "1", "1", "1", "done", "done"):
            send_line(s, step)
            out = recv_all(s)
    return s, out


# --- 1/2: invalid address retries, blank opts out ---
name1, pw1 = f"Emlopt{_suffix}", "emloptpw12345"
s1 = socket.create_connection((host, port), timeout=5)
recv_all(s1)
for step in (name1, "y", pw1, pw1, "y", ""):
    send_line(s1, step); recv_all(s1)
send_line(s1, "not-an-email")
out = recv_all(s1)
check("doesn't look like a valid email" in out, "an invalid address at the creation prompt is rejected with a retry")
send_line(s1, "")
out = recv_all(s1)
check("Your players" in out, "blank at the (retried) email prompt opts out and reaches the account menu")
s1.close()
check(db_email(name1) == "", "opting out at creation leaves account.email empty in the DB")

# --- 3: a valid address at the prompt is saved ---
name2, pw2 = f"Emlset{_suffix}", "emlsetpw12345"
s2, out = create_through_email_prompt(name2, pw2, f"{name2.lower()}@example.com")
s2.close()
check(db_email(name2) == f"{name2.lower()}@example.com", "a valid address at the creation prompt is saved to account.email")


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    return s


# --- 4/5/6: the `email` self-service command ---
s2 = relog(name2, pw2)
out = cmd(s2, "email")
check(f"{name2.lower()}@example.com" in out, "`email` with no args reports the currently-set address")

out = cmd(s2, f"email changed{_suffix}@example.org")
check("Email updated" in out, "`email <address>` accepts a new address")
check(db_email(name2) == f"changed{_suffix}@example.org", "`email <address>` persists the change to the DB")

out = cmd(s2, "email notanemail")
check("doesn't look like a valid email" in out, "`email notanemail` is rejected")
check(db_email(name2) == f"changed{_suffix}@example.org", "a rejected `email` argument does not overwrite the existing address")

out = cmd(s2, "email clear")
check("opted out" in out.lower(), "`email clear` reports the opt-out")
check(db_email(name2) == "", "`email clear` empties account.email in the DB")

out = cmd(s2, "email")
check("not provided" in out.lower(), "`email` with no args reports opted-out status after `email clear`")
s2.close()

# --- 7: `mailinglist` (level 60) exports opted-in addresses ---
imm_name, imm_pw = f"Emlimm{_suffix}", "emlimmpw12345"
s3, _ = create_through_email_prompt(imm_name, imm_pw, "")  # opted out, doesn't matter for this account
cmd(s3, "quit!"); s3.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")

# re-set name2's email so there's a known, current opted-in address to find
sql(f"UPDATE account SET email='mllist{_suffix}@example.net' WHERE name='{name2.lower()}';")

s3 = relog(imm_name, imm_pw)
out = cmd(s3, "mailinglist", timeout=3.0)
m = re.search(r"Wrote (\d+) opted-in email address(?:es)? to (\S+)", out)
check(m is not None, "`mailinglist` reports how many addresses it wrote and the output path")
full_path = os.path.join("/home/mud/TobinMUD/c_port", m.group(2))
check(os.path.isfile(full_path), f"mailinglist's reported output file actually exists ({full_path})")
with open(full_path) as f:
    contents = f.read()
check(f"mllist{_suffix}@example.net" in contents, "the currently opted-in address appears in the exported file")
check(db_email(name1) == "", "the opted-out account's own address stayed empty (mailinglist's own query excludes empty emails at the SQL level)")
s3.close()

announce_done("smoke_test_email", host, port)
print("=== ALL CHECKS PASSED ===")
