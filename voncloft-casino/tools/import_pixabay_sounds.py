#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 voncloft-casino contributors
"""Regenerate the game's selected WAV assets from separately downloaded originals.

Use the download buttons on the source pages in licenses/Pixabay-Sounds.md.
Requires Python 3 and FFmpeg for conversion only; neither is bundled by this tool.
"""
import argparse
from array import array
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import wave

SOUNDS = {
    'chip-bet': 'oxidvideos-placing-poker-chips-522521.mp3',
    'chip-allin': 'freesound_community-allinpushchips-96121.mp3',
    'chip-payout': 'freesound_community-poker_chips1-87592.mp3',
    'card-deal': 'oxidvideos-taking-playing-card-522520.mp3',
    'card-shuffle': 'oxidvideos-shuffling-deck-of-cards-522518.mp3',
    'intro': 'freesound_community-8bit-music-for-game-68698.mp3',
    'deck-tap': 'freesound_community-pounding-cards-on-table-99355.mp3',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('downloads', type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'assets')
    args = parser.parse_args()
    missing = [name for name in SOUNDS.values() if not (args.downloads / name).is_file()]
    if missing:
        parser.error('Missing downloads: ' + ', '.join(missing))
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for cue, name in SOUNDS.items():
        source = args.downloads / name
        raw = subprocess.check_output(['ffmpeg', '-nostdin', '-v', 'error', '-i', str(source),
                                       '-ac', '1', '-ar', '44100', '-f', 's16le', '-c:a', 'pcm_s16le', '-'])
        samples = array('h', raw)
        if sys.byteorder != 'little':
            samples.byteswap()
        peak = max(map(abs, samples), default=0)
        if not peak:
            raise ValueError(f'{name} contains no audible samples')
        # Only trim outer silence; preserve each recorded effect and its natural tail.
        threshold = max(24, int(peak * 0.002))
        active = [i for i, sample in enumerate(samples) if abs(sample) > threshold]
        first = max(0, active[0] - 662)
        end = min(len(samples), active[-1] + 3529)
        gain = 0.82 * 32767 / peak
        pcm = array('h', (round(sample * gain) for sample in samples[first:end]))
        if sys.byteorder != 'little':
            pcm.byteswap()
        target = args.output / (cue + '.wav')
        with wave.open(str(target), 'wb') as output:
            output.setparams((1, 2, 44100, 0, 'NONE', 'not compressed'))
            output.writeframes(pcm.tobytes())
        manifest[cue] = {'source': name, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                         'wav_sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
                         'duration_seconds': round(len(pcm) / 44100, 4), 'gain': round(gain, 5),
                         'trim_start_seconds': round(first / 44100, 5)}
        print(f'{name} -> {target.name}: {len(pcm)/44100:.3f}s')
    (args.output / 'provenance.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
