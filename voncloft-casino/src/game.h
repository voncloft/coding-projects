// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 voncloft-casino contributors
#pragma once
#include <QtCore>
#include <array>
#include <random>
#include <optional>
#include <vector>

namespace casino {
enum class Kind { Blackjack, Draw, Stud5, Stud7, Holdem };
enum class Control { Human, Computer, Remote };
QString gameName(Kind kind);
QString formatUnits(double amount);
struct Card {
    int rank = 2, suit = 0; // clubs, diamonds, hearts, spades
    bool operator==(const Card &) const = default;
    QJsonObject json() const;
};
using Cards = std::vector<Card>;
using Rank = std::array<int, 6>;
using ChipValues = std::array<int, 5>; // red, blue, white, green, yellow
using HandWagers = std::vector<std::array<int,5>>;
QJsonArray handWagersJson(const HandWagers &hands);
inline constexpr ChipValues DefaultChipValues{5, 10, 1, 25, 100};
inline constexpr std::array<const char *, 5> ChipNames{"Red", "Blue", "White", "Green", "Yellow"};
bool validChipValues(const ChipValues &values);
QJsonArray chipValuesJson(const ChipValues &values);
ChipValues readChipValues(const QJsonArray &values);
struct ChipBreakdown { std::array<int, 5> counts{}; int change=0; };
ChipBreakdown breakDownChips(int amount, const ChipValues &values);
qint64 chipInventoryValue(const ChipBreakdown &chips,const ChipValues &values);
void addChipInventory(ChipBreakdown &to,const ChipBreakdown &chips);
ChipBreakdown takeChipInventory(ChipBreakdown &from,int amount,const ChipValues &values,const std::array<int,5> &chosen={});
Rank rankHand(const Cards &cards);
QString rankName(const Rank &rank);
bool jacksOrBetter(const Cards &cards);
int blackjackValue(const Cards &cards, bool *soft = nullptr);
QString blackjackTotalText(const Cards &cards);
struct PlayerConfig { QString name; Control control = Control::Computer; double startingBalance = -1; std::optional<ChipBreakdown> startingInventory=std::nullopt; };
struct Config {
    Kind kind = Kind::Blackjack;
    std::vector<PlayerConfig> players{{"You", Control::Human}, {"Ruby", Control::Computer}};
    int bankroll = 1000, unit = 20;
    int ante = 0; // zero uses the legacy quarter-minimum default
    int firstCardSeat = 0; // host receives the first card of the opening poker deal
    int blackjackDecks = 8;
    bool jacks = false;
    ChipValues chipValues = DefaultChipValues;
    int cashoutTarget = 0;
};
struct Hand {
    Cards cards;
    int bet = 0;
    bool done = false, split = false;
    QString result;
    int origin = 0; // original betting hand; each may split into up to four hands
    double returned = 0;
    ChipBreakdown chipBet{};
};
struct Seat {
    PlayerConfig config;
    int chips = 0, committed = 0, streetBet = 0;
    int halfUnit = 0, wager = 0, wagerHands = 1;
    qint64 houseDebt = 0;
    bool wagerReady = false;
    std::array<int,5> wagerChips{};
    HandWagers wagerHandChips;
    ChipBreakdown bankChips,committedChips;
    bool folded = false, inHand = false;
    Cards cards;
    std::vector<bool> up;
    std::vector<Hand> hands;
    QString status, outcome;
    double roundNet = 0;
};
class Game {
public:
    explicit Game(quint32 seed = std::random_device{}());
    void configure(const Config &config);
    bool deal();
    bool act(int seat, const QString &action, const std::vector<int> &discards = {}, int raiseTo = 0,const std::array<int,5> &chosen={});
    void computerStep();
    bool fundBustedComputers(); // between-hand house loans; never changes playing strategy
    QJsonObject snapshot(int viewer = -1) const;
    QStringList legalActions() const;
    const Config &config() const { return cfg; }
    const std::vector<Seat> &players() const { return seats; }
    int current() const { return turn; }
    int handNumber() const { return handNo; }
    int potValue() const;
    bool active() const { return phase != "ready" && phase != "finished"; }
    QString stage() const { return phase; }
    double totalChips() const;
    void rigNextDeck(const Cards &top); // deterministic rule tests; not exposed through the UI or network
    void setPlayerName(int seat, const QString &name);
    bool setChipValues(const ChipValues &values);
    bool setBlackjackDecks(int count);
    int maxStartingHands(int seat) const;
    bool placeWager(int seat, const std::array<int,5> &counts, int handCount=1);
    bool placeHandWagers(int seat, const HandWagers &hands);
    bool clearWager(int seat);
    bool exchangeChips(int seat, const std::array<int,5> &counts);
    bool betsReady() const;
    bool usesAnte() const { return cfg.kind==Kind::Draw||cfg.kind==Kind::Stud5||cfg.kind==Kind::Stud7; }
    int anteValue() const { return cfg.ante?cfg.ante:cfg.unit/4; }
    int anteDue(int seat) const;
    bool postAnte(int seat);
    bool setAnte(int amount);
    bool setStartingBalance(int seat, double amount);
    bool updateProfile(int seat, const QString &name, std::optional<double> balance = std::nullopt);
    void requestCashout();
    bool sessionEnded() const { return cashedOut; }
private:
    Config cfg;
    std::vector<Seat> seats;
    Cards deck, nextDeck, board, dealer;
    std::mt19937 rng;
    QString phase = "ready", message;
    QStringList history;
    QString soundSession;
    int betSound = 0, payoutSound = 0, soundSequence = 0;
    QJsonArray soundEvents;
    void sound(const QString &cue, QJsonObject details = {});
    int roundHandLimit = 80;
    void recordCard(const Card &card, const QString &target, int index, int seat=-1, int hand=-1, bool privateCard=false);
    void dealCard(Cards &cards, const QString &target, int seat=-1, int hand=-1, bool privateCard=false);
    int turn = -1, handIndex = 0, handNo = 0, button = -1, street = 0;
    int currentBet = 0, raises = 0, minimumRaise = 0;
    bool opened = false, showdown = false;
    bool cashedOut = false, cashoutRequested = false;
    std::vector<int> carriedAntes;
    std::vector<ChipBreakdown> carriedChips;
    std::vector<int> pending;
    std::vector<bool> raiseRights;
    std::vector<int> actedAtBet;
    void log(const QString &text);
    Card take();
    int nextSeat(int after, bool funded = false) const;
    std::vector<int> clockwise(int start, bool canAct) const;
    int liveCount() const;
    ChipBreakdown pay(int seat, int amount, bool streetPayment = true,const std::array<int,5> &chosen={});
    int betSize() const;
    int raiseTarget() const;
    void beginBetting(int first, bool reset = true);
    void advancePoker();
    void endBetting();
    void finishPoker(bool noOpener = false);
    void advanceBlackjack();
    void finishBlackjack();
    Cards visibleCards(int seat) const;
    int studLeader() const;
    void checkCashout();
};
}
