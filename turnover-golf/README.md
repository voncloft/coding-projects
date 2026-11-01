# Turnover Golf 1.0.10

A native C++/Qt 6 card game for KDE, with 2–4 players, computers and online humans.
The working project is `/home/von/Desktop/coding-projects/turnover-golf`.
Its own upstream remote is `git@github.com:voncloft/Turnover-golf.git`.

## Settings

**Settings → Options** opens one spacious, scrollable dialog with five tabs:

- **Players & rounds:** 2–4 total players, computer/online-human seats, 1–99 rounds,
  and hosting details.
- **Cards:** 4×2 (8 cards), 3×3 (9 cards), or 4×4 (16 cards), matching mode,
  optional complete-row cancellation for 3×3/4×4, and 1–8 decks of 108 cards each. Layout dimensions mean columns × rows.
- **Computer:** finishing targets of 0, −5, −10, −20, 3, or 6 points or lower;
  plus fast or normal pacing. The default target is 0 or lower.
- **Appearance:** 12 front themes and 12 back themes chosen independently, with
  a live preview: Classic, Midnight, Ocean, Ruby, Forest, Ivory, Slate, Copper,
  Violet, Sunrise, Blueprint, and High contrast.
- **Join online:** guest name, host address, port, and room code.

**New game**, **Play again**, and **Ctrl+N** reuse your last choices without opening
player setup. Options are remembered between launches. Appearance, pacing, round
count and CPU target can change without restarting the current match. Completed
scores are preserved, and rounds cannot be reduced below the current round.
Changes to players, layout, matching mode or deck count start a new match; the
button and explanatory text make this explicit. Cancel changes nothing.
Online guests can change their own appearance and pacing; the host controls rules.

## House rules

1. Deal every card face down. Turn all but one face up before drawing: 7 of 8,
   8 of 9, or 15 of 16. Leave the final card in its original position.
2. Draw from the stock or take the previous player's top discard. Replace a
   face-up card and put the replaced card on the discard pile. A taken discard
   must be used; a stock draw may instead be discarded.
3. On your turn, flip your final card without drawing, or replace it with a drawn
   card. Either action ends the round immediately: everyone reveals and scores.
   There are no extra turns. Once you draw, resolve that draw first.
4. When the stock runs out, shuffle used discards back into it, keeping the top
   discard available. Play continues until someone ends the round.
5. The starting seat rotates each round. Lowest cumulative score wins; ties share
   the win. You always sit at the bottom and all hands face their own seats.

Each deck contains eight cards of each value 0–12 and four −5 cards. Multiple
decks keep the same proportions, with unique card identities throughout a round.

**Whole-column matching:** every card in a column must have the same value to
cancel it (2, 3 or 4 cards depending on layout).

**Pair matching:** equal pairs within a column cancel, even when nonadjacent.
Unpaired cards count normally. Four equal cards make two pairs.

−5 cards always retain their negative values. In either mode, two or more
matching groups of the same value earn an additional −5 per group. A group is a
whole column or a pair according to the selected mode. Columns need not be
adjacent. Without the row option, side-by-side cards alone do not cancel.

Both modes give the original scoring on a 4×2 hand: two, three or four same-value
vertical pairs earn −10, −15 or −20 bonuses; four −5 cards in two vertical pairs
contribute −30 before adding other columns. Larger layouts are custom variants.

