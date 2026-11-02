# Selected Velvet Casino game audio

These seven recordings are the default audio in Velvet Casino 1.0.18.
They are the exact normalized WAV files used by the original local build,
now included in `assets/` so normal source and package builds sound alike.
Original downloads were obtained on 2026-11-02 through the source pages.
The audio retains the **Pixabay Content License**, separately from GPL or CC0.

| Cue | Recording | Creator credited by Pixabay |
| --- | --- | --- |
| Bet / selected chip | [Placing poker chips, 522521](https://pixabay.com/sound-effects/film-special-effects-placing-poker-chips-522521/) | OxidVideos |
| All in | [AllInPushChips, 96121](https://pixabay.com/sound-effects/film-special-effects-allinpushchips-96121/) | joeyed (Freesound), via freesound_community |
| Payout | [poker_chips1, 87592](https://pixabay.com/sound-effects/film-special-effects-poker-chips1-87592/) | fartheststar (Freesound), via freesound_community |
| Deal one card | [Taking playing card, 522520](https://pixabay.com/sound-effects/film-special-effects-taking-playing-card-522520/) | OxidVideos |
| Shuffle | [Shuffling Deck of Cards, 522518](https://pixabay.com/sound-effects/film-special-effects-shuffling-deck-of-cards-522518/) | OxidVideos |
| Tap deck after shuffle | [Pounding Cards On Table, 99355](https://pixabay.com/sound-effects/film-special-effects-pounding-cards-on-table-99355/) | HogantheLogan (Freesound), via freesound_community |
| Setup intro | [8bit Music for Game, 68698](https://pixabay.com/sound-effects/musical-8bit-music-for-game-68698/) | annoyedCactus (Freesound), via freesound_community |

AllInPushChips was selected from the user's poker-chips search link because it
specifically records pushing chips forward for an all-in bet.

## Distribution with the game

[License summary](https://pixabay.com/service/license-summary/) and
[full terms, section 5](https://pixabay.com/service/terms/) checked 2026-11-02.
The license allows incorporating recordings into creative projects and
prohibits distributing them on their own. The WAVs are functional assets
integrated with Velvet Casino's game actions and setup screen. Keep this
notice with the complete game; do not publish these files as a separate
sound collection. Credit does not imply endorsement by the creators or Pixabay.
The application code remains GPL-3.0-or-later; the recordings are not relicensed.

## Provenance and reproduction

`assets/provenance.json` records original download filenames, source SHA-256
hashes, output WAV hashes, durations, trimming and gain. Each default WAV
matches the previously selected local pack byte for byte, including the intro.
All seven files are embedded during an ordinary build. No custom CMake option,
network access, Python, FFmpeg, or ignored local directory is needed to build
or run the game.

For asset maintenance only, download the seven originals from the source pages
and run `python3 tools/import_pixabay_sounds.py /path/to/downloads --output assets`.
The converter uses FFmpeg to decode mono 44,100 Hz signed 16-bit PCM, trim only
outer silence with short margins and normalize each peak to 82 percent.
It preserves the recordings rather than synthesizing or pitch-shifting them.
Original MP3 downloads are not included. The historical `assets/local-sounds/`
copy remains ignored and is not consulted by normal builds.

## Alternate CC0 recordings

The retained Kenney source effects and the separate Freesound source of the
intro are optional alternatives, not the game's defaults. Their notices are
in `Kenney-Casino-Audio.txt` and `Freesound-Intro.txt`. Running
`tools/prepare_chip_sounds.py` and `tools/prepare_intro.py` generates that
alternate pack in ignored `build/cc0-sounds/` without changing `assets/*.wav`.
An explicitly supplied `CASINO_SOUND_DIR` still selects a complete custom pack;
ordinary builds and the existing package recipe need no such setting.
