# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Velvet Casino contributors
"""Convert the included CC0 Kenney recordings to the app's embedded PCM WAVs.

Run with Python 3 and FFmpeg. This is an asset-maintenance tool; normal builds
use the checked-in WAV files and do not run or bundle the FFmpeg executable.
"""
from pathlib import Path
import subprocess

assets = Path(__file__).resolve().parents[1] / 'assets'
for source, target in [('chips-collide-1.ogg', 'chip-bet.wav'),
                       ('chips-handle-1.ogg', 'chip-payout.wav'),
                       ('chips-handle-2.ogg', 'chip-allin.wav'),
                       ('card-slide-1.ogg', 'card-deal.wav'),
                       ('card-shuffle.ogg', 'card-shuffle.wav'),
                       ('card-place-1.ogg', 'deck-tap.wav')]:
    subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-y',
                    '-i', str(assets / 'source' / source), '-map_metadata', '-1',
                    '-ac', '1', '-ar', '44100', '-c:a', 'pcm_s16le',
                    str(assets / target)], check=True)
    print(f'{source} -> {target}')
