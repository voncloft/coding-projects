#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <array>
#include <random>
#include <vector>

namespace Golf {
constexpr int Hidden = -99;
constexpr int CardsPerDeck = 108;
constexpr int MaxDecks = 8;
int value(int card);
enum class Phase { Opening, Draw, Place, Reveal, HoleOver, MatchOver };
enum class Matching { Columns, Pairs };
struct Rules {
    int columns = 4, rows = 2, decks = 1;
    Matching matching = Matching::Columns;
    int cpuTarget = 0;
    bool cancelRows = false;
    int cardCount() const { return columns * rows; }
    int deckSize() const { return CardsPerDeck * decks; }
    bool valid() const;
};
struct Slot { int card = -1; bool up = false; };
struct Hand : std::vector<Slot> {
    using std::vector<Slot>::vector;
    Hand() : std::vector<Slot>(8) {}
};
struct Score {
    int total = 0, bonus = 0;
    std::array<int,4> columns{};
    std::array<bool,16> matched{};
};
struct State {
    Rules rules;
    int opponents = 1, holes = 9, hole = 1, current = 0;
    int held = -1, endingBy = -1;
    bool fromDiscard = false;
    Phase phase = Phase::Opening;
    std::vector<Hand> hands;
    std::vector<int> deck, discards;
    std::vector<std::vector<int>> scores;
    std::vector<int> turns;
    QString message;
    QStringList names;
};
class Game {
public:
    explicit Game(unsigned seed = std::random_device{}());
    void newMatch(int opponents = 1, int holes = 9, const Rules &rules = {});
    const State &state() const { return s; }
    int players() const { return s.opponents + 1; }
    int hidden(int player) const;
    int visibleValue(int player, int slot) const;
    int total(int player) const;
    bool active() const;
    bool open(int slot);
    bool draw(bool discard);
    bool replace(int slot);
    bool discardHeld();
    bool finishHole();
    bool nextHole();
    bool setRounds(int rounds);
    bool setCpuTarget(int target);
    void computerStep();
    QJsonObject save() const;
    bool restore(const QJsonObject &object);
    QJsonObject publicView(int viewer) const;
    bool restorePublicView(const QJsonObject &object, int viewer);
    void setNames(const QStringList &names);
    QString name(int player) const;
    static Score score(const Hand &hand, bool visibleOnly = false, const Rules &rules = {});
    static QString playerName(int player);
private:
    friend class GameTest;
    State s;
    std::mt19937 rng;
    void deal();
    void endTurn();
    void scoreHole();
    double estimate(const Hand &hand) const;
    double estimate(const Hand &hand, const std::array<int,14> &counts) const;
    std::array<int,14> unseenValues() const;
    double handQuality(const Hand &hand) const;
    bool shouldFinish(int card, double replacementGain) const;
    bool shouldFlipFinal() const;
    std::pair<int,double> bestReplacement(int card) const;
};
}
