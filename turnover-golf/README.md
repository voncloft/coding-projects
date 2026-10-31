# Turnover Golf 1.0.8

A native C++/Qt 6 Widgets card game for KDE. Choose **2–4 total players, including you**. Play against computers or with a mix of online humans and computers.

## Play

Build from source using the commands below, then run `./run-test.sh` from this folder. No installation is required. The launcher uses `build/turnover-golf`, or the executable beside it in a local test bundle, and keeps saves and preferences in `test-profile/`. Compiled binaries and personal saves are excluded from Git.
Choose **Settings → Players…** or **Players / New game** to set the **total player count** and each other seat to **Computer** or **Online human**. For example, four players can be you plus three computers, or you plus one online human and two computers. Three players can be you plus two computers. Set **Rounds** to any number from 1 to 99 (default 9). Each completed hand of eight cards is one round; cumulative scores determine the match winner. **Settings → Rounds…** also changes the current match total and saves it as the default for future games. Completed scores are kept; the total cannot be lower than the current round. The host controls this setting for online matches.

This version uses the requested house rules:

1. All eight cards are dealt face down in two rows of four, for every player.
2. Turn **seven cards face up before drawing**. The remaining card stays face down, in its original position.
3. Draw from the deck or take the top discard. Choose one pile before drawing; you cannot peek and switch piles or draw twice. Replace any face-up card, putting the replaced card face up on the discard pile for the next player. You may instead discard a deck draw to keep playing. A taken discard must be used.
4. On your turn, you can end the round **without drawing**: click your last face-down card or **Flip final card · end round**. It turns face up and keeps its original value.
5. You can also draw from either pile and replace your last face-down card with the draw to end the round. Its old card is discarded. Once you have drawn, resolve that draw before the turn ends.
6. Either way of ending immediately makes everyone else turn their remaining face-down card face up and score. There are no extra turns. This applies to every human and computer.

The deck contains eight cards of each value 0–12 and four cards worth −5. Equal cards directly above and below one another score zero, except −5 cards retain their negative values. Two, three, or four vertical pairs of the same number earn a −10, −15, or −20 bonus. Four −5 cards in two vertical pairs score −30 before adding other columns. Columns need not be adjacent.

All completed rounds count toward the match total; lowest wins. Ties share the win. If the draw pile runs out, all used discards except the top one are shuffled into a new draw pile. The top discard remains available. Play continues with the last cards face down until someone chooses to end the round. The first player rotates each round. You always sit **south**. Two players occupy north/south, three occupy north/south/west, and four occupy all four sides. Every hand is oriented toward its seat, with its two rows rotated as a whole. This also applies to online guests: their own hand is always south. The computer sees only revealed cards and estimates face-down values from the remaining card counts. It develops its hand for at least six turns, looks a draw ahead for matching opportunities, and seeks bonuses and a near-zero or negative finishing score. It can also end without drawing when every still-possible value of its face-down card would give a reasonable finish. It will not choose to end above 6 points, even if other hands look worse. Another player can still end the round while a computer has a high score.

Card values and matching scores are based on [Play Nine](https://playnine.com/pages/how-to-play). Setup and the simultaneous final reveal follow the user's house rules, which differ from the publisher's standard game. Artwork is drawn locally with Qt.

## Online play

1. The host selects **Settings → Players… → Set up game**, sets the player and round counts, and changes any opponent to **Online human**. Remaining seats can be computers.
2. Select **Host game**, then **Connection details**. Share the host address, TCP port (default 45839), and room code with the other players.
3. Guests run the same version and select **Players / New game → Join online game**. Enter a name and the host's address, port, and room code.
4. Play starts when every chosen human seat connects. Each player opens seven cards on their turn. Each person's own hand appears at the bottom. The host advances to the next round.

On the same network, use the host's local address. Across the internet, use a reachable VPN address or the host's public address with the chosen TCP port forwarded to the host computer and allowed through its firewall. The app does not configure routers or provide matchmaking. Both players need compatible builds. The application is developed and tested on Linux/KDE.

The host validates moves and sends only face-up card values. Other players' face-down cards, deck order, and private stock draws are not sent to guests. The host computer manages the full game. Connections use plain TCP; a VPN can provide encryption.

A disconnected player pauses the match for everyone. Rejoin with **Connection details → Reconnect** in the original guest window to retain the seat and any drawn card. Closing that guest window loses its reconnect token. Keep the host window open for the match: online matches are not restored after the host closes. Online play leaves your offline save intact.

## Controls

- Click a card, or use **1–4** for the top row and **5–8** for the bottom.
- **D** draws from the deck; **T** takes the discard; **X** discards your draw.
- **Ctrl+N** starts a new game; **F1** opens the rules.
- **Settings → Players…** selects the total player count and each seat type.
- **Settings → Rounds…** changes the number of rounds.
- **Settings → Fast computer turns** changes computer pacing.

Offline games save after each action and resume automatically on the next launch. On a typical Linux desktop, the save is `~/.local/share/Voncloft/TurnoverGolf/game.json`; window and opponent preferences are in `~/.config/Voncloft/TurnoverGolf.conf`.

## Build and test

Requires CMake 3.20+, a C++17 compiler, and Qt 6.4+ development files for Core, Widgets, Network, and Test. Built here against Qt 6.11.2. Qt Test is only required when `BUILD_TESTING` is enabled, which is the default.

```sh
git clone git@github.com:voncloft/Turnover-golf.git
cd Turnover-golf
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
./run-test.sh
```

The commands build and test locally without installing anything. Tests use Qt's offscreen platform and require no display server. For a build without tests, add `-DBUILD_TESTING=OFF` to the CMake configure command. To launch with normal desktop save locations instead of the isolated test profile, run `./build/turnover-golf` directly.

Offline play needs no network. Online play uses direct TCP connections through Qt Network; no web runtime, account, or relay service is used.

## Scratchpkg recipe

The package recipe and source checksum are maintained in [Voncloft-OS-dev](https://github.com/voncloft/Voncloft-OS-dev), under `kde-apps/turnover-golf/`. The recipe pins a published source commit and stages all packaged files under `$PKG/usr`. Its updater override tracks new Git commits and reads the application version from the same commit.

## License

The repository includes the GNU General Public License, version 3. See [LICENSE](LICENSE).

## Changelog

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
