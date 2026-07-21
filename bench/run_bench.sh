#!/usr/bin/env bash
# Runs each benchmark 3 times per tier (JIT enabled vs interpreter-only,
# both forced hot so tier-up cost is included) and reports best-of-3 plus
# the speedup. On platforms without JIT support the two columns match.
set -u

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="${BIN:-$DIR/../ember}"

if [ ! -x "$BIN" ]; then
  echo "error: interpreter not found at $BIN (run 'make' first)" >&2
  exit 1
fi

best_of_3() {  # $1 = EMBER_JIT value, $2 = file; echoes best elapsed_s
  local best=""
  for _ in 1 2 3; do
    local t
    t="$(EMBER_JIT=$1 EMBER_JIT_THRESHOLD=1 "$BIN" "$2" \
         | awk '/^elapsed_s / {print $2}')"
    if [ -n "$t" ]; then
      if [ -z "$best" ] || awk "BEGIN{exit !($t < $best)}"; then
        best="$t"
      fi
    fi
  done
  echo "$best"
}

for file in "$DIR"/*.em; do
  name="$(basename "$file")"
  echo "== $name =="
  EMBER_JIT=1 EMBER_JIT_THRESHOLD=1 "$BIN" "$file" | grep -v '^elapsed_s ' \
    | sed 's/^/  /'
  jit="$(best_of_3 1 "$file")"
  interp="$(best_of_3 0 "$file")"
  if [ -n "$jit" ] && [ -n "$interp" ]; then
    printf "  interp: %ss\n  jit:    %ss\n" "$interp" "$jit"
    awk "BEGIN{printf \"  speedup: %.2fx\n\", $interp / $jit}"
  fi
done
