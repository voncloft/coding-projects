# voncloft-casino 1.0.19

A native Qt 6 desktop casino with play chips, computer opponents, and private network tables.

## Start

Run `./run.sh` to build if needed and launch. The startup dialog asks you to choose Blackjack or Poker. The local Linux executable is `build/voncloft-casino`.


For a computer-only table, set every active seat to **Computer**. For solo play, keep the first seat as **You on this computer** and fill the other seats with computers. Blackjack deals automatically as soon as every funded player has confirmed a bet. Computer-only blackjack tables run successive rounds automatically. Draw and stud deal automatically once every funded player has posted the ante; computers post theirs and human seats confirm their own. Hold’em retains **Auto-deal next hand**. Poker betting actions and streets always advance automatically. **Pause** stops the table.

## Games

- **Blackjack:** 1–5 players plus the dealer; choose **1–4 starting hands** before betting, then hit, stand, double down, or split equal ranks. Each original hand may split into up to four hands. **Settings → Blackjack decks** selects 1–8 decks (default 8), also available during setup. The shoe stays in play between rounds and is reshuffled when low or when a fresh reserve is needed for the next round and possible splits. A decorative shoe beside the dealer shows the remaining cards, empties one card at a time during dealing, and refills on a shuffle. Small shoes limit total starting/split hands so cards cannot run out mid-round. Changing decks between rounds returns pending bets and starts a fresh shoe. Dealer stands on all 17s. This table uses your house rules: split aces remain playable with hit, stand, double, or another eligible split; a two-card 21 after splitting counts as blackjack and pays 3:2, just like an initial blackjack. No insurance or surrender. Choose chips before each deal and **click the empty table felt to confirm**; the Place bet button also works. Use the **Hand** selector to choose a separate chip wager for each starting hand. Every hand needs a positive bet; the combined total comes from your shared bankroll. A single table click confirms all hands. **Clear hand** and right-click removal affect only the selected hand. Every human and network player confirms their own wager; computers choose theirs. Once all bets are ready, the cards deal automatically after the chip sound—there is no Deal button in blackjack. Left-click chips to add, right-click to remove. Odd blackjack wagers pay the exact half unit. During your turn, **left-click the empty table felt to Hit** or **right-click it to Stand**; buttons and keyboard shortcuts also work. Split and Double down stay visible, with tooltips explaining when unavailable. Doubling matches the hand’s bet, deals one more card, then stands; it is available on an eligible two-card hand, including after a split. Aces automatically count as 1 or 11: **A + 4 = 5 / 15**; adding an 8 makes **13**, while adding a 6 makes **11 / 21**. A soft 21 with three or more cards keeps **Hit** and **Stand** available. Standing uses 21 and beats a dealer 20. Only two-card blackjack, a hard 21, a bust, or a double ends that hand automatically.
- **5-card draw:** 2–5 players, ante, opening betting, replace 0–5 selected cards, final betting, showdown.
- **5-card stud:** one down and one up to start; three more cards face up, one at a time, with betting. Final layout: one down, four up.
- **7-card stud:** two down and one up to start; three more cards face up, then the seventh face down, with betting. Final layout: two down, four up, one down. Best five of seven wins.
- **Texas hold’em:** two private cards, flop, turn, river, and the best five-card combination from private/community cards. Correct heads-up blind/action order.

For draw and stud, the **host chooses the fixed starting ante** in setup or **Settings → Poker ante**. Everyone posts it before any cards are dealt, including every redeal. Click the empty felt to accept and post your exact ante, without selecting chips first; the **Ante** button also works. The chip panel stays available, but any selected amount leaves the fixed ante unchanged. All funded players can ante immediately, with no ante turn order. Bankrupt human seats are skipped for the next deal and betting rotation; computers receive a house loan between hands so they can keep playing. Ordinary raises happen during betting after the deal. Cards deal automatically once all funded seats have posted; there is no extra Deal click. A short stack may ante its remaining chips all in. Cash-out refunds pending, undealt antes.

