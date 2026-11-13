#!/bin/sh
# Regression tests for the IFJ26 scanner.
#
#   tests/ok/*.ifj   valid input: rc must be 0 and stdout must equal *.out
#   tests/err/*.ifj  invalid input: rc must be 1 (stdout is not compared)
#   tests/todo/*.ifj known bugs: should give rc 1 but do not yet.
#                    rc != 1 -> XFAIL (expected, does not fail the run)
#                    rc == 1 -> XPASS (bug fixed, move the file to err/)
#
# Usage: sh tests/run_tests.sh [path/to/ifj26]

BIN=${1:-./ifj26}
DIR=$(dirname "$0")
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

pass=0
fail=0
xfail=0
xpass=0

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

for f in "$DIR"/todo/*.ifj; do
  "$BIN" <"$f" >/dev/null 2>&1
  rc=$?
  if [ "$rc" -eq 1 ]; then
    echo "XPASS $f (now rc=1, move it to err/)"
    xpass=$((xpass + 1))
  else
    echo "XFAIL $f (known bug, rc=$rc)"
    xfail=$((xfail + 1))
  fi
done

echo "passed: $pass, failed: $fail, known bugs: $xfail, fixed bugs: $xpass"
[ "$fail" -eq 0 ]
