#!/usr/bin/env bash
# Runs every tests/*.em file and compares actual output against the
# `// expect: <value>` annotations in the file, in order.
set -u

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="${BIN:-$DIR/../ember}"

if [ ! -x "$BIN" ]; then
  echo "error: interpreter not found at $BIN (run 'make' first)" >&2
  exit 1
fi

pass=0
fail=0

for file in "$DIR"/*.em; do
  name="$(basename "$file")"
  expected="$(grep -o '// expect: .*' "$file" | sed 's|// expect: ||')"
  actual="$("$BIN" "$file" 2>&1)"
  if [ "$expected" = "$actual" ]; then
    echo "PASS $name"
    pass=$((pass + 1))
  else
    echo "FAIL $name"
    echo "--- expected ---"
    echo "$expected"
    echo "--- actual ---"
    echo "$actual"
    echo "---"
    fail=$((fail + 1))
  fi
done

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
