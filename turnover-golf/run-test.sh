#!/bin/sh
set -eu
turnover_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -x "$turnover_dir/build/turnover-golf" ]; then
    turnover_binary="$turnover_dir/build/turnover-golf"
elif [ -x "$turnover_dir/turnover-golf" ]; then
    turnover_binary="$turnover_dir/turnover-golf"
else
    printf '%s\n' 'Build Turnover Golf first, from this folder:' \
        '  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release' \
        '  cmake --build build -j4' >&2
    exit 1
fi
export XDG_DATA_HOME="$turnover_dir/test-profile/data"
export XDG_CONFIG_HOME="$turnover_dir/test-profile/config"
export XDG_STATE_HOME="$turnover_dir/test-profile/state"
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_STATE_HOME"
exec "$turnover_binary" "$@"
