#!/usr/bin/env bash
# Full smoke sweep against a running server on 127.0.0.1:4000.
# Run from anywhere: bash tests/sweep.sh
cd "$(dirname "$0")/.."

# Per-test wall-clock cap so a single hung test can never stall the whole
# sweep (a 1.5s poll vs the 1.2s combat round once let weapon_depth block the
# run for 2h+). Override with TEST_TIMEOUT=<seconds> if a legit test is slower.
TEST_TIMEOUT="${TEST_TIMEOUT:-180}"

pass=0; fail=0; failed=""
for t in tests/smoke_test_*.py tests/smoke_test.py; do
  [ -f "$t" ] || continue
  name=$(basename "$t")
  timeout "$TEST_TIMEOUT" python3 "$t" 127.0.0.1 4000 >/dev/null 2>&1
  rc=$?
  if [ "$rc" -eq 0 ]; then
    pass=$((pass+1))
  elif [ "$rc" -eq 124 ]; then
    fail=$((fail+1)); failed="$failed ${name}(timeout)"
  else
    fail=$((fail+1)); failed="$failed $name"
  fi
done
echo "SUMMARY: $pass passed, $fail failed"
[ -n "$failed" ] && echo "non-passing:$failed"
exit 0