Row cancellation is a documented [nine-card Golf variation](https://www.pagat.com/draw/golf.html#nine),
but these larger-layout bonus rules are **Turnover Golf house rules**, not official
Play Nine rules. The [Play Nine rule sheet](https://library.livermoreca.gov/home/showpublisheddocument/12961/638890453622070000)
uses eight cards; extending its bonuses to 3×3, 4×4 and intersecting lines is custom.

**Optional row matching (3×3 / 4×4):** enable **Also cancel complete matching
rows** on the Cards tab. Every card in a horizontal row must have the same value.
This works with either column mode. Each complete row adds one matching group
for bonuses; groups of the same value are counted together. A card at an
intersection only cancels once, and each −5 card retains its value once. The
option defaults off. Changing it starts a new match and is remembered for future
games; online games use the host's choice.

With whole-column matching, two complete columns of the **same** value 0–12
contribute −10. Two complete columns filled with −5 contribute:

| Layout | −5 cards in those columns | Card values | Bonus | Contribution |
| --- | ---: | ---: | ---: | ---: |
| 4×2 | 4 | −20 | −10 | **−30** |
| 3×3 | 6 | −30 | −10 | **−40** |
| 4×4 | 8 | −40 | −10 | **−50** |

Add the other cards and any additional matching groups to get the hand's total.
One deck has only four −5 cards, so six or eight require at least two decks.
In 4×4 **pair mode**, two all−5 columns form four pairs: −40 in card values and
−20 in bonuses, contributing **−60** before other cards or row matches. Columns
of different values can each cancel without earning a same-value bonus together.

## Computer play

Computers only use revealed information. They develop their hand for at least
six turns, look ahead for matches and bonuses, and favor lower scores, including
negative scores. They only voluntarily finish at or below the configured target
when their visible-information estimate also favors beating the other hands.
Flipping the final card requires every still-possible hidden value to satisfy the
target. Stricter targets may make rounds longer. A human or another computer can
still end the round while an opponent has a higher score.

## Online play

Choose Online human seats in Options to host a game. Share **Connection details**
with guests. Guests select **Game → Join online game** or the Options dialog's
**Join online** tab. A mixture of computers and humans is supported. The host
controls layout, decks, matching, CPU target and rounds; guests receive matching
rules and scores without hidden values or deck order.

Use a local address on the same network, or a VPN/reachable public address with
the host TCP port (default 45839) forwarded for internet play. The application
does not configure routers or provide matchmaking. Connections use plain TCP.
Both sides need compatible builds; version 1.0.10 uses network protocol 3.

Disconnection pauses play. Reconnect from the original guest window to retain
its seat. Closing that guest window loses its reconnect token. Keep the host
open for the match. Online matches do not replace offline saves.

## Build and test

Requires CMake 3.20+, C++17, and Qt 6.4+ Core, Widgets, Network and (for tests) Test.

```sh
git clone git@github.com:voncloft/Turnover-golf.git turnover-golf
cd turnover-golf
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
./run-test.sh
```

No installation is needed. `run-test.sh` prefers the current `build/turnover-golf`
and keeps local preferences and saves under `test-profile/`. To use normal desktop
save locations, run `./build/turnover-golf` directly. Existing version-1.0.8 offline
saves load as the original 4×2, one-deck rules and keep their previous CPU limit.
Version-1.0.9 saves also load with row matching off. New saves retain layout,
matching, row cancellation, deck count and CPU target.

Click cards or use their displayed keys: 1–9, then A, B, C, E, F, G, H. D draws,
T takes the discard, X discards a stock draw, Ctrl+N starts a new match and F1 opens
rules. The installed application is not changed by building or using the launcher.

The Scratchpkg recipe remains in `Voncloft-OS-dev/kde-apps/turnover-golf/`, outside
this source tree. It tracks published commits from the standalone golf repository.
The main coding-projects backup also tracks these source files and ignores this
folder's separate `.git/` metadata.

## License

GNU General Public License version 3; see [LICENSE](LICENSE).

## Changelog

### 1.0.10

Added optional complete-row cancellation for 3×3 and 4×4 on the Cards tab,
including matching bonuses, intersection handling, card highlights, CPU scoring,
persistent options and online synchronization. Saved games from 1.0.8 and 1.0.9
retain their original scoring. Updated in-game rules with explicit −5-column
examples. Online players need compatible protocol-3 builds.

### 1.0.9

Added persistent, tabbed Options with three hand layouts, two matching modes,
1–8 decks, configurable CPU finishing targets, and 12 independent front/back
card themes with previews. New Game reuses the previous choices. Updated scoring,
AI, save validation, network synchronization, card orientations and shortcuts
for 8, 9 and 16 cards. Renamed the window and desktop entry to Turnover Golf and
made the test launcher prefer the newly built executable.

### 1.0.8

Removed solo play. Setup offers 2–4 total players, with at least one computer or online human opponent. Old solo preferences default to a computer opponent, and old solo saves start a fresh match with an opponent. Updated the rules, About text, and desktop description.

### 1.0.7

The round-complete message now names the round winner and their score. After the final round, it names the overall winner using all round scores and shows your final total. Tied winners are all named, including online players. The scorecard leaves room for wrapped winner messages.

### 1.0.6

Prepared the source repository with SSH clone instructions, build-output and save exclusions, and a test launcher that works after a fresh source build. Qt Test is now optional for builds with tests disabled. Updated desktop metadata to describe 1–4 total players and online play. Preserved the repository's existing GPLv3 license.

### 1.0.5

Added a clearly labeled total-player selector in Settings → Players, with 1–4 players including you, solo practice, and computer/online-human choices per seat. Hands now surround the central piles in north/south, north/south/west, or north/south/west/east layouts. Each hand faces its own seat and the local player always sits south. Table labels show player names and scores without compass-direction words. Updated house rules allow flipping the final card to end a round without drawing, as well as replacing it with a drawn card. Setup pages now scroll and accommodate larger desktop fonts.

### 1.0.4

Fixed the squashed Settings → Rounds dialog. Its width and minimum height now follow the desktop font and wrapped text, with room around the controls and buttons.

### 1.0.3

Added Settings → Rounds, per-seat computer/online-human selection, direct-IP hosting and joining, room codes, synchronized scorecards, mixed tables, and reconnection from the same guest window. Added stock recycling while keeping the top discard. Improved computer strategy to pursue pairs, matching bonuses, and low scores instead of ending early with a poor hand. Added rule, strategy, TCP, and native setup tests.

### 1.0.2

Added a 1–99 round selector. Each final-card replacement completes one round; the match ends at the selected round count.

### 1.0.1

Clarified final-card replacement: the drawn card replaces the last face-down card and ends the round immediately for all players. Fixed dark-theme menu contrast.

### 1.0.0

Initial native Qt 6 release with original card artwork, one to four AI opponents, seven-up/one-face-down setup, simultaneous final reveal, matching bonuses, scorecards, keyboard controls, and automatic save/resume.
