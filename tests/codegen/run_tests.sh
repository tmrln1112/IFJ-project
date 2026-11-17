#!/bin/sh
# Regression tests for IFJcode26 constant printing (codegen helpers).
#
#   tests/codegen/ok/<function>/*.in   input: stdout must equal *.out
#
# Usage: sh tests/codegen/run_tests.sh [path/to/codegen_test]

BIN=${1:-./codegen_test}
DIR=$(dirname "$0")
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

pass=0
fail=0

if [ ! -x "$BIN" ]; then
  echo "binary '$BIN' not found, build it first" >&2
  exit 2
fi

for dir in "$DIR"/ok/*/; do
  func=$(basename "$dir")
  for f in "$dir"*.in; do
    "$BIN" "$func" <"$f" >"$TMP" 2>/dev/null
    if ! diff -u "${f%.in}.out" "$TMP" >/dev/null; then
      echo "FAIL  $f (output differs)"
      diff -u "${f%.in}.out" "$TMP" | head -n 20
      fail=$((fail + 1))
    else
      pass=$((pass + 1))
    fi
  done
done

echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]
