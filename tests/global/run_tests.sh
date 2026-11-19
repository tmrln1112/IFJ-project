#!/bin/sh
# Usage: sh tests/global/run_tests.sh [compiler [IFJcode26 interpreter]]
# Without an interpreter, check compilation only.

dir=$(dirname "$0")
compiler=${1:-"$dir/../../ifj26"}
interpreter=${2:-}

for binary in "$compiler" ${interpreter:+"$interpreter"}; do
  if ! command -v "$binary" >/dev/null 2>&1; then
    printf 'Executable not found: %s\n' "$binary" >&2
    exit 2
  fi
done

tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' 0
trap 'exit 2' HUP INT TERM

passed=0
failed=0
skipped=0

for group in ok compile_error runtime_error; do
  for source in "$dir/$group/"*.ifj26; do
    [ -f "$source" ] || continue
    name=$group/${source##*/}
    if [ "$group" = runtime_error ] && [ -z "$interpreter" ]; then
      skipped=$((skipped + 1))
      continue
    fi

    "$compiler" <"$source" >"$tmp/program.ifjcode" 2>"$tmp/stderr"
    status=$?
    if [ "$group" = compile_error ]; then
      case $status in
        1|2|3|4|5|6|7|10) ;;
        *)
          printf 'FAIL %s (compiler exit %s, expected a source error)\n' "$name" "$status"
          cat "$tmp/stderr" >&2
          failed=$((failed + 1))
          continue
          ;;
      esac
    elif [ "$status" -ne 0 ]; then
      printf 'FAIL %s (compiler exit %s, expected 0)\n' "$name" "$status"
      cat "$tmp/stderr" >&2
      failed=$((failed + 1))
      continue
    elif [ -n "$interpreter" ]; then
      fixture=${source%.ifj26}
      input=/dev/null
      [ ! -f "$fixture.in" ] || input=$fixture.in
      case $name in
        runtime_error/divide_by_zero.ifj26) expected=57 ;;
        runtime_error/substring_*.ifj26) expected=40 ;;
        runtime_error/*)
          printf 'FAIL %s (expected runtime exit is missing)\n' "$name"
          failed=$((failed + 1))
          continue
          ;;
        *) expected=0 ;;
      esac
      "$interpreter" "$tmp/program.ifjcode" <"$input" >"$tmp/stdout" 2>"$tmp/stderr"
      status=$?
      if [ "$status" -ne "$expected" ]; then
        printf 'FAIL %s (interpreter exit %s, expected %s)\n' "$name" "$status" "$expected"
        cat "$tmp/stderr" >&2
        failed=$((failed + 1))
        continue
      fi
      if ! diff -u "$fixture.out" "$tmp/stdout"; then
        printf 'FAIL %s (output differs)\n' "$name"
        failed=$((failed + 1))
        continue
      fi
    fi

    passed=$((passed + 1))
  done
done

if [ -z "$interpreter" ]; then
  printf 'Compilation only (no interpreter supplied).\n'
fi
printf 'passed: %s, failed: %s, skipped: %s\n' "$passed" "$failed" "$skipped"
[ "$failed" -eq 0 ] && [ "$passed" -gt 0 ]
