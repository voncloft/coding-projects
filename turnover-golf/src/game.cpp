#include "game.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <sstream>

namespace Golf {
int value(int card) {
    if(card < 0) return Hidden;
    const int local = card % CardsPerDeck;
    return local >= 104 ? -5 : local / 8;
}
bool Rules::valid() const {
    return ((columns==4 && rows==2) || (columns==3 && rows==3) || (columns==4 && rows==4)) &&
        decks>=1 && decks<=MaxDecks && (matching==Matching::Columns || matching==Matching::Pairs) && cpuTarget>=-20 && cpuTarget<=6 && (!cancelRows || rows>=3);
}
namespace {
bool readRules(const QJsonObject &o, Rules &rules, bool required, bool requireRows=false) {
    if(!required && !o.contains("columns") && !o.contains("rows") && !o.contains("decks") && !o.contains("matching") && !o.contains("cpuTarget") && !o.contains("cancelRows")) { rules.cpuTarget=6; return true; }
    for(const auto &key:{"columns","rows","decks","matching","cpuTarget"})
        if(!o[key].isDouble() || o[key].toDouble()!=o[key].toInt(-1)) return false;
    rules.columns=o["columns"].toInt();rules.rows=o["rows"].toInt();
    rules.decks=o["decks"].toInt();rules.matching=Matching(o["matching"].toInt());
    rules.cpuTarget=o["cpuTarget"].toInt();
    if((requireRows || o.contains("cancelRows")) && !o["cancelRows"].isBool()) return false;
    rules.cancelRows=o["cancelRows"].toBool(false);
    return rules.valid();
}
}
Game::Game(unsigned seed) : rng(seed) { newMatch(); }
QString Game::playerName(int p) { return p == 0 ? QStringLiteral("You") : QStringLiteral("Computer %1").arg(p); }
void Game::newMatch(int opponents, int holes, const Rules &rules) {
    s = State{};
    s.rules = rules.valid() ? rules : Rules{};
    s.opponents = std::clamp(opponents, 1, 4); // Retain support for older five-seat saves.
    s.holes = std::clamp(holes, 1, 99);
    for(int p=0;p<players();++p) s.names.append(playerName(p));
    deal();
}
void Game::deal() {
    s.deck.resize(s.rules.deckSize());
    std::iota(s.deck.begin(), s.deck.end(), 0);
    std::shuffle(s.deck.begin(), s.deck.end(), rng);
    s.hands.assign(players(), Hand(s.rules.cardCount()));
    for (int i = 0; i < s.rules.cardCount(); ++i) for (auto &hand : s.hands) {
        hand[i] = {s.deck.back(), false}; s.deck.pop_back();
    }
    s.discards = {s.deck.back()}; s.deck.pop_back();
    s.turns.assign(players(),0);
    s.held = -1; s.endingBy = -1; s.fromDiscard = false;
    s.current = 0; s.phase = Phase::Opening;
    s.message = QString("All cards are dealt face down. Turn %1 of your cards face up; leave one face down.").arg(s.rules.cardCount()-1);
}
int Game::hidden(int p) const {
    return int(std::count_if(s.hands.at(p).begin(), s.hands.at(p).end(), [](auto x){ return !x.up; }));
}
int Game::visibleValue(int p, int i) const { auto x = s.hands.at(p).at(i); return x.up ? value(x.card) : Hidden; }
int Game::total(int p) const { int n = 0; for (const auto &row : s.scores) n += row.at(p); return n; }
bool Game::active() const { return s.phase == Phase::Opening || s.phase == Phase::Draw || s.phase == Phase::Place; }
bool Game::open(int i) {
    if (s.phase != Phase::Opening || i < 0 || i >= s.rules.cardCount() || s.hands[s.current][i].up || hidden(s.current)<=1) return false;
    s.hands[s.current][i].up = true;
    if (hidden(s.current) == 1) {
        ++s.current;
        if(s.current==players()) { s.phase=Phase::Draw; s.current=(s.hole-1)%players(); s.message=QString("%1 cards face up, one face down. Draw to play, or flip your final card to end the round.").arg(s.rules.cardCount()-1); }
        else s.message=QString("%1 is turning %2 cards face up.").arg(name(s.current)).arg(s.rules.cardCount()-1);
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
    if (s.phase != Phase::Place || i < 0 || i >= s.rules.cardCount()) return false;
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
    for(int i=0;i<s.rules.cardCount();++i) if(!s.hands[s.current][i].up) return replace(i);
    return false;
}
void Game::endTurn() {
    ++s.turns[s.current];
    s.current = (s.current + 1) % players(); s.phase = Phase::Draw; s.fromDiscard = false;
}
Score Game::score(const Hand &hand, bool visibleOnly, const Rules &rules) {
    Score result;
    if(!rules.valid() || int(hand.size())!=rules.cardCount()) return result;
    std::array<int,14> groups{};
    for(int c=0;c<rules.columns;++c) {
        std::array<std::array<int,4>,14> ranks{};
        std::array<int,14> counts{};
        for(int r=0;r<rules.rows;++r) {
            int i=r*rules.columns+c;
            if(visibleOnly && !hand[i].up) continue;
            const int v=value(hand[i].card);
            if(v==Hidden) continue;
            result.columns[c]+=v;
            const int rank=v==-5?13:v;
            ranks[rank][counts[rank]++]=i;
        }
        for(int rank=0;rank<14;++rank) {
            const int count=counts[rank];
            const int matches=rules.matching==Matching::Columns ? (count==rules.rows?1:0) : count/2;
            const int matchedCards=matches*(rules.matching==Matching::Columns?rules.rows:2);
            groups[rank]+=matches;
            if(rank!=13) result.columns[c]-=matchedCards*rank;
            for(int k=0;k<matchedCards;++k) result.matched[ranks[rank][k]]=true;
        }
    }
    if(rules.cancelRows) for(int r=0;r<rules.rows;++r) {
        const int first=r*rules.columns, v=value(hand[first].card);
        bool match=v!=Hidden;
        for(int c=0;c<rules.columns;++c)
            match=match && (!visibleOnly || hand[first+c].up) && value(hand[first+c].card)==v;
        if(!match)continue;
        ++groups[v==-5?13:v];
        for(int c=0;c<rules.columns;++c) {
            // An intersection cancels once; each complete row still earns a group.
            if(v!=-5 && !result.matched[first+c])result.columns[c]-=v;
            result.matched[first+c]=true;
        }
    }
    result.total=std::accumulate(result.columns.begin(),result.columns.end(),0);
    for(int n:groups) if(n>=2) result.bonus-=5*n;
    result.total+=result.bonus;
    return result;
}
void Game::scoreHole() {
    std::vector<int> row;
    for (auto &hand : s.hands) { for (auto &slot : hand) slot.up = true; row.push_back(score(hand,false,s.rules).total); }
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
bool Game::setCpuTarget(int target) {
    if(target < -20 || target > 6)return false;
    s.rules.cpuTarget=target;return true;
}
std::array<int,14> Game::unseenValues() const {
    // Count only information already revealed at the table. Never inspect the stock
    // order or the identity of any face-down card, including our own.
    std::array<int,14> counts; counts.fill(8*s.rules.decks); counts[13]=4*s.rules.decks;
    auto seen=[&](int card){int v=value(card);if(v==Hidden)return;int rank=v==-5?13:v;if(counts[rank]>0)--counts[rank];};
    for(const auto &hand:s.hands)for(auto slot:hand)if(slot.up)seen(slot.card);
    for(int card:s.discards)seen(card);
    if(s.held>=0)seen(s.held);
    return counts;
}
double Game::estimate(const Hand &hand) const { return estimate(hand,unseenValues()); }
double Game::estimate(const Hand &hand, const std::array<int,14> &counts) const {
    int down=0,position=-1;
    for(int i=0;i<int(hand.size());++i)if(!hand[i].up){++down;position=i;}
    if(!down)return score(hand,false,s.rules).total;
    if(down==1) {
        double total=0,weight=0;
        auto possible=hand;
        for(int rank=0;rank<14;++rank)if(counts[rank]) {
            possible[position]={rank==13?104:rank*8,true};
            total+=counts[rank]*score(possible,false,s.rules).total;weight+=counts[rank];
        }
        return weight?total/weight:score(hand,true,s.rules).total;
    }
    // Opening choices are random; only a broad, non-peeking estimate is useful.
    double sum=0,weight=0;
    for(int rank=0;rank<14;++rank){sum+=counts[rank]*(rank==13?-5:rank);weight+=counts[rank];}
    return score(hand,true,s.rules).total+down*(weight?sum/weight:0);
}
double Game::handQuality(const Hand &hand) const {
    const auto counts=unseenValues();
    const double now=estimate(hand,counts);double improvement=0,weight=0;
    auto next=hand;
    // Look one draw ahead: a setup for a second matching pair can be worth keeping
    // even when a different replacement lowers the immediate total a little more.
    for(int rank=0;rank<14;++rank)if(counts[rank]) {
        double best=now;
        for(int i=0;i<s.rules.cardCount();++i)if(hand[i].up) {
            next[i]={rank==13?104:rank*8,true};
            best=std::min(best,estimate(next,counts));
            next[i]=hand[i];
        }
        improvement+=counts[rank]*(now-best);weight+=counts[rank];
    }
    return now-(weight?0.65*improvement/weight:0);
}
std::pair<int,double> Game::bestReplacement(int card) const {
    // estimate() only reads face-up cards, never the identity of a hidden slot.
    const auto &hand = s.hands[s.current];
    double best = -1e9, before = handQuality(hand); int position = 0;
    auto candidate=hand;
    for (int i = 0; i < s.rules.cardCount(); ++i) {
        if(!hand[i].up) continue;
        candidate[i] = {card, true};
        double gain = before - handQuality(candidate);
        candidate[i]=hand[i];
        if (gain > best) { best = gain; position = i; }
    }
    return {position,best};
}
bool Game::shouldFinish(int card,double replacementGain) const {
    if(s.turns[s.current]<6)return false;
    auto finished=s.hands[s.current];
    for(auto &slot:finished)if(!slot.up)slot={card,true};
    const int mine=score(finished,false,s.rules).total;
    // A poor score never becomes an acceptable finish just because other hands
    // look worse or time has passed. Seek a low single-digit or negative score.
    if(mine>s.rules.cpuTarget)return false;
    double other=10000;
    for(int p=0;p<players();++p)if(p!=s.current)other=std::min(other,estimate(s.hands[p]));
    if(mine>=other-3.0)return false;
    // Keep a useful draw in play when it can materially improve a positive hand.
    return mine<=0 || replacementGain<4.0;
}
bool Game::shouldFlipFinal() const {
    if(s.turns[s.current]<6)return false;
    const auto &hand=s.hands[s.current];int down=-1;
    for(int i=0;i<s.rules.cardCount();++i)if(!hand[i].up)down=i;
    if(down<0)return false;
    // Do not peek. Flip only when every still-possible card gives a reasonable
    // finish, so a bad face-down card cannot turn a voluntary finish into 28.
    const auto counts=unseenValues();int worst=-1000;
    auto possible=hand;
    for(int rank=0;rank<14;++rank)if(counts[rank]) {
        possible[down]={rank==13?104:rank*8,true};
        worst=std::max(worst,score(possible,false,s.rules).total);
    }
    if(worst>s.rules.cpuTarget || worst==-1000)return false;
    double other=10000;
    for(int p=0;p<players();++p)if(p!=s.current)other=std::min(other,estimate(s.hands[p]));
    return estimate(hand)<other-3.0;
}
void Game::computerStep() {
    if (!active()) return;
    if(s.phase==Phase::Opening) {
        std::vector<int> positions(s.rules.cardCount());
        std::iota(positions.begin(),positions.end(),0);
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
    QJsonObject o{{"format",4},{"columns",s.rules.columns},{"rows",s.rules.rows},{"decks",s.rules.decks},{"matching",int(s.rules.matching)},{"cpuTarget",s.rules.cpuTarget},{"cancelRows",s.rules.cancelRows},{"opponents",s.opponents},{"holes",s.holes},{"hole",s.hole},
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
    const int format=o["format"].toInt();
    if((format!=2 && format!=3 && format!=4) || !readRules(o,n.rules,format>=3,format>=4)) return false;
    n.opponents=o["opponents"].toInt(); n.holes=o["holes"].toInt(); n.hole=o["hole"].toInt();
    n.current=o["current"].toInt(-1); n.held=o["held"].toInt(-2); n.endingBy=o["endingBy"].toInt(-3);
    n.fromDiscard=o["fromDiscard"].toBool(); int phase=o["phase"].toInt(-1);
    if(n.opponents<1 || n.opponents>4 || (n.holes<1 || n.holes>99) || n.hole<1 || n.hole>n.holes || n.current<0 || n.current>n.opponents || phase<0 || phase>5 || phase==int(Phase::Reveal) || n.held< -1 || n.held>=n.rules.deckSize() || n.endingBy< -2 || n.endingBy>n.opponents) return false;
    n.phase=Phase(phase); n.message=o["message"].toString();
    for(auto v:o["names"].toArray()) n.names.append(v.toString().left(24));
    if(n.names.size()!=n.opponents+1) { n.names.clear(); for(int p=0;p<=n.opponents;++p)n.names.append(playerName(p)); }
    for(auto v:o["turns"].toArray()) { if(!v.isDouble() || v.toInt()<0) return false; n.turns.push_back(v.toInt()); }
    if(int(n.turns.size())!=n.opponents+1) return false;
    std::set<int> ids;
    auto add = [&ids,&n](int id){ return id>=0 && id<n.rules.deckSize() && ids.insert(id).second; };
    for (auto v:o["deck"].toArray()) { int id=v.toInt(-1); if(!add(id)) return false; n.deck.push_back(id); }
    for (auto v:o["discards"].toArray()) { int id=v.toInt(-1); if(!add(id)) return false; n.discards.push_back(id); }
    for (auto v:o["hands"].toArray()) {
        auto a=v.toArray(); if(a.size()!=n.rules.cardCount()) return false; Hand h(n.rules.cardCount());
        for(int i=0;i<n.rules.cardCount();++i) { auto x=a[i].toObject(); int id=x["card"].toInt(-1); if(!add(id) || !x["up"].isBool()) return false; h[i]={id,x["up"].toBool()}; } n.hands.push_back(h);
    }
    if(n.held>=0 && !add(n.held)) return false;
    if(int(ids.size())!=n.rules.deckSize() || int(n.hands.size())!=n.opponents+1 || ((n.phase==Phase::Place)!=(n.held>=0))) return false;
    for(auto v:o["scores"].toArray()) {
        auto a=v.toArray(); if(a.size()!=n.opponents+1) return false; std::vector<int> row;
        for(auto x:a) { if(!x.isDouble() || x.toInt() < -10*n.rules.cardCount() || x.toInt()>12*n.rules.cardCount()) return false; row.push_back(x.toInt()); } n.scores.push_back(row);
    }
    bool over=n.phase==Phase::HoleOver || n.phase==Phase::MatchOver;
    if(int(n.scores.size())!=n.hole-(over?0:1)) return false;
    if((n.phase==Phase::MatchOver)!=(over && n.hole==n.holes)) return false;
    auto hiddenIn = [&n](int p){ return std::count_if(n.hands[p].begin(),n.hands[p].end(),[](auto x){return !x.up;}); };
    if(n.phase==Phase::Opening) {
        for(int p=0;p<=n.opponents;++p) {
            if(p<n.current && hiddenIn(p)!=1) return false;
            if(p==n.current && hiddenIn(p)<2) return false;
            if(p>n.current && hiddenIn(p)!=n.rules.cardCount()) return false;
        }
    } else if(!over) for(int p=0;p<=n.opponents;++p) if(hiddenIn(p)!=1) return false;
    if(n.phase==Phase::Draw && n.deck.empty() && n.discards.size()<=1) return false;
    if(over) for(int p=0;p<=n.opponents;++p) if(hiddenIn(p)!=0 || n.scores.back()[p]!=score(n.hands[p],false,n.rules).total) return false;
    std::istringstream stream(o["random"].toString().toStdString()); std::mt19937 restored; if(!(stream >> restored)) return false;
    s=std::move(n); rng=restored; return true;
}
QString Game::name(int player) const { return s.names.value(player,playerName(player)); }
void Game::setNames(const QStringList &names) { if(names.size()==players())s.names=names; }
QJsonObject Game::publicView(int viewer) const {
    QJsonObject o{{"columns",s.rules.columns},{"rows",s.rules.rows},{"decks",s.rules.decks},{"matching",int(s.rules.matching)},{"cpuTarget",s.rules.cpuTarget},{"cancelRows",s.rules.cancelRows},{"players",players()},{"rounds",s.holes},{"round",s.hole},
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
    State n; if(!readRules(o,n.rules,false))return false;
    auto integer=[](QJsonValue v,int low,int high){ return v.isDouble() && v.toDouble()==v.toInt(low-1) && v.toInt()>=low && v.toInt()<=high; };
    if(!integer(o["players"],2,5) || !integer(o["rounds"],1,99) || !integer(o["round"],1,o["rounds"].toInt()) || !integer(o["current"],0,o["players"].toInt()-1) || !integer(o["phase"],0,5) || o["phase"].toInt()==3 || !integer(o["stockCount"],0,n.rules.deckSize()) || !integer(o["discardCount"],0,n.rules.deckSize()))return false;
    int players=o["players"].toInt(); if(viewer<0 || viewer>=players)return false;
    n.opponents=players-1; n.holes=o["rounds"].toInt(); n.hole=o["round"].toInt(); n.current=o["current"].toInt(); n.phase=Phase(o["phase"].toInt()); n.fromDiscard=o["fromDiscard"].toBool();
    auto validValue=[&](QJsonValue v){return v.isNull() || integer(v,0,12) || (v.isDouble() && v.toDouble()==-5);};
    auto cardId=[](int v){return v==-5?104:v*8;};
    const auto hands=o["hands"].toArray(); if(hands.size()!=players)return false;
    for(auto v:hands) {
        auto a=v.toArray(); if(a.size()!=n.rules.cardCount())return false; Hand hand(n.rules.cardCount());
        for(int i=0;i<n.rules.cardCount();++i){ if(!validValue(a[i]))return false; hand[i]={a[i].isNull()?-1:cardId(a[i].toInt()),!a[i].isNull()}; }
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
    if(o["stockCount"].toInt()+discardCount+players*n.rules.cardCount()+(n.phase==Phase::Place?1:0)!=n.rules.deckSize())return false;
    n.deck.assign(o["stockCount"].toInt(),-1); n.turns.assign(players,0);
    auto scores=o["scores"].toArray(); bool over=n.phase==Phase::HoleOver || n.phase==Phase::MatchOver;
    if(scores.size()!=n.hole-(over?0:1))return false;
    if((n.phase==Phase::MatchOver)!=(over && n.hole==n.holes))return false;
    for(auto v:scores) { auto a=v.toArray(); if(a.size()!=players)return false; std::vector<int> row; for(auto x:a){if(!integer(x,-10*n.rules.cardCount(),12*n.rules.cardCount()))return false;row.push_back(x.toInt());}n.scores.push_back(row); }
    auto names=o["names"].toArray(); if(names.size()!=players)return false;
    for(auto name:names) { if(!name.isString() || name.toString().size()>24)return false; n.names.append(name.toString()); }
    if(!o["message"].isString() || o["message"].toString().size()>500)return false;
    n.message=o["message"].toString();
    for(int p=0;p<players;++p) {
        int down=int(std::count_if(n.hands[p].begin(),n.hands[p].end(),[](auto slot){return !slot.up;}));
        if(over && down!=0)return false;
        if(n.phase==Phase::Opening && ((p<n.current && down!=1)||(p==n.current && down<2)||(p>n.current && down!=n.rules.cardCount())))return false;
        if(!over && n.phase!=Phase::Opening && down!=1)return false;
    }
    s=std::move(n); return true;
}

}