Your seat stays at the **bottom** and the **house dealer stays at the top**, including with four or five players. The poker marker says **BUTTON** and rotates between player seats without moving them; the house dealer does not play a poker hand. The host receives the **first card of the opening poker hand**. The initial dealer button sits immediately before that seat. Cards and action proceed clockwise; the button moves one seat clockwise each hand. Draw betting begins left of the button. Hold’em keeps the normal blind/action order, including heads-up; stud uses its bring-in and exposed-card order.

All four poker games use **one standard 52-card deck, reshuffled before each hand**. Poker uses no-limit betting, side pots, split pots, and a rotating button. **Call**, **Fold**, **Raise**, and **All in** stay visible, enabled when legal; **Check** is available when nothing is owed. Select the actual chips you want to put in, including any amount needed to call. With no chips selected, click the empty felt to call what you owe, or check if nothing is owed, in every poker variant. With chips selected, click the empty table felt: an exact call continues play, a legal higher amount raises, and selecting your remaining stack goes all in. The label previews the action and resulting street total. Incomplete calls and undersized raises stay selected for correction. Call, Raise, All in, Check, and Fold buttons also act immediately. Streets, showdown, and payouts advance automatically once betting is complete; folding lets the remaining players continue. With no chips selected, the Raise button uses the minimum legal raise. A full raise is at least the previous full raise size, initially the table minimum. Short all-ins do not reopen raising unless their cumulative increase reaches a full raise. There is no raise-count cap. Folding forfeits the chips already committed, removes that player from further betting and card replacement in the current hand, and leaves them watching until the next hand. A raise returns action to every player who still needs to respond. Players call, raise again, fold, or go all in; a betting round ends only once everyone still in has matched or is all in. All in commits the remaining stack and ends that player’s betting, but cards continue normally: five-card draw permits normal replacements, and stud still deals every remaining card. Other funded players can contest side pots. If everyone remaining is all in, no further betting is requested.

**Jacks or better to open** is selectable for draw and both stud variants. A pair of jacks or any better poker hand is needed to open voluntary betting; after an opening, anyone can call or raise. Computers always open a qualifying hand when a bet is legal, including a short all-in when their stack is below the minimum. Stud omits the forced bring-in with this option. If nobody opens, draw ends before replacement, or stud continues through the final card; the pot carries forward. Everyone posts another host-set ante before the next deal, repeating until someone opens. The carried amount joins the next main pot; current wagers still have normal side-pot eligibility. Cashing out before the pot is won returns the unopened antes to their contributors. The rollover and stud adaptation are explicit house rules. The normal no-option stud bring-in is half the minimum bet. Stud betting starts with the strongest upcards after the first street. All streets use no-limit betting.

