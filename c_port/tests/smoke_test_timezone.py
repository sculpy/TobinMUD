#!/usr/bin/env python3
"""Smoke test for the account-creation time-zone offset (Session 43
continued, ported from Sneezy's CON_TIME / doTime() -- user: "in account
creation, ask the character to choose a time zone based on machine time
zone, so for PST set timezone -3, etc"). Covers:
  1. The new-account flow prompts for a time zone offset right after the
     color preference, and rejects a non-numeric / out-of-range answer.
  2. A valid offset is accepted, persisted (account.time_adjust), and
     survives a relog.
  3. `time` (bare) shows a real-world clock line in addition to the mud
     clock, shifted by the stored offset.
  4. `time <difference>` re-sets the offset later, and the new offset is
     reflected immediately in the next `time` output.

    python3 tests/smoke_test_timezone.py [host] [port]
"""
import re
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


announce("smoke_test_timezone")

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


REAL_TIME_RE = re.compile(r"It is (\d{1,2}):(\d{2}) (AM|PM) where you are \(real time\)\.")

name = f"Tzone{_suffix}"
pw = "timezonepw123"

# --- 1/2: the prompt appears, rejects garbage, accepts and persists a value ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
out = cmd(s, "")  # accept the (Y) color default
check("time zone" in out.lower() and "Eastern" in out,
      "account creation prompts for a time-zone offset right after color")

out = cmd(s, "not-a-number")
check("doesn't look like a whole number" in out,
      "a non-numeric answer is rejected and re-prompts")

out = cmd(s, "99")
check("doesn't look like a whole number" in out,
      "an out-of-range answer (99) is rejected and re-prompts")

out = cmd(s, "-3")
check("Time zone set" in out, "a valid offset (-3) is accepted")

send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
cmd(s, "1")  # race: human (zero stat modifier)
cmd(s, "1")  # class: mage
cmd(s, "done")
cmd(s, "2")  # alignment: neutral
s.close()

# --- 3: relog, `time` shows both the mud clock and a real-time line ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

out1 = cmd(s, "time")
check("It is" in out1 and ", on " in out1, "`time` still shows the mud clock/weekday line")
m1 = REAL_TIME_RE.search(out1)
check(m1 is not None, f"`time` also shows a real-time line (got: {out1!r})")

# --- 4: `time <difference>` re-sets the offset, reflected on the next `time` ---
out_set = cmd(s, "time -5")
check("Time zone offset updated" in out_set, "`time -5` re-sets the offset")

out2 = cmd(s, "time")
m2 = REAL_TIME_RE.search(out2)
check(m2 is not None, f"`time` shows a real-time line after re-setting (got: {out2!r})")

h1 = int(m1.group(1)) % 12 + (12 if m1.group(3) == "PM" else 0)
h2 = int(m2.group(1)) % 12 + (12 if m2.group(3) == "PM" else 0)
total1 = h1 * 60 + int(m1.group(2))
total2 = h2 * 60 + int(m2.group(2))
delta = (total1 - total2) % (24 * 60)  # went from -3 to -5: real time moves back 2 hours
check(delta == 120,
      f"shifting the offset from -3 to -5 moves the shown real time back exactly 2 hours "
      f"(before={m1.group(0)!r}, after={m2.group(0)!r})")

s.close()
announce_done("smoke_test_timezone")
print("=== ALL CHECKS PASSED ===")
