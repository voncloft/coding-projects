#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"

# Plasmoid package under ./test/share
export XDG_DATA_DIRS="$ROOT/test/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

# QML import path: support common KDE/Qt6 layouts
if [ -d "$ROOT/test/lib64/qml" ]; then
  export QML2_IMPORT_PATH="$ROOT/test/lib64/qml:${QML2_IMPORT_PATH:-}"
elif [ -d "$ROOT/test/lib/qml" ]; then
  export QML2_IMPORT_PATH="$ROOT/test/lib/qml:${QML2_IMPORT_PATH:-}"
elif [ -d "$ROOT/test/lib64/qt6/qml" ]; then
  export QML2_IMPORT_PATH="$ROOT/test/lib64/qt6/qml:${QML2_IMPORT_PATH:-}"
elif [ -d "$ROOT/test/lib/qt6/qml" ]; then
  export QML2_IMPORT_PATH="$ROOT/test/lib/qt6/qml:${QML2_IMPORT_PATH:-}"
else
  echo "ERROR: cannot find QML import dir under ./test"
  echo "Found these candidates:"
  find "$ROOT/test" -maxdepth 4 -type d -name qml -print || true
  exit 1
fi

plasmawindowed org.nick.radicaleagenda
