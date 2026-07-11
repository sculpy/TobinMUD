#!/usr/bin/env python3
"""Smoke test for the new-account confirmation prompt (Session 43
continued, user: "in account login, if someone types in an account name
that doesnt exist, we're assuming the want a new account. it should ask:
New account, are you sure you want to create account <account name>?
(y/n) yes creates a new account and no prompts for the correct login
name."). Covers:

  1. An unrecognized account name gets the confirmation prompt (naming the
     exact account name typed), not an immediate jump to password creation.
  2. Answering "n" does NOT create the account -- it re-prompts for the
     account name instead, and a fresh reconnect with the same name still
     shows "New account" (proving nothing was created).
  3. Answering "y" proceeds to password creation as before, and the account
     is genuinely persisted -- a later reconnect with that name goes
     straight to "Password:" instead of asking to confirm creation again.

    python3 tests/smoke_test_account_confirm.py [host] [port]
"""
import socket
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


announce("smoke_test_account_confirm")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"Confirmtest{_suffix}"
password = "confirmtestpw123"


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


def step(sock, label, line):
    send_line(sock, line)
    out = recv_all(sock)
    print(f"=== {label} ===")
    print(out)
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


# --- Part 1: an unrecognized name gets the confirmation prompt ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)  # banner + "Account name: " prompt
out = step(s, "unrecognized account name", account_name)
check("new account" in out.lower(), "an unrecognized name is flagged as a new account")
check(f"create the account {account_name}" in out, "the prompt names the exact account name typed")
check("(y/n)" in out.lower(), "the prompt asks for a y/n answer")
check("choose a password" not in out.lower(), "password creation does NOT start before confirmation")
s.close()

# --- Part 2: "n" declines -- nothing is created, re-prompts for the name ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
step(s, "unrecognized account name", account_name)
out = step(s, "decline with n", "n")
check("account name" in out.lower(), "declining re-prompts for the account name")
check("password" not in out.lower(), "declining does not fall through to a password prompt")
s.close()

# Reconnect fresh -- if "n" truly created nothing, the same name is STILL
# unrecognized and gets the confirmation prompt again.
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
out = step(s, "same name again after decline", account_name)
check("new account" in out.lower(), "the declined name is still unrecognized on a fresh connection")
s.close()

# --- Part 3: "y" confirms -- proceeds to real account creation ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
step(s, "unrecognized account name", account_name)
out = step(s, "confirm with y", "y")
check("choose a password" in out.lower(), "confirming with y proceeds to password creation")
step(s, "password (first entry)", password)
out = step(s, "confirm password -> color prompt", password)
check("color" in out.lower(), "account creation continues normally after confirmation")
step(s, "answer color prompt", "y")
step(s, "answer time zone prompt", "")
s.close()

# Reconnect -- the account now genuinely exists, so the SAME name goes
# straight to a password prompt, no confirmation step.
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
out = step(s, "reconnect with the now-real account", account_name)
check("password:" in out.lower(), "a real, created account skips the confirmation and asks for the password")
check("new account" not in out.lower(), "a real, created account is never flagged as new again")
out = step(s, "log in with the real password", password)
check("(none yet)" in out or "character" in out.lower(),
      "the created account actually logs in to the account menu")
s.close()

announce_done("smoke_test_account_confirm")
print("=== ALL CHECKS PASSED ===")
