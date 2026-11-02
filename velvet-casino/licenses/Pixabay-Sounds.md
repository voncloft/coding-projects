# Optional Pixabay table sounds

These are the recordings selected for the local Velvet Casino 1.0.16 build.
Downloaded on 2026-11-02 using each page's download button. They are separate
assets under the **Pixabay Content License**, not GPL-3.0-or-later or CC0.

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

## Terms and distribution

[Pixabay license summary](https://pixabay.com/service/license-summary/) and
[full terms, section 5](https://pixabay.com/service/terms/) were checked on
2026-11-02. The terms permit use and adaptation in creative works, including
commercial projects, without mandatory attribution, but prohibit distribution
of the content on a standalone basis. Attribution is supplied here anyway.
The source pages identify these files as available under that license; they
are not treated as public domain merely because downloading is free.

The custom executable incorporates the sounds into interactive game actions.
The downloaded MP3s and converted WAVs must not be published as a standalone
sound pack. In particular, this repository keeps `assets/local-sounds/` ignored
and supplies CC0 substitutes for its public source build. Do not force-add the
local pack to Git. The application source remains GPL-3.0-or-later. Keep this
notice with a custom build; no creator or Pixabay endorsement is implied.

## Reproduce the local pack

Download all seven files from the linked pages into one directory, retaining
Pixabay's filenames. Then run from the project directory:

```sh
python3 tools/import_pixabay_sounds.py /path/to/downloaded-mp3s
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCASINO_SOUND_DIR="$PWD/assets/local-sounds"
cmake --build build -j2
```

The importer requires a local FFmpeg executable but does not bundle it. It
decodes to mono 44,100 Hz signed 16-bit PCM, removes only outer silence with
short margins, and adjusts each recording's peak to 82% for consistent levels.
It does not synthesize or pitch-shift the recordings. The game loops the intro only while the setup dialog remains open. Original downloads
remain untouched. `assets/local-sounds/provenance.json` records source/output
SHA-256 checksums, timing and gain. Normal builds need neither Python nor FFmpeg.

To restore the public CC0 pack in an existing build:

```sh
cmake -S . -B build -DCASINO_SOUND_DIR=
cmake --build build -j2
```

The public CC0 build includes the six table effects and no intro music.

An incomplete custom pack fails configuration rather than silently substituting
some recordings. All seven selected WAVs are embedded; no network access or loose
audio files are required when running the built game.
