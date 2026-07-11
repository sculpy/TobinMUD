#!/usr/bin/env python3
"""Smoke test for the trade-based point-buy rules: attributes can be
lowered (down to -30) to free up room to raise others (up to +30), each
individually capped at +/-30 from base, still bounded overall by the net
pool (30 -- deliberately smaller than the +/-30 per-attribute cap, so a
single attribute can exhaust the whole pool on its own).

    python3 tests/smoke_test_trade_attrs.py [host] [port]
"""
import socket
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test. Self-contained (own socket, doesn't depend on this file's other
    helpers) so it can run at any point in the script."""
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
    """Companion to announce() -- emits a [TEST] "finished" log line. Call
    once at the very end of a smoke test, just before it reports success."""
    announce(f"done {test_name}", host, port)


announce("smoke_test_trade_attrs")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
account_name = f"TradeTester{_suffix}"
char_name = f"Trader{_suffix}"
password = "tradetestpw123"


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


s = socket.create_connection((host, port), timeout=5)
recv_all(s)
step(s, "account name", account_name)
step(s, "confirm new account creation", "y")
step(s, "password (first entry)", password)
step(s, "confirm password -> menu", password)
step(s, "new", "new")
out = step(s, "char name -> attr screen", char_name)
check("Points remaining: 30" in out, "starts with the full 30-point net pool")

# Per-stat cap: +30 is fine (and, since pool == cap here, also exactly
# exhausts the pool by itself), +31 is rejected regardless of the pool.
out = step(s, "str +30 (cap, should succeed, also exhausts the pool)", "str 30")
check("Strength:      150" in out and "Points remaining: 0" in out,
      "str 30 -> Strength 150 (120+30), pool now fully spent")

out = step(s, "str +31 (over cap, should reject)", "str 31")
check("Adjustment must be between -30 and +30" in out, "adjustment beyond +30 is rejected")
check("Strength:      150" in out, "Strength unchanged after the rejected attempt")

# Pool-specific rejection: dex +30 is within the per-attribute cap, but the
# pool is already fully committed to str -- this must be rejected for a
# different reason than the cap check above.
out = step(s, "dex +30 -- within cap, but pool already spent on str", "dex 30")
check("Not enough points remaining" in out, "pool exhaustion rejects an otherwise-valid amount")
check("Dexterity:     120" in out, "Dexterity unchanged after the rejected attempt")

# Lowering a stat frees room again -- the trade mechanic.
out = step(s, "wis -30 (floor, should succeed and free up room)", "wis -30")
check("Wisdom:         90" in out and "Points remaining: 30" in out,
      "wis -30 offsets str's +30, netting the sum back to 0 and refilling the pool")

out = step(s, "wis -31 (past floor, should reject)", "wis -31")
check("Adjustment must be between -30 and +30" in out, "adjustment beyond -30 is rejected")
check("Wisdom:         90" in out, "Wisdom unchanged after the rejected attempt")

# Now that wis freed up the pool again, dex +30 should succeed.
out = step(s, "dex +30 (now affordable thanks to the wis trade)", "dex 30")
check("Dexterity:     150" in out and "Points remaining: 0" in out,
      "str(+30) + wis(-30) + dex(+30) = net 30, pool exactly spent again")

# One more point anywhere should now be rejected (pool-specific, not cap).
out = step(s, "con +1 -- pool has nothing left", "con 1")
check("Not enough points remaining" in out, "pool is fully committed, no further net increase allowed")
check("Constitution:  120" in out, "the rejected attempt left Constitution unchanged")

out = step(s, "reset clears everything", "reset")
check("Points remaining: 30" in out and "Strength:      120" in out,
      "reset returns all attributes to base and restores the full pool")

s.close()
announce_done("smoke_test_trade_attrs")
print("=== ALL CHECKS PASSED ===")
