#!/usr/bin/env bash
# Runs each benchmark 3 times and reports every elapsed_s line.
set -u

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="${BIN:-$DIR/../ember}"

if [ ! -x "$BIN" ]; then
  echo "error: interpreter not found at $BIN (run 'make' first)" >&2
  exit 1
fi

for file in "$DIR"/*.em; do
  name="$(basename "$file")"
  echo "== $name =="
  best=""
  for run in 1 2 3; do
    out="$("$BIN" "$file")"
    echo "$out" | sed "s/^/  run$run: /"
    t="$(echo "$out" | awk '/^elapsed_s / {print $2}')"
    if [ -n "$t" ]; then
      if [ -z "$best" ] || awk "BEGIN{exit !($t < $best)}"; then
        best="$t"
      fi
    fi
  done
  [ -n "$best" ] && echo "  best:  ${best}s"
done
