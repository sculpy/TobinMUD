#!/usr/bin/env python3
"""Regression test for the wiznews pager truncation bug (user: "wiznews
bug with the pager/long output that freezes the mud"). Root cause:
descriptor_page_start() (descriptor.c) copied its whole source string
into a fixed 16384-byte `page_buf` via a bounded snprintf -- silently
TRUNCATING anything longer, no matter how big the caller's own source
buffer was. cmd_wiznews.c/cmd_news.c already build up to a 101000-byte
`full` string (a previous, real fix for a growing feed that used to
overflow a smaller buffer) -- but that fix never reached page_buf, so
once the feed grew past ~16KB it silently cut off mid-sentence with no
indication anything was missing, rather than a true infinite hang --
easily read as "the mud froze" by someone mid-page expecting more.
Fixed by sizing page_buf to comfortably clear cmd_news.c/cmd_wiznews.c's
own 101000-byte ceiling (131072 bytes).

`smoke_test_news.py` already has a "the whole feed is shown after paging
through" check, but it depends on specific, real, aging headlines that
can (and, per that test's own comment, eventually will) scroll out of
the 40-item recent-news window as more real content gets posted over
many sessions -- not a reliable regression guard for THIS bug. This test
instead seeds its own large, uniquely-marked synthetic wiznews entries
(freshly inserted, so guaranteed newest-first, ahead of any real
content) sized to cumulatively exceed the OLD 16384-byte cap, and
confirms every one of them -- including each entry's own last character
-- survives paging intact.

    python3 tests/smoke_test_wiznews_pager.py [host] [port]
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


announce("smoke_test_wiznews_pager")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(65536)
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
    send_line(s, "1"); recv_all(s)  # class: mage
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


imm_name, imm_pw = f"Wnpimm{_suffix}", "wnpimmpw1234"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

# 5 synthetic entries, ~4200 chars of body each (~21000 total) -- well
# past the old 16384-byte page_buf cap, and each individually marked at
# both its start and its very last character so a mid-entry truncation
# would be caught, not just a missing-entirely one.
ENTRY_COUNT = 5
BODY_CHAR = "x"
titles = []
for i in range(ENTRY_COUNT):
    title = f"Pager Stress Test {_suffix} {i}"
    titles.append(title)
    body = f"START{i}-" + (BODY_CHAR * 4180) + f"-END{i}"
    sql(f"INSERT INTO wiznews (author, title, body) VALUES "
        f"('Test Rig', '{title}', '{body}') "
        f"ON DUPLICATE KEY UPDATE body=VALUES(body), created_at=NOW();")
    time.sleep(1.1)  # created_at has 1s resolution -- keep insertion order stable

out = cmd(si, "wiznews")
full = out
pages = 0
while "ENTER for more" in out and pages < 30:
    out = cmd(si, "")
    full += out
    pages += 1

for i in range(ENTRY_COUNT):
    check(f"START{i}-" in full, f"entry {i}'s opening marker survived paging")
    check(f"-END{i}" in full, f"entry {i}'s CLOSING marker survived paging (not truncated mid-body)")

check(full.rstrip().endswith(">"), "paging through the full oversized feed ends at a normal prompt")

for title in titles:
    sql(f"DELETE FROM wiznews WHERE title='{title}';")

si.close()
announce_done("smoke_test_wiznews_pager")
print("=== ALL CHECKS PASSED ===")
