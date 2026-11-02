#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
CASINO_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [ ! -x "$CASINO_DIR/build/voncloft-casino" ]; then
  cmake -S "$CASINO_DIR" -B "$CASINO_DIR/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$CASINO_DIR/build" -j2
fi
exec "$CASINO_DIR/build/voncloft-casino" "$@"