Highest to lowest: **royal flush → straight flush → four of a kind → full house → flush → straight → three of a kind → two pair → one pair → high card**. A royal flush is A–K–Q–J–10 of one suit. Kickers break otherwise equal categories; identical best five-card values split the pot, with any odd chip assigned clockwise left of the dealer. Suits do not break ties. See the [poker hand-ranking reference](https://www.pokerstars.com/poker/games/rules/hand-rankings/).

The **How to play** button contains detailed rules and keyboard shortcuts. Cards marked **DOWN** in your stud hand are private even though you can see their faces. Other players see card backs until showdown. Computer decisions use their own cards and community cards, never other players’ hidden cards or deck order.

## Network players

1. On the host, choose **Network player** for each human opponent’s seat, then **Open table**.
2. Share the host address, port (default **45821**), and eight-character room code shown above the table.
3. On each other computer, run the app and choose **Join a table**. Enter those details and a player name.
4. Once every reserved network seat is connected, blackjack deals automatically when all bets are confirmed; draw and stud wait for each ante and then deal automatically. The host starts the first hold’em hand with Deal. The host may also occupy a human seat, run a computer seat, or watch an entirely remote/computer table.

Use a trusted LAN or private VPN. There is no central lobby, account, matchmaking service, or encrypted transport. The host must be reachable on the selected TCP port. Clients receive only their own private cards, public cards, and public game state. The host is authoritative.

The table waits if someone disconnects. That client retries automatically and reclaims its seat while the app remains open. Reconnection identity is held in memory; closing/restarting the client requires a new table. Closing the host ends the session. Starting a new table uses your local saved bankroll. Completed-hand balances persist; unfinished hands are not restored after restarting.

## Readable cards

Cards stay on the table with a larger **center rank** and the correct number of suit symbols on 2–10 (two diamonds on the 2♦, for example). Aces have one symbol; J/Q/K use decorative suit symbols. The extra card panel has been removed. Multiple blackjack hands appear in pairs side by side, making a split visible immediately. Click a hand to inspect it, or use the arrows beside your name to navigate more hands directly at your seat. Play advances automatically through the active hands, and soft totals show both values. Click table cards to select replacements in five-card draw, then click the empty felt to confirm. With no cards selected, the same table click keeps all five. The Draw / Keep all 5 button remains available. Poker privacy is preserved.

Each hand shows its **Bust, Won, Lost, Tied, or Blackjack** result. Finished player, dealer, and community cards stay visible with their results until you click a chip or start the next hand; there is no clearing timer. Choosing chips for the next blackjack bet or poker ante clears them immediately. In hold’em, a chip click clears the finished hand without placing a wager; the next deal posts the normal blinds. Bankrolls and history remain; completed cash-out tables retain their final result. Bust appears immediately. A colored result banner directly above your table cards shows each visible hand’s result and net winnings. The bankroll stays visible at your seat and in the sidebar. Poker results distinguish winning, splitting a pot, losing, folding, and unopened pots carried forward.

## Table sounds

Six embedded recordings cover placing chips, going all in, payouts, dealing a card, shuffling, and tapping the deck on the table. Adding/removing a selected chip responds immediately. Confirmed wagers, calls, raises, splits, and doubles use the betting sound. Poker payments that use the remaining stack use the all-in sound. Settling a hand first plays the chip-collection recording, followed by the payout recording when chips are awarded. Blackjack wins and ties play both; losing-only hands still play collection. The collection cue reuses the bundled all-in chip recording.

A new or replenished blackjack shoe plays **shuffle → deck tap → dealt cards**; ordinary rounds from the same shoe begin with dealing. Poker reshuffles its single deck each new hand. Cards appear **one at a time**, synchronized with their dealing sound, at roughly 380 ms intervals. Payouts wait for the final card. Hits, doubles, splits, draw replacements, stud streets, and hold’em community cards produce dealing sounds; burn cards do not. Player actions, including remote actions, wait until the deal finishes. Computer turns and auto-deal also wait. Dealing remains animated when sounds are muted.

**Settings → Table sounds** enables or mutes the effects and setup music, remembering the choice. Muting stops audio immediately while card animation continues. Network players receive ordered per-card events with other players’ downcards redacted. Refreshes, initial joins, new tables, and reconnects do not replay history. Every participant must support network protocol 10; this release is 1.0.19.

Local builds and installed packages use the same seven selected recordings from `assets/`: **Placing poker chips**, **AllInPushChips**, **poker_chips1**, **Taking playing card**, **Shuffling Deck of Cards**, **Pounding Cards On Table**, and **8bit Music for Game**. Their WAV files match the original local pack byte for byte; `assets/provenance.json` records the hashes. Plain CMake builds and the existing `spkgbuild` use these defaults without extra flags, downloads, or an ignored local sound folder. The intro loops quietly while setup is open and stops on Open table, Join table, or Cancel. All audio is embedded in the executable. Credits and the separate audio license are in [third-party notices](THIRD_PARTY_NOTICES.txt) and [selected recording provenance](licenses/Pixabay-Sounds.md).

Playback uses Qt Multimedia's normal application audio, so muting desktop notification sounds does not mute the game. The app's toggle and normal application/output volume controls still apply.

## Appearance

**Settings → Card and table themes** provides 20 independent table finishes, 20 card-face themes, and 20 patterned card backs. Each face now has distinct borders, motifs, and rank typography, while keeping large center values and the correct pip counts. Card selectors show actual front/back thumbnails and a larger pair of card previews; the table-finish selector retains its original swatches. Preview choices during a hand, Save to keep them or Cancel to revert. Preferences persist through Qt settings.

## Chip values

Open **Settings → Chip values** to choose the denomination of each chip color. Defaults are **red 5**, **blue 10**, **white 1**, **green 25**, and **yellow 100** units. Every color accepts a whole-number value from 1 through 1,000,000. Use **Save** to apply, **Cancel** to discard edits, or **Restore Defaults** to fill in the original values before saving.

The chip-selection row sits at the bottom, centered beneath the table, with the action buttons centered immediately above it. The row uses compact controls and keeps its height when hidden. Results appear above the cards on the felt, with no separate status panel taking table space; the table keeps the same size throughout betting, play, and results. Poker pots appear as chip stacks in the center, with the exact total. Draw and stud pot chips sit dead center on the felt. Hold’em places the pot below its five community cards, including the river. “Voncloft Casino” lettering is centered on the felt above the pot or community cards. The sidebar displays all five colors and their values. Each seat keeps its bet chips above its cards. Bankroll piles sit outside the player panels, between the cards and the nearest table edge: to the left of left-side seats, to the right of right-side seats, and below your bottom seat. All cards and chip piles stay upright. Piles have an owner label and exact balance. The separate chip-selection panel controls betting; table piles are visual only.

Each player owns a chip inventory, issued once as a mixed starting stack. Betting removes the selected colors; change is made only when needed. Blackjack returns the original stake chips on a win or tie and adds the profit as new chips. Poker awards transfer chips from the actual pot. Existing stacks keep their colors rather than being converted into larger denominations on every update. Piles contain up to twenty actual chips each, with partial piles showing the remainder; additional piles represent every chip. The whole group scales to fit, and exact counts appear under each pile. Fractional payouts and amounts smaller than the configured denominations appear as change. Changing denomination settings explicitly exchanges inventories at the same total value and preserves an active hand. Between blackjack, draw and stud hands it refunds pending bets so everyone can select the new chips. Saved profiles retain the balance; a new table issues a fresh mixed stack.

Use **Exchange** in the chip row or **Settings → Exchange chips** to choose how your available balance is divided among the five colors. Set the number of each chip you want; the dialog shows the total and any remainder as change, including half units. **Clear counts** starts from zero, and **Fill remaining** fills the unassigned amount using the configured denominations. **Exchange** updates your actual stacks immediately without changing your balance, existing bets, cards or turn. Cancel keeps the previous stacks. Network exchanges are verified by the host; stale requests and totals above your balance are rejected. For example, with red 1, blue 5, green 10 and yellow 100, an 8,000 balance can be 100 red + 100 blue + 40 green + 70 yellow. Five green + nineteen blue + five red totals 150.

Values persist between launches and new tables. On a network table, the host chooses the values and shares them with every client, including after reconnection; clients can view them in Settings. All participants must support network protocol 10; this release is 1.0.19.

## Local profile and cash-out

When a computer runs out of whole chips, it automatically borrows the configured **Computer starting chips** amount from the house between hands. Its mixed chip stack refills, and **Owes house** beside its name shows the accumulated loans, also recorded in hand history and shared with network players. Debt stays separate from the playable balance and is not automatically deducted from winnings; it lasts for the table session and appears in the cash-out summary. Human balances are never replenished this way. All-in hands settle first, and cash-out takes priority over new loans. Computer playing strategy is unchanged.

**Settings → Player profile** stores your name, starting/saved bankroll, completed hands, cash-out count, and last cash-out amount locally through Qt settings. Every completed hand saves your new bankroll, including half units. New local tables and network joins use that balance. Computer stacks use the separate starting-chip setting. Saving edits updates your current seat immediately: name, displayed stack, betting limits, and saved profile. Name edits work during a hand, including during dealing; bankroll edits work before a deal, between hands, or after cashing out. The editor starts with your live bankroll, including chips in pending bets. A bankroll change cancels only your pending blackjack wager so you can select a new one; it does not restart the table, change other players, or erase hand history. Network edits are applied by the host and saved locally only after acknowledgment, and the updated name survives reconnects. Bankroll changes are unavailable while a hand or payout is in progress so active bets and side pots stay intact.

Use **Cash out** in either game to end the session. If a hand is underway, it finishes and pays out first; before a deal, pending wagers and unopened carried antes are refunded. Any connected player may request a table cash-out. Final balances appear in hand history and each connected human saves their own amount locally. The **Cash-out balance** setting automatically ends the session after a settled hand when any player reaches the target; zero disables the target. All amounts are play chips.

## Build and test

Clone the source over SSH:

```sh
git clone git@github.com:voncloft/Voncloft-Casino.git
cd Voncloft-Casino
```

Requires a C++20 compiler, CMake 3.20+, and Qt 6.4+ Core, Widgets, Network, Multimedia (plus Qt Test for testing).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/voncloft-casino
```

The source is portable Qt/C++; the supplied executable was built and tested on this Linux machine. Builds on other operating systems have not been verified. Network validation used real TCP sockets on loopback; a second physical computer has not been tested.

Tests cover all 2,598,960 five-card combinations against known rank-category totals, blackjack payouts/splits/doubles/soft aces, draw selection, exact stud visibility, jacks opening, no-limit minimum raises, short and cumulative all-ins, heads-up hold’em, a deterministic main/side-pot payout, chip conservation through simulated games, network private-card isolation, room-code rejection, reconnect behavior, per-seat wager confirmation, left/right chip clicks, half-unit payouts, cash-out, saved profiles, pictured card-theme selectors, and startup/UI rendering.

Version 1.0.4 also checks numbered-card symbol counts, table hand navigation, immediate busts and final results, network outcome/sound notifications, and sound mute/reconnect behavior. Version 1.0.7 also verifies all-in detection, card/deck sequencing, silent rejected actions, bounded event history, and old-host compatibility. Version 1.0.8 additionally checks persistent shoes, 1–8-deck simulations, deck settings and refunds, one-card timing while muted, early network-action rejection, table-click bets, and setup-music lifecycle. The optional `CASINO_AUDIO_SMOKE=1` test plays the intro and all six effects; `CASINO_AUDIO_DEVICE` can select a Qt audio device ID for isolated output capture.

Rule references: [Bicycle blackjack](https://bicyclecards.com/how-to-play/blackjack/), [PokerStars seven-card stud](https://www.pokerstars.com/poker/games/stud/), [Texas hold’em](https://www.pokerstars.com/poker/games/texas-holdem/), and [five-card draw](https://www.pokerstars.com/poker/games/draw/). This app’s house variations are described above.

## License

voncloft-casino's original application source, tests, build scripts, documentation and artwork are licensed under the **GNU General Public License, version 3 or (at your option) any later version** (`GPL-3.0-or-later`). Copyright (C) 2026 voncloft-casino contributors. See [LICENSE](LICENSE) for the complete terms. This program is distributed without any warranty, including the implied warranties of merchantability or fitness for a particular purpose.

The selected game recordings retain the **Pixabay Content License**, separately from the application's GPL license. They are included as functional audio assets of voncloft-casino. Keep their credits and license notice with the game; do not distribute the audio separately as a sound collection. See [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) and [the audio notice](licenses/Pixabay-Sounds.md).

The older Kenney effects and Freesound intro sources remain available under **CC0-1.0** for an optional alternate pack. Their conversion tools write to ignored `build/cc0-sounds/` and never overwrite the selected defaults. Normal builds do not run those tools.

For GitHub, include the complete game source, `assets/*.wav`, `assets/provenance.json`, and the notices. `build/` and the historical `assets/local-sounds/` copy remain ignored. Publish corresponding application source with GPL binary releases. Qt and media libraries are external dependencies with their own licenses. Do not bundle this machine's nonredistributable FFmpeg build in releases.

## Changelog

### 1.0.19

Renamed the application to voncloft-casino. In-game branding uses Voncloft Casino with a space on the blackjack/poker felt, window title, header, setup and help screens. The primary executable and icon are `voncloft-casino`; compatibility names keep existing package recipes working. Saved profile, bankroll, themes and preferences migrate once from the previous application name without overwriting an existing new profile. The seven selected recordings and network protocol 10 are unchanged.

### 1.0.18

Made the seven recordings selected for the original local build the default game assets. Local builds, clean GitHub-source builds and installed packages now embed identical WAV bytes without custom CMake flags or external files. Added their provenance hashes and corrected the audio notices to retain the Pixabay Content License. Alternate CC0 conversion tools now write into `build/cc0-sounds/` instead of replacing the selected game sounds. Network protocol remains 10.

### 1.0.17

Included the original setup tune from annoyedCactus's CC0 Freesound release in the public source. Default builds, including `spkgbuild` with an empty `CASINO_SOUND_DIR`, now embed all six effects and the intro. Missing audio files fail configuration; the setup lifecycle test now fails instead of skipping if the intro is missing. Added source attribution, checksums and a reproducible conversion script. Optional custom packs still override all seven sounds. Network protocol remains 10.

### 1.0.16

Empty-felt clicks confirm selected replacements in five-card draw, or keep all five when none are selected. In every poker variant, clicking the felt with no chips selected calls the amount owed or checks when nothing is owed. Blackjack uses left-click on the felt to Hit and right-click to Stand. Existing selected-chip calls, raises, all-ins and ante confirmation remain available, and right-clicking betting chips still removes chips. Card clicks retain their selection/navigation behavior; turn, pause, dealing and network guards apply.

### 1.0.15

Busted computer players receive the configured starting stack as a house loan between hands, keeping the existing table running. Accumulated debt appears at their seat, in history, in network snapshots and in cash-out totals. Human winnings, final hand results, rotation and computer playing strategy are preserved. Loans wait for settlement and are suppressed on cash-out.

### 1.0.14

Added Exchange chips to choose the actual color counts for the available balance, with live totals, retained change, clear/fill controls, cancel, and immediate updates. Bets and funds are preserved, including half units. Protocol 10 validates exchanges against the host's balance, denominations and inventory and rejects stale requests. Restored upright cards for all side seats and placed their decorative bankroll stacks between the cards and the nearest table edge. Tested full and partial exchanges, invalid totals, cancellation, active hands and network synchronization.

### 1.0.13

Bankroll piles now sit between the cards and the table edge. Side seats rotate inward together with their cards and chip stacks; the local player stays upright at the bottom. Added persistent per-table chip inventories: selected colors leave the stack, refunds return them, blackjack winnings add new chips, and poker winners receive the actual pot chips. All chip counts are represented in piles of up to twenty, with no decorative count cap. The chip controls and fixed table size are preserved. Protocol 9 shares inventories and selected betting colors across network players. Tests verify inventory conservation, exact colors, refunds, payouts, network betting and normal draw checks returning to the player after another player bets.

### 1.0.12

Added decorative chip piles on the felt outside every player panel that track bets, payouts and live balances, while keeping the chip controls and table geometry. Added a theme-colored, angled transparent blackjack shoe with a curved front opening beside the dealer with a live remaining-card count and fill level synchronized to dealing and shuffles. Clicking empty felt before a draw/stud deal accepts the fixed ante directly; chip selection is optional. Regression checks cover a bankrupt third player, remaining-seat rotation, short antes and network confirmation. Network protocol remains 8.

### 1.0.11

Finished hands remain on the table until the player clicks a chip or starts the next hand; removed timed clearing in blackjack and every poker variant. Hold’em chip clicks clear the result without altering balances or blinds. Added a fixed house-dealer area above poker tables and labeled the rotating player marker BUTTON; the local player stays at the bottom. Expanded first-card checks to two through five seats, including blackjack. Computers with jacks-or-better openers now bet instead of randomly checking, including short-stack all-ins. Network protocol remains 8.

### 1.0.10

Draw and stud now require a confirmed, host-configured ante before each deal and start automatically when everyone is ready. Jacks-or-better hands with no opener carry the pot forward and require another ante; the next winning hand receives it, with exact tie and side-pot handling. The host receives the first opening card and the dealer button then rotates clockwise. Added pot chips centered in draw/stud and below hold’em’s community cards, chip-collection sounds before payouts, centered voncloft-casino felt lettering, persistent poker action controls, explicit hand rankings, and tests for folding, repeated raises, all-in draw/stud card completion, network antes, refunds and reconnects. Network protocol is 8.

### 1.0.9

Profile saves now update the current seat and bankroll immediately between hands, including pending-wager refunds; names can change during play. Network profile updates are host-authorized and acknowledged, with reconnect-safe names. Finished cards clear after a brief result display or as soon as next-bet chips are selected. Each starting blackjack hand now has its own chip wager. Split hands are displayed side by side; split aces remain playable, and split two-card blackjacks pay 3:2 under the requested house rules. Multi-card soft 21 remains playable until the player chooses to stand, using 21 at settlement. Fixed-deck tests cover the reported ace sequences, split/double/resplit progression, separate wagers, payouts, animation, and real TCP clients. Network protocol is 7.

### 1.0.8

Replaced the humming shuffle recording with OxidVideos’ Shuffling Deck of Cards. Blackjack now uses a persistent, configurable 1–8-deck shoe and shuffles only when the shoe is created or replenished; poker retains one deck per hand. Cards appear individually with matching sounds, including splits, draws and dealer hits. Actions wait until dealing finishes, with private cards redacted in network animation events. Clicking the empty felt confirms selected blackjack wagers and poker calls, raises, or all-ins. Blackjack deals automatically after all bets are ready. Centered the action buttons above a compact bottom chip row and moved result banners directly above the hands, keeping the larger table stable throughout play. Gave all 20 card faces distinct borders, motifs, and rank typography without changing table finishes. Added looping setup intro music that stops when the setup dialog closes. Network protocol is 6.

### 1.0.7

Added separate all-in, card-dealing, shuffle, and deck-tap recordings alongside bet/payout sounds. Ordered network events preserve shuffle → tap → cards → payout, including multiple events in one update; mute and reconnect cancel stale playback. Computer turns wait for active audio. Added an optional local Pixabay pack with a reproducible import script and source/license documentation, keeping the public GitHub build self-contained with CC0 alternatives. Network protocol remains 5.

### 1.0.6

Replaced synthesized metallic chip sounds with Kenney Casino Audio chip-collision and handling recordings. WAVs remain embedded and play through normal application audio. Added GPL-3.0-or-later licensing for the project, retained CC0 notices for the audio, and included original audio sources plus a conversion script. Network protocol remains 5.

### 1.0.5

Fixed silent chip sounds when system notification effects are muted: cached PCM now plays through normal Qt application audio. Added immediate chip sounds when adding or removing a chip from the bet. Verified the actual chip and Place bet buttons plus payouts with the production application identity and no audio mute/routing-property overrides. Network protocol remains 5.

### 1.0.4

Removed the enlarged card panel, added large center ranks with numbered suit symbols, and moved multiple-hand navigation into the player’s table seat. Added immediate Bust labels, explicit blackjack and poker results, round profit/loss in the status area, and synthesized chip sounds for bets and payouts with a persistent mute option. Network protocol 5 includes sound-event counters and explicit round outcomes.

### 1.0.3

Added jumbo card ranks and a large local card view; multiple blackjack hands (1–4 starting hands with up to four splits per original hand), per-hand wagering from a shared bankroll, automatic hand progression, and visible soft-ace totals. Split and Double down remain visible with eligibility explanations. Network protocol 4 carries the selected hand count. Blackjack uses an eight-deck shoe to accommodate multiple hands at all five seats. Corrected the in-app poker rulebook to describe the existing no-limit betting.

### 1.0.2

Added per-player blackjack chip selection before every deal, left-click add/right-click remove, bet and bankroll stacks at each seat, exact half-unit blackjack payouts, no-limit poker Raise/Call/All in, manual and target cash-out, and persistent local player profiles. Blackjack setup hides poker options. Card-theme selectors now show real front and back pictures while table selection stays the same. Network protocol 3 carries confirmed wagers, raise amounts, profile starting balances, and cash-out requests.

### 1.0.1

Added Settings → Chip values for red, blue, white, green, and yellow chips, saved denominations, a color/value legend, colored pot and wager stacks, and host-to-client synchronization. Added checks for denomination validation, exact displayed amounts, Save/Cancel/defaults, persistence, unchanged hand state, and network synchronization/reconnect. Network protocol advances to 2 so all players use the shared values.

### 1.0.0

Initial release: blackjack, four poker variants, up to five configurable seats, computer-only tables, network players with private hands and reconnect, jacks-or-better opening, 20 choices in each appearance category, saved preferences, and an in-app rulebook.
# Voncloft-Casino
