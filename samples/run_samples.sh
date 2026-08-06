#!/usr/bin/env bash
# Smoke-runs every sample: each must exit 0 and produce output. Keeps the
# showcase programs from silently rotting as the language evolves.
set -u

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="${BIN:-$DIR/../ember}"

if [ ! -x "$BIN" ]; then
  echo "error: interpreter not found at $BIN (run 'make' first)" >&2
  exit 1
fi

fail=0
for file in "$DIR"/*.em; do
  name="$(basename "$file")"
  out="$("$BIN" "$file" 2>&1)"
  status=$?
  if [ "$status" -ne 0 ] || [ -z "$out" ]; then
    echo "FAIL $name (exit $status)"
    echo "$out" | sed 's/^/    /'
    fail=1
  else
    echo "PASS $name ($(echo "$out" | wc -l | tr -d ' ') lines)"
  fi
done

exit "$fail"
