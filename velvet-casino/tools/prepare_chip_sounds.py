# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Velvet Casino contributors
"""Convert the alternate CC0 Kenney recordings to build/cc0-sounds/.

Run with Python 3 and FFmpeg. This is an asset-maintenance tool; normal builds
use the selected checked-in WAV files in assets/ and do not run this tool.
This does not overwrite the game's selected default recordings.
"""
from pathlib import Path
import subprocess

assets = Path(__file__).resolve().parents[1] / 'assets'
output = assets.parent / 'build' / 'cc0-sounds'
output.mkdir(parents=True, exist_ok=True)
for source, target in [('chips-collide-1.ogg', 'chip-bet.wav'),
                       ('chips-handle-1.ogg', 'chip-payout.wav'),
                       ('chips-handle-2.ogg', 'chip-allin.wav'),
                       ('card-slide-1.ogg', 'card-deal.wav'),
                       ('card-shuffle.ogg', 'card-shuffle.wav'),
                       ('card-place-1.ogg', 'deck-tap.wav')]:
    subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-y',
                    '-i', str(assets / 'source' / source), '-map_metadata', '-1',
                    '-ac', '1', '-ar', '44100', '-c:a', 'pcm_s16le',
                    str(output / target)], check=True)
    print(f'{source} -> {target}')
