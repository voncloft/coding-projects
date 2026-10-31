#include "game.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <sstream>

namespace Golf {
int value(int card) { return card < 0 ? Hidden : card >= 104 ? -5 : card / 8; }
Game::Game(unsigned seed) : rng(seed) { newMatch(); }
QString Game::playerName(int p) { return p == 0 ? QStringLiteral("You") : QStringLiteral("Computer %1").arg(p); }
void Game::newMatch(int opponents, int holes) {
    s = State{};
    s.opponents = std::clamp(opponents, 1, 4); // Retain support for older five-seat saves.
    s.holes = std::clamp(holes, 1, 99);
    for(int p=0;p<players();++p) s.names.append(playerName(p));
    deal();
}
void Game::deal() {
    s.deck.resize(108);
    std::iota(s.deck.begin(), s.deck.end(), 0);
    std::shuffle(s.deck.begin(), s.deck.end(), rng);
    s.hands.assign(players(), Hand{});
    for (int i = 0; i < 8; ++i) for (auto &hand : s.hands) {
        hand[i] = {s.deck.back(), false}; s.deck.pop_back();
    }
    s.discards = {s.deck.back()}; s.deck.pop_back();
    s.turns.assign(players(),0);
    s.held = -1; s.endingBy = -1; s.fromDiscard = false;
    s.current = 0; s.phase = Phase::Opening;
    s.message = "All cards are dealt face down. Turn seven of your cards face up; leave one face down.";
}
int Game::hidden(int p) const {
    return int(std::count_if(s.hands.at(p).begin(), s.hands.at(p).end(), [](auto x){ return !x.up; }));
}
int Game::visibleValue(int p, int i) const { auto x = s.hands.at(p).at(i); return x.up ? value(x.card) : Hidden; }
int Game::total(int p) const { int n = 0; for (const auto &row : s.scores) n += row.at(p); return n; }
bool Game::active() const { return s.phase == Phase::Opening || s.phase == Phase::Draw || s.phase == Phase::Place; }
bool Game::open(int i) {
    if (s.phase != Phase::Opening || i < 0 || i >= 8 || s.hands[s.current][i].up || hidden(s.current)<=1) return false;
    s.hands[s.current][i].up = true;
    if (hidden(s.current) == 1) {
        ++s.current;
        if(s.current==players()) { s.phase=Phase::Draw; s.current=(s.hole-1)%players(); s.message="Seven cards face up, one face down. Draw to play, or flip your final card to end the round."; }
        else s.message=QString("%1 is turning seven cards face up.").arg(name(s.current));
    } else s.message=QString("%1: turn %2 more cards face up.").arg(name(s.current)).arg(hidden(s.current)-1);
    return true;
}
bool Game::draw(bool discard) {
    if (s.phase != Phase::Draw) return false;
    if(!discard && s.deck.empty() && s.discards.size()>1) {
        int top=s.discards.back();s.deck.assign(s.discards.begin(),s.discards.end()-1);s.discards={top};
        std::shuffle(s.deck.begin(),s.deck.end(),rng);
    }
    auto &pile = discard ? s.discards : s.deck;
    if (pile.empty()) return false;
    s.held = pile.back(); pile.pop_back(); s.fromDiscard = discard; s.phase = Phase::Place;
    s.message = QString("%1 drew %2%3.").arg(name(s.current)).arg(discard ? QString::number(value(s.held)) : "a card").arg(discard ? " from the discard pile" : " from the deck");
    return true;
}
bool Game::replace(int i) {
    if (s.phase != Phase::Place || i < 0 || i >= 8) return false;
    auto &slot = s.hands[s.current][i];
    const bool finishing = !slot.up;
    int old = value(slot.card), incoming = value(s.held);
    s.discards.push_back(slot.card); slot = {s.held, true}; s.held = -1;
    s.message = QString("%1 placed %2 and discarded %3.").arg(name(s.current)).arg(incoming).arg(old);
    if(finishing) { s.endingBy=s.current; scoreHole(); }
    else endTurn();
    return true;
}
bool Game::discardHeld() {
    if (s.phase != Phase::Place || s.fromDiscard) return false;
    int v = value(s.held); s.discards.push_back(s.held); s.held = -1;
    s.message = QString("%1 discarded %2.").arg(name(s.current)).arg(v);
    endTurn();
    return true;
}
bool Game::finishHole() {
    if(s.phase==Phase::Draw) {
        s.endingBy=s.current;scoreHole();return true;
    }
    // Once a card is drawn, use it to replace the final face-down card.
    if(s.phase!=Phase::Place) return false;
    for(int i=0;i<8;++i) if(!s.hands[s.current][i].up) return replace(i);
    return false;
}
void Game::endTurn() {
    ++s.turns[s.current];
    s.current = (s.current + 1) % players(); s.phase = Phase::Draw; s.fromDiscard = false;
}
Score Game::score(const Hand &hand, bool visibleOnly) {
    Score result;
    std::array<int,14> pairs{};
    for (int c = 0; c < 4; ++c) {
        const auto a = hand[c], b = hand[c+4];
        bool av = !visibleOnly || a.up, bv = !visibleOnly || b.up;
        int va = value(a.card), vb = value(b.card);
        if (av && bv && va == vb) {
            ++pairs[va == -5 ? 13 : va];
            result.columns[c] = va == -5 ? -10 : 0;
        } else result.columns[c] = (av ? va : 0) + (bv ? vb : 0);
        result.total += result.columns[c];
    }
    for (int n : pairs) if (n >= 2) result.bonus -= 5 * n;
    result.total += result.bonus;
    return result;
}
void Game::scoreHole() {
    std::vector<int> row;
    for (auto &hand : s.hands) { for (auto &slot : hand) slot.up = true; row.push_back(score(hand).total); }
    s.scores.push_back(row);
    s.phase = s.hole == s.holes ? Phase::MatchOver : Phase::HoleOver;
    s.message = QString("%1 Everyone turns their last card face up. Round %2 is scored.").arg(s.endingBy==-2?"The deck ran out.":name(s.endingBy)+" ended the round.").arg(s.hole);
}
bool Game::nextHole() {
    if (s.phase != Phase::HoleOver) return false;
    ++s.hole; deal(); return true;
}
bool Game::setRounds(int rounds) {
    if(rounds<s.hole || rounds>99)return false;
    s.holes=rounds;
    if(s.phase==Phase::HoleOver || s.phase==Phase::MatchOver)
        s.phase=s.hole==s.holes?Phase::MatchOver:Phase::HoleOver;
    return true;
}
std::array<int,14> Game::unseenValues() const {
    // Count only information already revealed at the table. Never inspect the stock
    // order or the identity of any face-down card, including our own.
    std::array<int,14> counts; counts.fill(8); counts[13]=4;
    auto seen=[&](int card){int v=value(card);if(v==Hidden)return;int rank=v==-5?13:v;if(counts[rank]>0)--counts[rank];};
    for(const auto &hand:s.hands)for(auto slot:hand)if(slot.up)seen(slot.card);
    for(int card:s.discards)seen(card);
    if(s.held>=0)seen(s.held);
    return counts;
}
double Game::estimate(const Hand &hand) const {
    int down=0,position=-1;
    for(int i=0;i<8;++i)if(!hand[i].up){++down;position=i;}
    if(!down)return score(hand).total;
    if(down==1) {
        const auto counts=unseenValues();double total=0,weight=0;
        for(int rank=0;rank<14;++rank)if(counts[rank]) {
            auto possible=hand;possible[position]={rank==13?104:rank*8,true};
            total+=counts[rank]*score(possible).total;weight+=counts[rank];
        }
        return weight?total/weight:score(hand,true).total;
    }
    // Before opening is complete, only a broad estimate is meaningful.
    double total = 0;
    std::array<int,14> pairs{};
    for (int c = 0; c < 4; ++c) {
        auto a = hand[c], b = hand[c+4];
        if (!a.up && !b.up) { total += 10.8; continue; }
        if (!a.up || !b.up) {
            int known = value(a.up ? a.card : b.card);
            total += known + 5.6 - (known >= 0 ? (known * 2.0 / 14.0) : 0); continue;
        }
        int va = value(a.card), vb = value(b.card);
        if (va == vb) { ++pairs[va == -5 ? 13 : va]; if (va == -5) total -= 10; }
        else total += va + vb;
    }
    for (int n : pairs) if (n >= 2) total -= n * 5;
    return total;
}
double Game::handQuality(const Hand &hand) const {
    const double now=estimate(hand);
    const auto counts=unseenValues();double improvement=0,weight=0;
    // Look one draw ahead: a setup for a second matching pair can be worth keeping
    // even when a different replacement lowers the immediate total a little more.
    for(int rank=0;rank<14;++rank)if(counts[rank]) {
        double best=now;
        for(int i=0;i<8;++i)if(hand[i].up) {
            auto next=hand;next[i]={rank==13?104:rank*8,true};
            best=std::min(best,estimate(next));
        }
        improvement+=counts[rank]*(now-best);weight+=counts[rank];
    }
    return now-(weight?0.65*improvement/weight:0);
}
std::pair<int,double> Game::bestReplacement(int card) const {
    // estimate() only reads face-up cards, never the identity of a hidden slot.
    const auto &hand = s.hands[s.current];
    double best = -1e9, before = handQuality(hand); int position = 0;
    for (int i = 0; i < 8; ++i) {
        if(!hand[i].up) continue;
        auto candidate = hand; candidate[i] = {card, true};
        double gain = before - handQuality(candidate);
        if (gain > best) { best = gain; position = i; }
    }
    return {position,best};
}
bool Game::shouldFinish(int card,double replacementGain) const {
    if(s.turns[s.current]<6)return false;
    auto finished=s.hands[s.current];
    for(auto &slot:finished)if(!slot.up)slot={card,true};
    const int mine=score(finished).total;
    // A poor score never becomes an acceptable finish just because other hands
    // look worse or time has passed. Seek a low single-digit or negative score.
    if(mine>6)return false;
    double other=10000;
    for(int p=0;p<players();++p)if(p!=s.current)other=std::min(other,estimate(s.hands[p]));
    if(mine>=other-3.0)return false;
    // Keep a useful draw in play when it can materially improve a positive hand.
    return mine<=0 || replacementGain<4.0;
}
bool Game::shouldFlipFinal() const {
    if(s.turns[s.current]<6)return false;
    const auto &hand=s.hands[s.current];int down=-1;
    for(int i=0;i<8;++i)if(!hand[i].up)down=i;
    if(down<0)return false;
    // Do not peek. Flip only when every still-possible card gives a reasonable
    // finish, so a bad face-down card cannot turn a voluntary finish into 28.
    const auto counts=unseenValues();int worst=-100;
    for(int rank=0;rank<14;++rank)if(counts[rank]) {
        auto possible=hand;possible[down]={rank==13?104:rank*8,true};
        worst=std::max(worst,score(possible).total);
    }
    if(worst>6 || worst==-100)return false;
    double other=10000;
    for(int p=0;p<players();++p)if(p!=s.current)other=std::min(other,estimate(s.hands[p]));
    return estimate(hand)<other-3.0;
}
void Game::computerStep() {
    if (!active()) return;
    if(s.phase==Phase::Opening) {
        std::array<int,8> positions{0,1,2,3,4,5,6,7};
        std::shuffle(positions.begin(),positions.end(),rng);
        int player=s.current;
        for(int i:positions) if(s.current==player && !s.hands[player][i].up) open(i);
        return;
    }
    if (s.phase == Phase::Draw) {
        if(shouldFlipFinal()){finishHole();return;}
        bool take=false;
        if(!s.discards.empty()) {
            const double gain=bestReplacement(s.discards.back()).second;
            take=gain>0.2 || shouldFinish(s.discards.back(),gain);
        }
        draw(take); return;
    }
    if (s.phase == Phase::Place) {
        auto [i,gain] = bestReplacement(s.held);
        if(shouldFinish(s.held,gain)) { finishHole(); return; }
        if (s.fromDiscard || gain > 0.1) replace(i); else discardHeld();
        return;
    }
}
QJsonObject Game::save() const {
    QJsonObject o{{"format",2},{"opponents",s.opponents},{"holes",s.holes},{"hole",s.hole},
      {"current",s.current},{"held",s.held},{"endingBy",s.endingBy},
      {"fromDiscard",s.fromDiscard},{"phase",int(s.phase)},{"message",s.message}};
    auto numbers = [](const auto &v){ QJsonArray a; for (auto n : v) a.append(n); return a; };
    o["deck"] = numbers(s.deck); o["discards"] = numbers(s.discards);
    o["turns"]=numbers(s.turns);
    o["names"]=QJsonArray::fromStringList(s.names);
    QJsonArray hands; for (auto hand : s.hands) { QJsonArray h; for(auto x : hand) h.append(QJsonObject{{"card",x.card},{"up",x.up}}); hands.append(h); } o["hands"] = hands;
    QJsonArray scores; for(auto row:s.scores) scores.append(numbers(row)); o["scores"] = scores;
    std::ostringstream stream; stream << rng; o["random"] = QString::fromStdString(stream.str());
    return o;
}
bool Game::restore(const QJsonObject &o) {
    State n;
    if (o["format"].toInt() != 2) return false;
    n.opponents=o["opponents"].toInt(); n.holes=o["holes"].toInt(); n.hole=o["hole"].toInt();
    n.current=o["current"].toInt(-1); n.held=o["held"].toInt(-2); n.endingBy=o["endingBy"].toInt(-3);
    n.fromDiscard=o["fromDiscard"].toBool(); int phase=o["phase"].toInt(-1);
    if(n.opponents<1 || n.opponents>4 || (n.holes<1 || n.holes>99) || n.hole<1 || n.hole>n.holes || n.current<0 || n.current>n.opponents || phase<0 || phase>5 || phase==int(Phase::Reveal) || n.held< -1 || n.held>107 || n.endingBy< -2 || n.endingBy>n.opponents) return false;
    n.phase=Phase(phase); n.message=o["message"].toString();
    for(auto v:o["names"].toArray()) n.names.append(v.toString().left(24));
    if(n.names.size()!=n.opponents+1) { n.names.clear(); for(int p=0;p<=n.opponents;++p)n.names.append(playerName(p)); }
    for(auto v:o["turns"].toArray()) { if(!v.isDouble() || v.toInt()<0) return false; n.turns.push_back(v.toInt()); }
    if(int(n.turns.size())!=n.opponents+1) return false;
    std::set<int> ids;
    auto add = [&ids](int id){ return id>=0 && id<108 && ids.insert(id).second; };
    for (auto v:o["deck"].toArray()) { int id=v.toInt(-1); if(!add(id)) return false; n.deck.push_back(id); }
    for (auto v:o["discards"].toArray()) { int id=v.toInt(-1); if(!add(id)) return false; n.discards.push_back(id); }
    for (auto v:o["hands"].toArray()) {
        auto a=v.toArray(); if(a.size()!=8) return false; Hand h;
        for(int i=0;i<8;++i) { auto x=a[i].toObject(); int id=x["card"].toInt(-1); if(!add(id) || !x["up"].isBool()) return false; h[i]={id,x["up"].toBool()}; } n.hands.push_back(h);
    }
    if(n.held>=0 && !add(n.held)) return false;
    if(ids.size()!=108 || int(n.hands.size())!=n.opponents+1 || ((n.phase==Phase::Place)!=(n.held>=0))) return false;
    for(auto v:o["scores"].toArray()) {
        auto a=v.toArray(); if(a.size()!=n.opponents+1) return false; std::vector<int> row;
        for(auto x:a) { if(!x.isDouble() || x.toInt() < -40 || x.toInt()>96) return false; row.push_back(x.toInt()); } n.scores.push_back(row);
    }
    bool over=n.phase==Phase::HoleOver || n.phase==Phase::MatchOver;
    if(int(n.scores.size())!=n.hole-(over?0:1)) return false;
    if((n.phase==Phase::MatchOver)!=(over && n.hole==n.holes)) return false;
    auto hiddenIn = [&n](int p){ return std::count_if(n.hands[p].begin(),n.hands[p].end(),[](auto x){return !x.up;}); };
    if(n.phase==Phase::Opening) {
        for(int p=0;p<=n.opponents;++p) {
            if(p<n.current && hiddenIn(p)!=1) return false;
            if(p==n.current && hiddenIn(p)<2) return false;
            if(p>n.current && hiddenIn(p)!=8) return false;
        }
    } else if(!over) for(int p=0;p<=n.opponents;++p) if(hiddenIn(p)!=1) return false;
    if(n.phase==Phase::Draw && n.deck.empty() && n.discards.size()<=1) return false;
    if(over) for(int p=0;p<=n.opponents;++p) if(hiddenIn(p)!=0 || n.scores.back()[p]!=score(n.hands[p]).total) return false;
    std::istringstream stream(o["random"].toString().toStdString()); std::mt19937 restored; if(!(stream >> restored)) return false;
    s=std::move(n); rng=restored; return true;
}
QString Game::name(int player) const { return s.names.value(player,playerName(player)); }
void Game::setNames(const QStringList &names) { if(names.size()==players())s.names=names; }
QJsonObject Game::publicView(int viewer) const {
    QJsonObject o{{"players",players()},{"rounds",s.holes},{"round",s.hole},
        {"current",s.current},{"phase",int(s.phase)},{"fromDiscard",s.fromDiscard},
        {"stockCount",int(s.deck.size())},{"discardCount",int(s.discards.size())},{"message",s.message},{"names",QJsonArray::fromStringList(s.names)}};
    o["discard"]=s.discards.empty()?QJsonValue():QJsonValue(value(s.discards.back()));
    o["drawn"]=(s.phase==Phase::Place && s.current==viewer)?QJsonValue(value(s.held)):QJsonValue();
    QJsonArray hands;
    for(const auto &hand:s.hands) { QJsonArray a; for(auto slot:hand)a.append(slot.up?QJsonValue(value(slot.card)):QJsonValue()); hands.append(a); }
    o["hands"]=hands;
    QJsonArray scores; for(const auto &row:s.scores) { QJsonArray a; for(int n:row)a.append(n); scores.append(a); } o["scores"]=scores;
    return o;
}
bool Game::restorePublicView(const QJsonObject &o,int viewer) {
    auto integer=[](QJsonValue v,int low,int high){ return v.isDouble() && v.toDouble()==v.toInt(low-1) && v.toInt()>=low && v.toInt()<=high; };
    if(!integer(o["players"],2,5) || !integer(o["rounds"],1,99) || !integer(o["round"],1,o["rounds"].toInt()) || !integer(o["current"],0,o["players"].toInt()-1) || !integer(o["phase"],0,5) || o["phase"].toInt()==3 || !integer(o["stockCount"],0,108) || !integer(o["discardCount"],0,108))return false;
    int players=o["players"].toInt(); if(viewer<0 || viewer>=players)return false;
    State n; n.opponents=players-1; n.holes=o["rounds"].toInt(); n.hole=o["round"].toInt(); n.current=o["current"].toInt(); n.phase=Phase(o["phase"].toInt()); n.fromDiscard=o["fromDiscard"].toBool();
    auto validValue=[&](QJsonValue v){return v.isNull() || integer(v,0,12) || (v.isDouble() && v.toDouble()==-5);};
    auto cardId=[](int v){return v==-5?104:v*8;};
    const auto hands=o["hands"].toArray(); if(hands.size()!=players)return false;
    for(auto v:hands) {
        auto a=v.toArray(); if(a.size()!=8)return false; Hand hand;
        for(int i=0;i<8;++i){ if(!validValue(a[i]))return false; hand[i]={a[i].isNull()?-1:cardId(a[i].toInt()),!a[i].isNull()}; }
        n.hands.push_back(hand);
    }
    if(!validValue(o["discard"]) || !validValue(o["drawn"]))return false;
    int discardCount=o["discardCount"].toInt();
    if((discardCount==0)!=o["discard"].isNull())return false;
    n.discards.assign(discardCount,-1);if(discardCount)n.discards.back()=cardId(o["discard"].toInt());
    if(n.phase==Phase::Place) {
        if(n.current==viewer && o["drawn"].isNull())return false;
        if(n.current!=viewer && !o["drawn"].isNull())return false;
        n.held=n.current==viewer?cardId(o["drawn"].toInt()):-2;
    } else if(!o["drawn"].isNull())return false;
    n.deck.assign(o["stockCount"].toInt(),-1); n.turns.assign(players,0);
    auto scores=o["scores"].toArray(); bool over=n.phase==Phase::HoleOver || n.phase==Phase::MatchOver;
    if(scores.size()!=n.hole-(over?0:1))return false;
    if((n.phase==Phase::MatchOver)!=(over && n.hole==n.holes))return false;
    for(auto v:scores) { auto a=v.toArray(); if(a.size()!=players)return false; std::vector<int> row; for(auto x:a){if(!integer(x,-40,96))return false;row.push_back(x.toInt());}n.scores.push_back(row); }
    auto names=o["names"].toArray(); if(names.size()!=players)return false;
    for(auto name:names) { if(!name.isString() || name.toString().size()>24)return false; n.names.append(name.toString()); }
    if(!o["message"].isString() || o["message"].toString().size()>500)return false;
    n.message=o["message"].toString();
    for(int p=0;p<players;++p) {
        int down=int(std::count_if(n.hands[p].begin(),n.hands[p].end(),[](auto slot){return !slot.up;}));
        if(over && down!=0)return false;
        if(n.phase==Phase::Opening && ((p<n.current && down!=1)||(p==n.current && down<2)||(p>n.current && down!=8)))return false;
        if(!over && n.phase!=Phase::Opening && down!=1)return false;
    }
    s=std::move(n); return true;
}

}
