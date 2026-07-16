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
  for run in 1 2 3; do
    "$BIN" "$file" | sed "s/^/  run$run: /"
  done
done
