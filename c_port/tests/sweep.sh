#!/usr/bin/env bash
# Full smoke sweep against a running server on 127.0.0.1:4000.
# Run from anywhere: bash tests/sweep.sh
cd "$(dirname "$0")/.."
pass=0; fail=0; failed=""
for t in tests/smoke_test_*.py tests/smoke_test.py; do
  [ -f "$t" ] || continue
  name=$(basename "$t")
  if python3 "$t" 127.0.0.1 4000 >/dev/null 2>&1; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); failed="$failed $name"
  fi
done
echo "SUMMARY: $pass passed, $fail failed"
[ -n "$failed" ] && echo "non-passing:$failed"
exit 0
