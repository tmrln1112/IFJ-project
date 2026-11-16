#!/bin/sh
# Regression tests for the IFJ26 scanner.
#
#   tests/ok/*.ifj   valid input: rc must be 0 and stdout must equal *.out
#   tests/err/*.ifj  invalid input: rc must be 1 (stdout is not compared)
#
# Usage: sh tests/run_tests.sh [path/to/scanner_test]

BIN=${1:-./scanner_test}
DIR=$(dirname "$0")
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

pass=0
fail=0

if [ ! -x "$BIN" ]; then
  echo "binary '$BIN' not found, run make first" >&2
  exit 2
fi

for f in "$DIR"/ok/*.ifj; do
  "$BIN" <"$f" >"$TMP" 2>/dev/null
  rc=$?
  if [ "$rc" -ne 0 ]; then
    echo "FAIL  $f (rc=$rc, expected 0)"
    fail=$((fail + 1))
  elif ! diff -u "${f%.ifj}.out" "$TMP" >/dev/null; then
    echo "FAIL  $f (output differs)"
    diff -u "${f%.ifj}.out" "$TMP" | head -n 20
    fail=$((fail + 1))
  else
    pass=$((pass + 1))
  fi
done

for f in "$DIR"/err/*.ifj; do
  "$BIN" <"$f" >/dev/null 2>&1
  rc=$?
  if [ "$rc" -ne 1 ]; then
    echo "FAIL  $f (rc=$rc, expected 1)"
    fail=$((fail + 1))
  else
    pass=$((pass + 1))
  fi
done

echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]
