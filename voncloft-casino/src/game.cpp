// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 voncloft-casino contributors
#include "game.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <set>
#include <stdexcept>

namespace casino {
QString formatUnits(double amount) {return QLocale().toString(amount,'f',amount==std::floor(amount)?0:1);}
QString gameName(Kind k) {
    switch (k) {
    case Kind::Blackjack: return "Blackjack";
    case Kind::Draw: return "5-card draw";
    case Kind::Stud5: return "5-card stud";
    case Kind::Stud7: return "7-card stud";
    case Kind::Holdem: return "Texas hold’em";
    }
    return {};
}
QJsonObject Card::json() const { return {{"rank", rank}, {"suit", suit}}; }
static Rank rankFive(const Cards &c) {
    std::array<int, 15> counts{};
    for (auto card : c) ++counts[card.rank];
    std::vector<std::pair<int, int>> groups;
    for (int r = 14; r >= 2; --r) if (counts[r]) groups.emplace_back(counts[r], r);
    std::sort(groups.rbegin(), groups.rend());
    bool flush = c.size() == 5 && std::all_of(c.begin(), c.end(), [&](auto x) { return x.suit == c[0].suit; });
    int straight = 0;
    if (c.size() == 5 && groups.size() == 5) {
        for (int high = 14; high >= 5; --high) {
            bool found = true;
            for (int x = 0; x < 5; ++x) if (!counts[high-x == 1 ? 14 : high-x]) found = false;
            if (found) { straight = high; break; }
        }
    }
    Rank out{};
    if (flush && straight) return {8, straight, 0, 0, 0, 0};
    if (groups[0].first == 4) out[0] = 7;
    else if (groups[0].first == 3 && groups.size() > 1 && groups[1].first == 2) out[0] = 6;
    else if (flush) out[0] = 5;
    else if (straight) return {4, straight, 0, 0, 0, 0};
    else if (groups[0].first == 3) out[0] = 3;
    else if (groups[0].first == 2 && groups.size() > 1 && groups[1].first == 2) out[0] = 2;
    else if (groups[0].first == 2) out[0] = 1;
    int index = 1;
    for (auto [count, rank] : groups) { Q_UNUSED(count); if (index < 6) out[index++] = rank; }
    return out;
}
Rank rankHand(const Cards &c) {
    if (c.empty()) return {};
    if (c.size() <= 5) return rankFive(c);
    Rank best{};
    for (size_t a=0; a<c.size()-4; ++a) for (size_t b=a+1; b<c.size()-3; ++b)
    for (size_t d=b+1; d<c.size()-2; ++d) for (size_t e=d+1; e<c.size()-1; ++e)
    for (size_t f=e+1; f<c.size(); ++f) best = std::max(best, rankFive({c[a],c[b],c[d],c[e],c[f]}));
    return best;
}
QString rankName(const Rank &r) {
    static const QStringList names{"High card", "One pair", "Two pair", "Three of a kind", "Straight", "Flush", "Full house", "Four of a kind", "Straight flush"};
    return r[0] == 8 && r[1] == 14 ? "Royal flush" : names.value(r[0]);
}
bool jacksOrBetter(const Cards &c) { auto r = rankHand(c); return r[0] >= 2 || (r[0] == 1 && r[1] >= 11); }
int blackjackValue(const Cards &c, bool *soft) {
    int total = 0, aces = 0;
    for (auto card : c) { total += card.rank == 14 ? 11 : std::min(card.rank, 10); aces += card.rank == 14; }
    while (total > 21 && aces) { total -= 10; --aces; }
    if (soft) *soft = aces > 0;
    return total;
}
QString blackjackTotalText(const Cards &cards) {
    bool soft=false;int value=blackjackValue(cards,&soft);
    return value>21?QString("Bust %1").arg(value):soft?QString("%1 / %2").arg(value-10).arg(value):QString::number(value);
}
Game::Game(quint32 seed) : rng(seed) {}
bool validChipValues(const ChipValues &values) {
    return std::all_of(values.begin(),values.end(),[](int value){return value>=1 && value<=1000000;});
}
QJsonArray chipValuesJson(const ChipValues &values) {
    QJsonArray out;for(int value:values)out.append(value);return out;
}
ChipValues readChipValues(const QJsonArray &values) {
    if(values.size()!=5)return DefaultChipValues;
    ChipValues out{};
    for(int i=0;i<5;++i) {
        if(!values[i].isDouble() || values[i].toDouble()!=values[i].toInt(-1))return DefaultChipValues;
        out[i]=values[i].toInt();
    }
    return validChipValues(out)?out:DefaultChipValues;
}
ChipBreakdown breakDownChips(int amount,const ChipValues &values) {
    ChipBreakdown out;out.change=std::max(0,amount);
    if(!validChipValues(values))return out;
    std::array<int,5> order{0,1,2,3,4};
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return values[a]>values[b];});
    for(int i:order){out.counts[i]=out.change/values[i];out.change%=values[i];}
    return out;
}
qint64 chipInventoryValue(const ChipBreakdown &chips,const ChipValues &values) {
    qint64 total=chips.change;for(int i=0;i<5;++i)total+=qint64(chips.counts[i])*values[i];return total;
}
void addChipInventory(ChipBreakdown &to,const ChipBreakdown &chips) {
    for(int i=0;i<5;++i)to.counts[i]+=chips.counts[i];
    to.change+=chips.change;
}
static ChipBreakdown startingChips(int amount,const ChipValues &values) {
    ChipBreakdown bank;int largest=int(std::max_element(values.begin(),values.end())-values.begin());
    std::array<int,5> order{0,1,2,3,4};std::stable_sort(order.begin(),order.end(),[&](int a,int b){return values[a]<values[b];});
    for(int i:order)if(i!=largest){bank.counts[i]=std::min(5,amount/values[i]);amount-=bank.counts[i]*values[i];}
    addChipInventory(bank,breakDownChips(amount,values));return bank;
}
ChipBreakdown takeChipInventory(ChipBreakdown &from,int amount,const ChipValues &values,const std::array<int,5> &chosen) {
    if(amount<0||amount>chipInventoryValue(from,values))throw std::invalid_argument("Insufficient chip inventory");
    ChipBreakdown taken;bool selected=std::any_of(chosen.begin(),chosen.end(),[](int n){return n!=0;});
    if(selected){
        taken.counts=chosen;
        if(std::any_of(chosen.begin(),chosen.end(),[](int n){return n<0;})||chipInventoryValue(taken,values)!=amount)throw std::invalid_argument("Chip selection does not match payment");
        int exchange=0;for(int i=0;i<5;++i){int owned=std::min(from.counts[i],chosen[i]);from.counts[i]-=owned;exchange+=(chosen[i]-owned)*values[i];}
        if(exchange)takeChipInventory(from,exchange,values);
        return taken;
    }
    taken.change=std::min(amount,from.change);from.change-=taken.change;amount-=taken.change;
    std::array<int,5> order{0,1,2,3,4};std::stable_sort(order.begin(),order.end(),[&](int a,int b){return values[a]>values[b];});
    for(int i:order){int count=std::min(from.counts[i],amount/values[i]);from.counts[i]-=count;taken.counts[i]+=count;amount-=count*values[i];}
    if(amount){
        // Exchange only the smallest remaining chip needed to make change.
        for(auto it=order.rbegin();it!=order.rend();++it)if(from.counts[*it]){
            --from.counts[*it];addChipInventory(from,breakDownChips(values[*it]-amount,values));addChipInventory(taken,breakDownChips(amount,values));break;
        }
    }
    return taken;
}
bool Game::setChipValues(const ChipValues &values) {
    if(!validChipValues(values))return false;
    if(values==cfg.chipValues)return true;
    if(values!=cfg.chipValues && (cfg.kind==Kind::Blackjack||usesAnte()) && !active() && !cashedOut) {
        for(int i=0;i<int(seats.size());++i)clearWager(i);
        message="Chip values changed. Choose chips and place your bets before the deal.";
    }
    for(int i=0;i<int(seats.size());++i){auto &s=seats[i];s.bankChips=startingChips(s.chips,values);s.committedChips=breakDownChips(s.committed,values);carriedChips[i]=breakDownChips(carriedAntes[i],values);for(auto &hand:s.hands)hand.chipBet=breakDownChips(hand.bet,values);}
    cfg.chipValues=values;return true;
}
bool Game::setBlackjackDecks(int count) {
    if(cfg.kind!=Kind::Blackjack||active()||count<1||count>8)return false;
    if(count!=cfg.blackjackDecks){
        for(int i=0;i<int(seats.size());++i)clearWager(i);
        cfg.blackjackDecks=count;deck.clear();
        log(QString("Blackjack uses %1 deck%2. Choose your bets; the next round starts a fresh shoe.").arg(count).arg(count==1?"":"s"));
    }
    return true;
}
int Game::maxStartingHands(int seat) const {
    // A hand (including its final bust card) has at most 30 points with aces
    // counted as one. Reserving 31 per possible hand guarantees a physical
    // shoe cannot run out, even after splits and unusually long small-card runs.
    int capacity=(340*cfg.blackjackDecks-1)/31-1;
    int minimum=*std::min_element(cfg.chipValues.begin(),cfg.chipValues.end());
    for(int i=0;i<int(seats.size());++i)if(i!=seat&&(seats[i].chips>=minimum||seats[i].wagerReady))capacity-=seats[i].wagerReady?seats[i].wagerHands:1;
    return std::clamp(capacity,1,4);
}
QJsonArray handWagersJson(const HandWagers &hands) {
    QJsonArray result;for(const auto &counts:hands)result.append(chipValuesJson(counts));return result;
}
bool Game::placeWager(int seat,const std::array<int,5> &counts,int handCount) {
    if(handCount<1||handCount>4)return false;
    return placeHandWagers(seat,HandWagers(handCount,counts));
}
bool Game::placeHandWagers(int seat,const HandWagers &hands) {
    if(cfg.kind!=Kind::Blackjack||active()||cashedOut||seat<0||seat>=int(seats.size())||hands.empty()||int(hands.size())>maxStartingHands(seat))return false;
    qint64 total=0;std::array<int,5> aggregate{};QStringList amounts;
    for(const auto &counts:hands){
        qint64 amount=0;
        for(int i=0;i<5;++i){if(counts[i]<0||counts[i]>1000000)return false;amount+=qint64(counts[i])*cfg.chipValues[i];aggregate[i]+=counts[i];}
        if(amount<=0)return false;
        total+=amount;amounts<<QString::number(amount);
    }
    auto &seatData=seats[seat];if(total>qint64(seatData.chips)+seatData.wager)return false;
    addChipInventory(seatData.bankChips,seatData.committedChips);
    seatData.committedChips=takeChipInventory(seatData.bankChips,int(total),cfg.chipValues,aggregate);
    seatData.chips+=seatData.wager-int(total);seatData.committed=int(total);seatData.streetBet=int(total);seatData.wager=int(total);
    seatData.wagerChips=aggregate;seatData.wagerHandChips=hands;seatData.wagerHands=int(hands.size());seatData.wagerReady=true;
    ++betSound;sound("bet");
    seatData.status=QString("Bets %1 · ready").arg(amounts.join(" / "));return true;
}
bool Game::clearWager(int seat) {
    if((cfg.kind!=Kind::Blackjack&&!usesAnte()) || active() || cashedOut || seat<0 || seat>=int(seats.size()))return false;
    auto &s=seats[seat];addChipInventory(s.bankChips,s.committedChips);s.committedChips={};s.chips+=s.wager;s.committed=0;s.streetBet=0;s.wager=0;s.wagerHands=1;s.wagerChips={};s.wagerHandChips.clear();s.wagerReady=false;s.status="Choose chips";return true;
}
bool Game::exchangeChips(int seat,const std::array<int,5> &counts) {
    if(cashedOut||seat<0||seat>=int(seats.size()))return false;
    auto &player=seats[seat];ChipBreakdown replacement;replacement.counts=counts;
    if(std::any_of(counts.begin(),counts.end(),[](int n){return n<0;}))return false;
    qint64 total=chipInventoryValue(replacement,cfg.chipValues);
    if(total>player.chips)return false;
    replacement.change=player.chips-int(total);
    if(replacement.counts==player.bankChips.counts&&replacement.change==player.bankChips.change)return false;
    player.bankChips=replacement;sound("bet");return true;
}
int Game::anteDue(int seat) const {
    if(!usesAnte()||seat<0||seat>=int(seats.size()))return 0;
    return std::min(anteValue(),seats[seat].chips+seats[seat].wager);
}
bool Game::postAnte(int seat) {
    if(!usesAnte()||active()||cashedOut||seat<0||seat>=int(seats.size())||seats[seat].wagerReady)return false;
    int amount=anteDue(seat);if(amount<=0)return false;
    auto &s=seats[seat];s.wager=amount;s.wagerReady=true;s.wagerChips=breakDownChips(amount,cfg.chipValues).counts;
    pay(seat,amount,false);if(phase!="finished")s.status=QString("Ante %1 · ready").arg(amount);return true;
}
bool Game::setAnte(int amount) {
    if(!usesAnte()||active()||cashedOut||amount<1||amount>1000000)return false;
    if(amount!=anteValue()){for(int i=0;i<int(seats.size());++i)if(seats[i].wagerReady)clearWager(i);cfg.ante=amount;log(QString("Ante is %1. Place chips before the deal.").arg(amount));}
    return true;
}
bool Game::betsReady() const {
    if(cashedOut||active())return false;
    if(cfg.kind!=Kind::Blackjack&&!usesAnte())return true;
    int funded=0;int minimum=cfg.kind==Kind::Blackjack?*std::min_element(cfg.chipValues.begin(),cfg.chipValues.end()):1;
    for(const auto &s:seats)if(s.chips>=minimum || s.wagerReady){++funded;if(!s.wagerReady || s.wager<=0)return false;}
    return funded>=(cfg.kind==Kind::Blackjack?1:2);
}
void Game::configure(const Config &c) {
    if (c.players.empty() || c.players.size() > 5 || (c.kind != Kind::Blackjack && c.players.size() < 2)) throw std::invalid_argument("Invalid player count");
    if(c.firstCardSeat<0||c.firstCardSeat>=int(c.players.size()))throw std::invalid_argument("Invalid first-card seat");
    if (c.unit < 1 || c.unit > 1000 || (c.kind!=Kind::Blackjack && (c.unit<4 || c.unit%4)) || c.bankroll < c.unit * 5 || c.bankroll > 1000000) throw std::invalid_argument("Invalid chip settings");
    if(c.ante<0||c.ante>1000000)throw std::invalid_argument("Invalid ante");
    if(c.blackjackDecks<1||c.blackjackDecks>8)throw std::invalid_argument("Blackjack decks must be 1–8");
    if(!validChipValues(c.chipValues))throw std::invalid_argument("Invalid chip values");
    if(c.cashoutTarget<0 || c.cashoutTarget>10000000)throw std::invalid_argument("Invalid cash-out target");
    for(const auto &p:c.players)if(p.startingBalance!=-1 && (!std::isfinite(p.startingBalance) || p.startingBalance<0 || p.startingBalance>10000000 || std::floor(p.startingBalance*2)!=p.startingBalance*2))throw std::invalid_argument("Invalid starting balance");
    cfg = c;deck.clear();nextDeck.clear();roundHandLimit=80;soundSession=QUuid::createUuid().toString(QUuid::WithoutBraces);betSound=0;payoutSound=0;soundSequence=0;soundEvents={};
    if (cfg.kind == Kind::Holdem || cfg.kind == Kind::Blackjack) cfg.jacks = false;
    seats.clear();
    for (auto p : cfg.players) { Seat s; s.config = p;double balance=p.startingBalance>=0?p.startingBalance:c.bankroll;s.chips=int(balance);s.halfUnit=int(balance*2)%2;s.bankChips=p.startingInventory.value_or(startingChips(s.chips,cfg.chipValues));if(s.bankChips.change<0||std::any_of(s.bankChips.counts.begin(),s.bankChips.counts.end(),[](int n){return n<0;})||chipInventoryValue(s.bankChips,cfg.chipValues)!=s.chips)throw std::invalid_argument("Invalid starting chip inventory");seats.push_back(s); }
    carriedAntes.assign(seats.size(),0);carriedChips.assign(seats.size(),{});cashedOut=false;cashoutRequested=false;currentBet=0;minimumRaise=c.unit;street=0;raises=0;
    phase = "ready"; turn = -1; handNo = 0; button = -1; history.clear(); board.clear(); dealer.clear(); pending.clear(); showdown = false;
    message = cfg.kind==Kind::Blackjack?"Choose chips and place your bets before the deal.":usesAnte()?QString("Place the %1 ante before the deal.").arg(anteValue()):"Your table is ready. Deal the first hand.";
}
bool Game::setStartingBalance(int seat,double amount) {
    if(handNo || active() || cashedOut || seat<0 || seat>=int(seats.size()) || seats[seat].wagerReady || !std::isfinite(amount) || amount<0 || amount>10000000 || std::floor(amount*2)!=amount*2)return false;
    seats[seat].chips=int(amount);seats[seat].halfUnit=int(amount*2)%2;seats[seat].bankChips=startingChips(int(amount),cfg.chipValues);return true;
}
bool Game::updateProfile(int seat,const QString &name,std::optional<double> balance) {
    if(seat<0||seat>=int(seats.size())||name.simplified().isEmpty())return false;
    if(balance&&(active()||!std::isfinite(*balance)||*balance<0||*balance>10000000||std::floor(*balance*2)!=*balance*2))return false;
    auto &player=seats[seat];
    if(balance){
        if(player.wagerReady)clearWager(seat);
        if(*balance!=player.chips+player.halfUnit*0.5)player.bankChips=startingChips(int(*balance),cfg.chipValues);
        player.chips=int(*balance);player.halfUnit=int(*balance*2)%2;
        player.config.startingBalance=*balance;cfg.players[seat].startingBalance=*balance;
    }
    setPlayerName(seat,name);
    return true;
}
void Game::requestCashout() {
    if(cashedOut)return;
    cashoutRequested=true;
    if(active())log("Cash-out requested. Finish this hand to settle the table.");else checkCashout();
}
bool Game::fundBustedComputers() {
    if(phase!="finished"||cashedOut||cashoutRequested)return false;
    bool funded=false;
    for(auto &s:seats){
        // Zero available chips can also mean a pending all-in wager. Only
        // replenish a settled, busted computer, keeping any half-unit change.
        if(s.config.control!=Control::Computer||s.chips!=0||s.committed||s.wagerReady)continue;
        s.chips+=cfg.bankroll;s.houseDebt+=cfg.bankroll;
        addChipInventory(s.bankChips,startingChips(cfg.bankroll,cfg.chipValues));
        log(QString("%1 borrowed %2 from the house · owes house %3.").arg(s.config.name,formatUnits(cfg.bankroll),formatUnits(s.houseDebt)));
        funded=true;
    }
    if(funded){++payoutSound;sound("payout");}
    return funded;
}
void Game::checkCashout() {
    if(active())return;
    bool target=false;
    if(cfg.cashoutTarget>0)for(const auto &s:seats)if(s.chips+s.halfUnit*0.5>=cfg.cashoutTarget)target=true;
    if(!cashoutRequested && !target)return;
    for(int i=0;i<int(seats.size());++i)if(seats[i].wagerReady)clearWager(i);
    int returned=std::accumulate(carriedAntes.begin(),carriedAntes.end(),0);
    for(int i=0;i<int(seats.size());++i){seats[i].chips+=carriedAntes[i];addChipInventory(seats[i].bankChips,carriedChips[i]);carriedChips[i]={};carriedAntes[i]=0;}
    if(returned){++payoutSound;sound("payout");log("Unopened pot returned to its contributors on cash-out.");}
    cashedOut=true;QStringList totals;
    for(const auto &s:seats)totals<<s.config.name+" "+formatUnits(s.chips+s.halfUnit*0.5)+(s.houseDebt?QString(" (owes house %1)").arg(formatUnits(s.houseDebt)):QString{});
    log((target?QString("Cash-out target reached. "):QString("Cashed out. "))+totals.join(" · "));
}
void Game::rigNextDeck(const Cards &cards) { nextDeck = cards; }
void Game::setPlayerName(int seat,const QString &name) {
    QString clean=name.simplified().left(24);
    if(seat>=0 && seat<int(seats.size()) && !clean.isEmpty()) {cfg.players[seat].name=clean;seats[seat].config.name=clean;}
}
void Game::log(const QString &s) { history.append(s); while (history.size() > 80) history.removeFirst(); message = s; }
void Game::sound(const QString &cue,QJsonObject details) {
    details["id"]=++soundSequence;details["cue"]=cue;soundEvents.append(details);
    // Retain recent actions without growing network snapshots forever.
    while(soundEvents.size()>256)soundEvents.removeFirst();
}
Card Game::take() { if (deck.empty()) throw std::runtime_error("Deck exhausted"); Card c = deck.back(); deck.pop_back(); return c; }
void Game::recordCard(const Card &card,const QString &target,int index,int seat,int hand,bool privateCard) {
    auto value=card.json();if(target=="player"&&cfg.kind!=Kind::Blackjack)value["down"]=privateCard;
    sound("deal",{{"target",target},{"seat",seat},{"handIndex",hand},{"index",index},{"card",value},{"cardsRemaining",int(deck.size())},{"privateSeat",privateCard?(target=="dealer"?-2:seat):-1}});
}
void Game::dealCard(Cards &cards,const QString &target,int seat,int hand,bool privateCard) {
    cards.push_back(take());recordCard(cards.back(),target,int(cards.size())-1,seat,hand,privateCard);
}
int Game::nextSeat(int after, bool funded) const {
    for (int offset = 1; offset <= int(seats.size()); ++offset) {
        int i = (after + offset + int(seats.size())) % int(seats.size());
        if (funded ? seats[i].chips > 0 : seats[i].inHand && !seats[i].folded) return i;
    }
    return -1;
}
std::vector<int> Game::clockwise(int start, bool canAct) const {
    std::vector<int> out;
    if (start < 0) return out;
    for (int x=0; x<int(seats.size()); ++x) {
        int i = (start+x)%int(seats.size());
        if (seats[i].inHand && !seats[i].folded && (!canAct || seats[i].chips > 0)) out.push_back(i);
    }
    return out;
}
int Game::liveCount() const { return int(clockwise(0, false).size()); }
int Game::potValue() const { int n=std::accumulate(carriedAntes.begin(),carriedAntes.end(),0); for (const auto &s : seats) n+=s.committed; return n; }
double Game::totalChips() const { double n=potValue(); for (const auto &s : seats) n+=s.chips+s.halfUnit*0.5; return n; }
ChipBreakdown Game::pay(int i, int amount, bool streetPayment,const std::array<int,5> &chosen) {
    auto &s = seats[i]; amount = std::clamp(amount, 0, s.chips);if(amount>0){++betSound;sound(cfg.kind!=Kind::Blackjack&&amount==s.chips?"allin":"bet");} s.chips -= amount; s.committed += amount;
    auto paid=takeChipInventory(s.bankChips,amount,cfg.chipValues,chosen);addChipInventory(s.committedChips,paid);
    if (streetPayment) s.streetBet += amount;
    return paid;
}
bool Game::deal() {
    if (active() || cashedOut) return false;
    fundBustedComputers();
    if((cfg.kind==Kind::Blackjack||usesAnte()) && !betsReady())return false;
    int funded=0;
    const int minimum=cfg.kind==Kind::Blackjack?*std::min_element(cfg.chipValues.begin(),cfg.chipValues.end()):1;
    for (auto &s : seats) if (s.chips >= minimum || ((cfg.kind==Kind::Blackjack||usesAnte()) && s.wagerReady)) ++funded;
    if (funded < (cfg.kind == Kind::Blackjack ? 1 : 2)) { log("Session complete. Start a new table to refill the play chips."); return false; }
    int startingHands=0;for(const auto &s:seats)if(s.wagerReady)startingHands+=s.wagerHands;
    auto shoePoints=[&]{int points=0;for(auto c:deck)points+=c.rank==14?1:std::min(c.rank,10);return points;};
    const int maximumHands=std::min(startingHands*4,(340*cfg.blackjackDecks-1)/31-1);
    const int reserve=std::max(85*cfg.blackjackDecks,31*(maximumHands+1)+1);
    bool reshuffled=cfg.kind!=Kind::Blackjack||deck.empty()||shoePoints()<reserve||!nextDeck.empty();
    board.clear(); dealer.clear(); pending.clear();
    if(reshuffled){
        deck.clear();
        for(int pack=0;pack<(cfg.kind==Kind::Blackjack?cfg.blackjackDecks:1);++pack)
            for(int suit=0;suit<4;++suit)for(int rank=2;rank<=14;++rank)deck.push_back({rank,suit});
        std::shuffle(deck.begin(),deck.end(),rng);
    }
    roundHandLimit=(shoePoints()-1)/31-1;
    for (auto it=nextDeck.rbegin(); it!=nextDeck.rend(); ++it) {
        auto found=std::find(deck.begin(),deck.end(),*it);
        if (found == deck.end()) throw std::invalid_argument("Invalid rigged deck");
        deck.erase(found); deck.push_back(*it);
    }
    nextDeck.clear();
    for (auto &s : seats) {
        if(cfg.kind==Kind::Holdem){s.committed=0;s.streetBet=0;}
        s.folded=false; s.inHand=(cfg.kind==Kind::Blackjack||usesAnte())?s.wagerReady:s.chips>=minimum;
        s.cards.clear(); s.up.clear(); s.hands.clear();s.outcome.clear();s.roundNet=0; s.status=s.inHand ? "" : "Out of chips";
    }
    if(handNo==0&&cfg.kind!=Kind::Blackjack){
        for(int offset=1;offset<=int(seats.size());++offset){int previous=(cfg.firstCardSeat-offset+int(seats.size()))%int(seats.size());if(seats[previous].inHand){button=previous;break;}}
    }else button=nextSeat(button);
    ++handNo; street=0; currentBet=0; raises=0; opened=false; showdown=false; handIndex=0;
    log(QString(cfg.kind==Kind::Blackjack?"Round %1 · %2":"Hand %1 · %2").arg(handNo).arg(gameName(cfg.kind)));
    if (cfg.kind == Kind::Blackjack) {
        phase="blackjack";
        for(auto &s:seats)if(s.inHand)for(int h=0;h<s.wagerHands;++h){int amount=0;for(int i=0;i<5;++i)amount+=s.wagerHandChips[h][i]*cfg.chipValues[i];Hand hand{{},amount,false,false,{},h};hand.chipBet.counts=s.wagerHandChips[h];s.hands.push_back(hand);}
        sound("round",{{"cardsRemaining",int(deck.size())}});if(reshuffled)sound("shuffle",{{"cardsRemaining",int(deck.size())}});
        for(int c=0;c<2;++c){
            for(int i=0;i<int(seats.size());++i)if(seats[i].inHand)
                for(int h=0;h<int(seats[i].hands.size());++h)dealCard(seats[i].hands[h].cards,"player",i,h);
            dealCard(dealer,"dealer",-1,-1,c==1);
        }
        for (auto &s:seats)for(auto &h:s.hands)if(blackjackValue(h.cards)==21){h.done=true;h.result="Blackjack";}
        if (blackjackValue(dealer)==21) { finishBlackjack(); return true; }
        turn=-1; advanceBlackjack(); return true;
    }
    sound("round");sound("shuffle");
    int cards=cfg.kind==Kind::Draw ? 5 : cfg.kind==Kind::Stud7 ? 3 : 2;
    for (int round=0; round<cards; ++round) for (int i:clockwise(nextSeat(button),false)) {
        bool up=(cfg.kind==Kind::Stud5&&round>0)||(cfg.kind==Kind::Stud7&&round==2);
        seats[i].up.push_back(up);dealCard(seats[i].cards,"player",i,-1,!up);
    }
    int first=nextSeat(button);
    if (cfg.kind==Kind::Holdem) {
        int sb=liveCount()==2 ? button : nextSeat(button), bb=nextSeat(sb);
        pay(sb,cfg.unit/2); pay(bb,cfg.unit); currentBet=cfg.unit; opened=true; raises=1;
        seats[sb].status="Small blind"; seats[bb].status="Big blind"; first=nextSeat(bb);
    } else {
        if (cfg.kind!=Kind::Draw && !cfg.jacks) {
            int low=first;
            for (int i:clockwise(first,false)) if (std::pair(seats[i].cards.back().rank,seats[i].cards.back().suit) < std::pair(seats[low].cards.back().rank,seats[low].cards.back().suit)) low=i;
            int bring=low;
            if (!seats[bring].chips) { auto available=clockwise(nextSeat(bring),true); if (!available.empty()) bring=available.front(); }
            pay(bring,cfg.unit/2); currentBet=seats[bring].streetBet; opened=currentBet>0;
            seats[bring].status="Bring-in"; first=nextSeat(bring);
        }
    }
    beginBetting(first,false); return true;
}
int Game::betSize() const {
    return cfg.unit;
}
int Game::raiseTarget() const { return currentBet < betSize() ? betSize() : currentBet + minimumRaise; }
void Game::beginBetting(int first, bool reset) {
    if (reset) { currentBet=0; raises=0; for (auto &s:seats) { s.streetBet=0; if (s.inHand && !s.folded) s.status=s.chips ? "" : "All in"; } }
    minimumRaise=cfg.unit;phase="betting"; pending=clockwise(first,true);
    raiseRights.assign(seats.size(),true);
    actedAtBet.assign(seats.size(),-1);
    if (pending.size()==1 && seats[pending[0]].streetBet>=currentBet) pending.clear();
    if (pending.empty()) { endBetting(); return; }
    turn=pending.front();
}
QStringList Game::legalActions() const {
    if (!active() || turn<0) return {};
    const auto &s=seats[turn];
    if (phase=="blackjack") {
        const auto &h=s.hands[handIndex]; QStringList actions{"hit","stand"};
        int totalHands=0;for(const auto &seat:seats)totalHands+=int(seat.hands.size());
        if (h.cards.size()==2 && s.chips>=h.bet) actions<<"double";
        if (totalHands<roundHandLimit && h.cards.size()==2 && h.cards[0].rank==h.cards[1].rank && s.chips>=h.bet && std::count_if(s.hands.begin(),s.hands.end(),[&](const Hand &other){return other.origin==h.origin;})<4) actions<<"split";
        return actions;
    }
    if (phase=="draw") return {"draw"};
    QStringList actions{"fold"}; int due=std::max(0,currentBet-s.streetBet);
    actions<<(due?"call":"check");
    bool opener=!cfg.jacks || opened || jacksOrBetter(s.cards);
    bool opponentCanCall=false;
    for (int i:clockwise(0,true)) if(i!=turn) opponentCanCall=true;
    if (s.chips>=raiseTarget()-s.streetBet && opener && opponentCanCall && raiseRights[turn]) actions<<"raise";
    if((due>0 && s.chips<=due) || (s.chips>due && opener && opponentCanCall && raiseRights[turn]))actions<<"allin";
    return actions;
}
bool Game::act(int player, const QString &action, const std::vector<int> &discards, int raiseTo,const std::array<int,5> &chosen) {
    if (player!=turn || !legalActions().contains(action)) return false;
    auto &s=seats[turn];
    ChipBreakdown tender;tender.counts=chosen;const qint64 selected=chipInventoryValue(tender,cfg.chipValues);
    if(std::any_of(chosen.begin(),chosen.end(),[](int n){return n<0||n>10000000;}))return false;
    if(selected){
        int due=action=="allin"?s.chips:action=="call"?std::min(s.chips,currentBet-s.streetBet):action=="raise"?(raiseTo?raiseTo:raiseTarget())-s.streetBet:0;
        if(phase!="betting"||selected!=due)return false;
    }
    if (phase=="blackjack") {
        auto &h=s.hands[handIndex];
        if (action=="hit") { dealCard(h.cards,"player",turn,handIndex); bool soft=false;int value=blackjackValue(h.cards,&soft);h.done=value>21||(value==21&&!soft); }
        if (action=="stand") h.done=true;
        if (action=="double") { int extra=h.bet; addChipInventory(h.chipBet,pay(turn,extra)); h.bet+=extra; dealCard(h.cards,"player",turn,handIndex); h.done=true; }
        if (action=="split") {
            int wager=h.bet; Card other=h.cards.back();auto paid=pay(turn,wager);
            h.cards.pop_back();h.split=true;int origin=h.origin;
            s.hands.insert(s.hands.begin()+handIndex+1,Hand{{other},wager,false,true,{},origin});
            s.hands[handIndex+1].chipBet=paid;
            sound("split",{{"seat",turn},{"handIndex",handIndex}});
            // House rules: split aces remain playable, and a two-card 21
            // after any split is blackjack, with the usual 3:2 payout.
            for(int index: {handIndex,handIndex+1}){auto &splitHand=s.hands[index];dealCard(splitHand.cards,"player",turn,index);splitHand.done=blackjackValue(splitHand.cards)==21;splitHand.result=splitHand.done?"Blackjack":"";}
        }
        // Splitting can reallocate the hands vector, so inspect the active hand again.
        auto &played=s.hands[handIndex];
        if(blackjackValue(played.cards)>21)played.result="Bust";
        s.status=played.result=="Bust"?QString("Hand %1: Bust").arg(handIndex+1):action=="double"?"Doubled down":action=="split"?"Split":action=="hit"?"Hit":"Stood";
        log(s.config.name+" · "+s.status);
        advanceBlackjack(); return true;
    }
    if (phase=="draw") {
        std::set<int> unique(discards.begin(),discards.end());
        if (unique.size()!=discards.size() || unique.size()>5 || (!unique.empty() && (*unique.begin()<0 || *unique.rbegin()>4))) return false;
        for(int index:unique){s.cards[index]=take();recordCard(s.cards[index],"player",index,turn,-1,true);}
        s.status=unique.empty()?"Stood pat":QString("Drew %1").arg(unique.size()); log(s.config.name+" · "+s.status);
        pending.erase(pending.begin());
        if (pending.empty()) { street=1; beginBetting(nextSeat(button)); } else turn=pending.front();
        return true;
    }
    if (action=="fold") { s.folded=true; s.status="Folded"; }
    if (action=="check") s.status="Checked";
    if (action=="call" || (action=="allin" && s.chips<=currentBet-s.streetBet)) { int paid=std::min(s.chips,currentBet-s.streetBet); pay(turn,paid,true,chosen); s.status=s.chips?QString("Called %1").arg(paid):"All in"; }
    bool increased=false, reopened=false;
    if (action=="raise" || (action=="allin" && s.chips>currentBet-s.streetBet)) {
        int target=action=="allin"?s.streetBet+s.chips:raiseTo?raiseTo:raiseTarget(); int before=currentBet;
        if(target<=currentBet || target-s.streetBet>s.chips || (action=="raise" && target<raiseTarget()))return false;
        pay(turn,target-s.streetBet,true,chosen); currentBet=std::max(currentBet,s.streetBet);
        reopened=before<betSize()?currentBet>=betSize():currentBet-before>=minimumRaise;
        if(reopened)minimumRaise=before<betSize()?std::max(betSize(),currentBet):currentBet-before;
        if (reopened) ++raises;
        increased=currentBet>before; opened=true;
        s.status=s.chips?QString("Bet to %1").arg(currentBet):QString("All in · %1").arg(currentBet);
    }
    log(s.config.name+" · "+s.status);
    if (liveCount()==1) { finishPoker(); return true; }
    raiseRights[turn]=action=="check" && currentBet==0;actedAtBet[turn]=currentBet;
    if (increased) {
        if (reopened) for (size_t i=0;i<raiseRights.size();++i) raiseRights[i]=int(i)!=turn;
        else for(size_t i=0;i<raiseRights.size();++i)if(int(i)!=turn && actedAtBet[i]>=0 && currentBet-actedAtBet[i]>=minimumRaise)raiseRights[i]=true;
        auto oldPending=pending;
        pending=clockwise(nextSeat(turn),true);
        pending.erase(std::remove_if(pending.begin(),pending.end(),[&](int i){return i==turn || (!reopened && seats[i].streetBet>=currentBet && std::find(oldPending.begin(),oldPending.end(),i)==oldPending.end());}),pending.end());
    } else pending.erase(pending.begin());
    pending.erase(std::remove(pending.begin(),pending.end(),turn),pending.end());
    if (pending.empty()) endBetting(); else turn=pending.front();
    return true;
}
void Game::advanceBlackjack() {
    if (turn>=0 && !seats[turn].hands[handIndex].done) return;
    int start=turn<0?0:turn;
    for (int i=start; i<int(seats.size()); ++i) {
        if (!seats[i].inHand) continue;
        for (int h=(i==turn?handIndex+1:0); h<int(seats[i].hands.size()); ++h) if (!seats[i].hands[h].done) { turn=i; handIndex=h; return; }
    }
    finishBlackjack();
}
void Game::finishBlackjack() {
    QJsonArray revealed;for(auto c:dealer)revealed.append(c.json());sound("reveal",{{"dealer",revealed}});
    bool needed=false;
    for (auto &s:seats) for (auto &h:s.hands) if (blackjackValue(h.cards)<=21 && !(h.cards.size()==2 && blackjackValue(h.cards)==21)) needed=true;
    if (needed) while (blackjackValue(dealer)<17) dealCard(dealer,"dealer");
    if(potValue()>0)sound("collect");
    int dv=blackjackValue(dealer); bool dealerNatural=dealer.size()==2 && dv==21;
    bool paidOut=false;
    for (auto &s:seats) {
        QStringList results;s.roundNet=0;
        for (auto &h:s.hands) {
            int value=blackjackValue(h.cards), returned=0, extraHalf=0; bool natural=value==21 && h.cards.size()==2;
            if (value>21) h.result="Bust";
            else if (dealerNatural) { if(natural) { returned=h.bet; h.result="Tied"; } else h.result="Lost · dealer blackjack"; }
            else if (natural) { returned=h.bet*5/2;extraHalf=h.bet%2; h.result="Blackjack · 3:2"; }
            else if (dv>21 || value>dv) { returned=h.bet*2; h.result="Won"; }
            else if (value==dv) { returned=h.bet; h.result="Tied"; }
            else h.result="Lost";
            h.returned=returned+extraHalf*0.5;s.roundNet+=h.returned-h.bet;paidOut|=h.returned>0;
            if(returned){addChipInventory(s.bankChips,h.chipBet);addChipInventory(s.bankChips,breakDownChips(returned-h.bet,cfg.chipValues));}
            addChipInventory(s.bankChips,breakDownChips((s.halfUnit+extraHalf)/2,cfg.chipValues));
            s.chips+=returned+(s.halfUnit+extraHalf)/2;s.halfUnit=(s.halfUnit+extraHalf)%2; h.done=true; results<<h.result;
        }
        s.committedChips={};
        s.committed=0; s.streetBet=0;s.wager=0;s.wagerHands=1;s.wagerReady=false;s.wagerChips={};s.wagerHandChips.clear(); if(s.inHand){s.outcome=results.size()==1?results.first():QString("%1 hands · %2%3").arg(results.size()).arg(s.roundNet>0?"+":"").arg(formatUnits(s.roundNet));s.status=s.outcome;}
    }
    if(paidOut){++payoutSound;sound("payout");}
    phase="finished"; turn=-1; showdown=true; log(QString("Dealer %1%2 · hand complete").arg(dv).arg(dv>21?" — bust":""));checkCashout();
}
Cards Game::visibleCards(int seat) const { Cards out; for(size_t c=0;c<seats[seat].cards.size();++c) if(seats[seat].up[c]) out.push_back(seats[seat].cards[c]); return out; }
int Game::studLeader() const {
    int best=nextSeat(button);
    for (int i:clockwise(best,false)) if(rankHand(visibleCards(i))>rankHand(visibleCards(best))) best=i;
    return best;
}
void Game::endBetting() {
    if (cfg.kind==Kind::Draw && street==0) {
        if (cfg.jacks && !opened) { finishPoker(true); return; }
        phase="draw"; pending=clockwise(nextSeat(button),false); turn=pending.front(); return;
    }
    int last=cfg.kind==Kind::Holdem?3:cfg.kind==Kind::Draw?1:cfg.kind==Kind::Stud7?4:3;
    if (street==last) { finishPoker(cfg.jacks && !opened); return; }
    advancePoker();
}
void Game::advancePoker() {
    ++street;
    if (cfg.kind==Kind::Holdem) {
        take(); // burn: do not play a dealt-card cue
        for (int x=0;x<(street==1?3:1);++x) dealCard(board,"board");
    } else {
        for(int i:clockwise(nextSeat(button),false)){bool up=!(cfg.kind==Kind::Stud7&&street==4);seats[i].up.push_back(up);dealCard(seats[i].cards,"player",i,-1,!up);}
    }
    beginBetting(cfg.kind==Kind::Holdem?nextSeat(button):studLeader());
}
void Game::finishPoker(bool noOpener) {
    if(potValue()>0)sound("collect");
    QStringList winners;bool paidOut=false;
    showdown=liveCount()>1;
    if (noOpener) {
        for(int i=0;i<int(seats.size());++i){auto &s=seats[i];carriedAntes[i]+=s.committed;addChipInventory(carriedChips[i],s.committedChips);s.roundNet=-s.committed;s.outcome=s.inHand?"No opener · pot carries":"Out of chips";s.status=s.outcome;}
        log(QString("Nobody opened with jacks or better. Pot %1 carries over; everyone antes %2 again before the next deal.").arg(std::accumulate(carriedAntes.begin(),carriedAntes.end(),0)).arg(anteValue()));
    } else {
        std::set<int> levels;
        for (const auto &s:seats) if(s.committed) levels.insert(s.committed);
        int previous=0;
        std::vector<int> awards(seats.size(),0);std::vector<bool> won(seats.size(),false),tiedPot(seats.size(),false);
        auto awardPot=[&](int amount,const std::vector<int> &eligible,bool contested){
            if(!amount||eligible.empty())return;
            Rank best{};std::vector<int> tied;
            for(int i:eligible){Cards cards=seats[i].cards;cards.insert(cards.end(),board.begin(),board.end());auto rank=rankHand(cards);if(tied.empty()||rank>best){best=rank;tied={i};}else if(rank==best)tied.push_back(i);}
            auto order=clockwise(nextSeat(button),false);
            std::sort(tied.begin(),tied.end(),[&](int a,int b){return std::find(order.begin(),order.end(),a)<std::find(order.begin(),order.end(),b);});
            if(contested||liveCount()==1)for(int i:tied){won[i]=true;tiedPot[i]=tiedPot[i]||tied.size()>1;}
            for(size_t j=0;j<tied.size();++j)awards[tied[j]]+=amount/int(tied.size())+(int(j)<amount%int(tied.size())?1:0);
        };
        // Earlier unopened antes form a common pot for this deal. Current-hand
        // wagers still form their own side pots, with normal all-in eligibility.
        ChipBreakdown potChips;for(int i=0;i<int(seats.size());++i){addChipInventory(potChips,seats[i].committedChips);addChipInventory(potChips,carriedChips[i]);carriedChips[i]={};}
        int carried=std::accumulate(carriedAntes.begin(),carriedAntes.end(),0);
        std::fill(carriedAntes.begin(),carriedAntes.end(),0);
        if(levels.empty())awardPot(carried,clockwise(0,false),true);
        for (int level:levels) {
            int amount=0,contributors=0;std::vector<int> eligible;
            for(int i=0;i<int(seats.size());++i)if(seats[i].committed>=level){++contributors;amount+=level-previous;if(!seats[i].folded&&seats[i].inHand)eligible.push_back(i);}
            previous=level;
            amount+=carried;carried=0;
            if(eligible.empty()){for(int i=0;i<int(seats.size());++i)if(seats[i].committed>=level)awards[i]+=amount/contributors;continue;}
            awardPot(amount,eligible,contributors>1);
        }
        for(int i=0;i<int(seats.size());++i) {
            auto &s=seats[i];s.roundNet=awards[i]-s.committed;s.chips+=awards[i];paidOut|=awards[i]>0;
            addChipInventory(s.bankChips,takeChipInventory(potChips,awards[i],cfg.chipValues));
            s.outcome=!s.inHand?"Out of chips":s.folded?"Folded":won[i]?(tiedPot[i]?"Tied · split pot":"Won"):"Lost";
            if(s.inHand && !s.folded) { Cards c=s.cards; c.insert(c.end(),board.begin(),board.end()); s.status=s.outcome+" · "+(showdown?rankName(rankHand(c)):"Uncontested"); }
            if(awards[i]) { s.status+=QString(" · +%1").arg(awards[i]); winners<<QString("%1 +%2").arg(s.config.name).arg(awards[i]); }
        }
        log(winners.join("   •   "));
    }
    if(paidOut){++payoutSound;sound("payout");}
    for(auto &s:seats) { s.committed=0;s.committedChips={};s.streetBet=0;s.wager=0;s.wagerReady=false;s.wagerChips={}; }
    phase="finished"; turn=-1; pending.clear();checkCashout();
}
void Game::computerStep() {
    if(cashedOut)return;
    if(usesAnte()&&!active()){
        for(int i=0;i<int(seats.size());++i)if(seats[i].config.control==Control::Computer&&!seats[i].wagerReady&&seats[i].chips>0){postAnte(i);return;}
        return;
    }
    if(cfg.kind==Kind::Blackjack && !active()) {
        int minimum=*std::min_element(cfg.chipValues.begin(),cfg.chipValues.end());
        for(int i=0;i<int(seats.size());++i)if(seats[i].config.control==Control::Computer && !seats[i].wagerReady && seats[i].chips>=minimum) {
            auto parts=breakDownChips(std::max(minimum,std::min(cfg.unit,seats[i].chips)),cfg.chipValues);
            placeWager(i,parts.counts);return;
        }
        return;
    }
    if(turn<0 || !active() || seats[turn].config.control!=Control::Computer) return;
    const auto &s=seats[turn]; auto actions=legalActions();
    if(phase=="blackjack") {
        const auto &h=s.hands[handIndex]; bool soft=false; int value=blackjackValue(h.cards,&soft), up=std::min(dealer[0].rank,10); if(dealer[0].rank==14) up=11;
        QString choice="stand";
        if(actions.contains("split") && (h.cards[0].rank==14 || h.cards[0].rank==8)) choice="split";
        else if(actions.contains("double") && ((!soft && value==11) || (!soft && value==10 && up<10))) choice="double";
        else if(value<12 || (soft && value<18) || (!soft && value<17 && (up>=7 || (value==12 && up<4)))) choice="hit";
        act(turn,choice); return;
    }
    if(phase=="draw") {
        auto rank=rankHand(s.cards); std::array<int,15> counts{}; for(auto c:s.cards) ++counts[c.rank];
        std::vector<int> discard;
        if(rank[0]<4) for(int i=0;i<5;++i) if(counts[s.cards[i].rank]==1 && !(rank[0]==0 && s.cards[i].rank>=13)) discard.push_back(i);
        act(turn,"draw",discard); return;
    }
    Cards known=s.cards; known.insert(known.end(),board.begin(),board.end()); auto rank=rankHand(known);
    // Do not roll over a jacks-to-open pot simply because a computer randomly
    // checked a qualifying opener. Human players retain their normal choices.
    if(cfg.jacks&&!opened){
        if(actions.contains("raise")){act(turn,"raise");return;}
        if(actions.contains("allin")){act(turn,"allin");return;}
    }
    double roll=std::uniform_real_distribution<double>(0,1)(rng);
    int due=currentBet-s.streetBet;
    bool strong=rank[0]>=2 || (rank[0]==1 && rank[1]>=11);
    if(actions.contains("allin") && rank[0]>=3 && roll<0.08)act(turn,"allin");
    else if(actions.contains("raise") && ((strong && roll<0.65) || roll<0.055)) act(turn,"raise");
    else if(actions.contains("check")) act(turn,"check");
    else if(actions.contains("call") && (rank[0]>0 || due<=cfg.unit || roll<0.55 || known.size()<5)) act(turn,"call");
    else act(turn,"fold");
}
QJsonObject Game::snapshot(int viewer) const {
    QJsonArray people;
    for(int i=0;i<int(seats.size());++i) {
        const auto &s=seats[i]; QJsonArray cards, hands;
        for(size_t c=0;c<s.cards.size();++c) {
            bool visible=s.up[c] || i==viewer || (showdown && !s.folded);
            auto card=visible?s.cards[c].json():QJsonObject{{"hidden",true}};
            card["down"]=!s.up[c]; cards.append(card);
        }
        for(const auto &h:s.hands) { QJsonArray hc; for(auto c:h.cards) hc.append(c.json()); hands.append(QJsonObject{{"cards",hc},{"bet",h.bet},{"value",blackjackValue(h.cards)},{"totalText",blackjackTotalText(h.cards)},{"origin",h.origin},{"result",h.result},{"returned",h.returned},{"net",phase=="finished"?h.returned-h.bet:0.0},{"done",h.done}}); }
        people.append(QJsonObject{{"name",s.config.name},{"control",int(s.config.control)},{"houseDebt",s.houseDebt},{"chips",s.chips+s.halfUnit*0.5},{"bankChips",chipValuesJson(s.bankChips.counts)},{"bankChange",s.bankChips.change+s.halfUnit*0.5},{"committedChips",chipValuesJson(s.committedChips.counts)},{"committedChange",s.committedChips.change},{"wager",s.wager},{"anteDue",anteDue(i)},{"maxWagerHands",maxStartingHands(i)},{"wagerHands",s.wagerHands},{"wagerReady",s.wagerReady},{"wagerChips",chipValuesJson(s.wagerChips)},{"wagerHandChips",handWagersJson(s.wagerHandChips)},{"bet",s.streetBet},{"committed",s.committed},{"folded",s.folded},{"inHand",s.inHand},{"status",s.status},{"outcome",s.outcome},{"roundNet",s.roundNet},{"cards",cards},{"hands",hands}});
    }
    QJsonArray community, house;
    for(auto c:board) community.append(c.json());
    for(size_t i=0;i<dealer.size();++i) house.append(i==1 && active()?QJsonObject{{"hidden",true}}:dealer[i].json());
    QString streetName="Ready to deal";
    if(phase=="finished") streetName="Hand complete";
    if(cfg.kind==Kind::Blackjack && !active())streetName=betsReady()?"Bets placed · ready to deal":"Choose chips before the deal";
    else if(usesAnte()&&!active())streetName=betsReady()?"Antes placed · ready to deal":QString("Place ante %1 before the deal").arg(anteValue());
    else if(phase=="blackjack") streetName="Players’ turns";
    else if(phase=="draw") streetName="Draw · choose cards to replace";
    else if(phase=="betting") {
        if(cfg.kind==Kind::Holdem) streetName=QStringList{"Pre-flop","Flop","Turn","River"}.value(street);
        else if(cfg.kind==Kind::Draw) streetName=street?"Final betting":"Opening betting";
        else streetName=QStringList{"","","Second street","Third street","Fourth street","Fifth street","Sixth street","Seventh street"}.value(street+(cfg.kind==Kind::Stud7?3:2));
    }
    if(cashedOut)streetName="Cashed out · session ended";
    QJsonArray actions;
    if(viewer==turn) for(auto a:legalActions()) actions.append(a);
    QJsonArray events;
    for(const auto &value:soundEvents){
        auto event=value.toObject();
        if(event["cue"]=="deal"){
            int owner=event["privateSeat"].toInt(-1);event.remove("privateSeat");
            if(owner==-2||(owner>=0&&owner!=viewer)){
                auto card=event["card"].toObject();QJsonObject hidden{{"hidden",true}};
                if(card.contains("down"))hidden["down"]=card["down"];
                event["card"]=hidden;
            }
        }
        events.append(event);
    }
    ChipBreakdown potChips;for(int i=0;i<int(seats.size());++i){addChipInventory(potChips,seats[i].committedChips);addChipInventory(potChips,carriedChips[i]);}
    return {{"potChips",chipValuesJson(potChips.counts)},{"potChange",potChips.change},{"deckCount",cfg.kind==Kind::Blackjack?cfg.blackjackDecks:1},{"blackjackDecks",cfg.blackjackDecks},{"cardsRemaining",int(deck.size())},{"roundHandLimit",roundHandLimit},{"soundSequence",soundSequence},{"soundEvents",events},{"soundSession",soundSession},{"betSound",betSound},{"payoutSound",payoutSound},{"kind",int(cfg.kind)},{"game",gameName(cfg.kind)},{"players",people},{"board",community},{"dealer",house},{"dealerValue",active()?0:blackjackValue(dealer)},
        {"turn",turn},{"handIndex",handIndex},{"hand",handNo},{"button",button},{"phase",phase},{"street",streetName},{"message",message},{"pot",potValue()},
        {"unit",cfg.unit},{"usesAnte",usesAnte()},{"ante",anteValue()},{"carriedPot",std::accumulate(carriedAntes.begin(),carriedAntes.end(),0)},{"jacks",cfg.jacks},{"chipValues",chipValuesJson(cfg.chipValues)},{"betsReady",betsReady()},{"currentBet",currentBet},{"raiseTo",raiseTarget()},{"minimumRaise",minimumRaise},{"cashedOut",cashedOut},{"cashoutRequested",cashoutRequested},{"cashoutTarget",cfg.cashoutTarget},{"viewer",viewer},{"actions",actions},{"history",QJsonArray::fromStringList(history)}};
}
}
