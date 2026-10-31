#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="$project_root/build/todolist"

if [[ ! -x "$binary" ]]; then
  printf 'Built binary not found at %s\n' "$binary" >&2
  printf 'Run: cmake -S %q -B %q && cmake --build %q\n' "$project_root" "$project_root/build" "$project_root/build" >&2
  exit 1
fi

exec "$binary" "$@"
