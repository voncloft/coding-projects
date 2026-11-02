#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 voncloft-casino contributors
"""Convert the alternate CC0 Freesound preview to build/cc0-sounds/intro.wav.

Requires FFmpeg only for this optional conversion, not for normal builds.
This does not overwrite the game's selected default intro.
See licenses/Freesound-Intro.txt for source, author and license information.
"""
from array import array
import hashlib
from pathlib import Path
import subprocess
import sys
import wave


def main():
    root = Path(__file__).resolve().parents[1]
    source = root / 'assets/source/455017_9182239-hq.mp3'
    target = root / 'build/cc0-sounds/intro.wav'
    target.parent.mkdir(parents=True, exist_ok=True)
    raw = subprocess.check_output([
        'ffmpeg', '-nostdin', '-v', 'error', '-i', str(source),
        '-ac', '1', '-ar', '44100', '-f', 's16le', '-c:a', 'pcm_s16le', '-'])
    samples = array('h', raw)
    if sys.byteorder != 'little':
        samples.byteswap()
    peak = max(map(abs, samples), default=0)
    if not peak:
        raise ValueError('The source intro contains no audible samples')
    threshold = max(24, int(peak * 0.002))
    active = [i for i, sample in enumerate(samples) if abs(sample) > threshold]
    first = max(0, active[0] - 662)
    end = min(len(samples), active[-1] + 3529)
    gain = 0.82 * 32767 / peak
    pcm = array('h', (round(sample * gain) for sample in samples[first:end]))
    if sys.byteorder != 'little':
        pcm.byteswap()
    with wave.open(str(target), 'wb') as output:
        output.setparams((1, 2, 44100, 0, 'NONE', 'not compressed'))
        output.writeframes(pcm.tobytes())
    print(f'{target.name}: {len(pcm) / 44100:.4f}s, gain {gain:.5f}')
    for path in (source, target):
        print(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(root)}')


if __name__ == '__main__':
    main()
