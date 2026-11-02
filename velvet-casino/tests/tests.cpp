// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Velvet Casino contributors
#include "window.h"
#include <QAudioDevice>
#include <QMediaDevices>
#include <QtTest>
#include <set>
using namespace casino;
static Config config(Kind kind,int count=2,bool jacks=false) {
    Config c;c.kind=kind;c.jacks=jacks;c.players.clear();
    // Established rule fixtures start with seat 0 as dealer. Production starts
    // with the host receiving the first card; that rotation has its own tests.
    c.firstCardSeat=kind==Kind::Blackjack?0:1;
    for(int i=0;i<count;++i)c.players.push_back({QString("Player %1").arg(i+1),Control::Computer});
    return c;
}
static bool deal(Game &g) {
    g.fundBustedComputers();
    if(g.config().kind==Kind::Blackjack)for(int i=0;i<int(g.players().size());++i)if(!g.players()[i].wagerReady&&g.players()[i].chips>=g.config().unit)g.placeWager(i,breakDownChips(g.config().unit,g.config().chipValues).counts);
    if(g.usesAnte())for(int i=0;i<int(g.players().size());++i)if(!g.players()[i].wagerReady&&g.players()[i].chips>0)g.postAnte(i);
    return g.deal();
}
static double houseLoans(const Game &g) { double total=0;for(const auto &seat:g.players())total+=seat.houseDebt;return total; }
static void finish(Game &g) {
    int guard=0;
    while(g.active() && ++guard<500) {
        auto legal=g.legalActions();
        if(legal.contains("draw"))g.act(g.current(),"draw");
        else if(legal.contains("check"))g.act(g.current(),"check");
        else if(legal.contains("call"))g.act(g.current(),"call");
        else if(legal.contains("stand"))g.act(g.current(),"stand");
        else break;
    }
}
static QJsonObject rulesOnly(QJsonObject state){state.remove("chipValues");state.remove("potChips");state.remove("potChange");auto people=state["players"].toArray();for(int i=0;i<people.size();++i){auto seat=people[i].toObject();for(const char *key:{"bankChips","bankChange","committedChips","committedChange"})seat.remove(key);people[i]=seat;}state["players"]=people;return state;}
static void tableClick(Table *table,const QPointF &point,Qt::MouseButton button=Qt::LeftButton) {
    table->repaint();qreal scale=std::min(table->width()/1280.0,table->height()/820.0);QPointF offset((table->width()-1280*scale)/2,(table->height()-820*scale)/2);
    QTest::mouseClick(table,button,Qt::NoModifier,(offset+point*scale).toPoint());
}
static Cards pokerClickDeck(Kind kind) {
    const std::array<Cards,2> hands{{{{13,0},{13,1},{2,2},{4,2},{6,3},{8,1},{10,2}},{{14,0},{14,1},{3,2},{5,2},{7,3},{9,1},{11,2}}}};
    Cards deck;int count=kind==Kind::Holdem?2:kind==Kind::Stud7?7:5;for(int i=0;i<count;++i)for(int seat=0;seat<2;++seat)deck.push_back(hands[seat][i]);return deck;
}
class CasinoTests:public QObject {
    Q_OBJECT
    QTemporaryDir settingsDirectory;
private slots:
    void initTestCase(){QCoreApplication::setOrganizationName("VelvetCasinoTests");QCoreApplication::setApplicationName("IsolatedTests");QVERIFY(settingsDirectory.isValid());QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDirectory.path());}
    void init(){QSettings().clear();QSettings().setValue("sound/enabled",false);}
    void chipValuesValidateAndPreserveHand() {
        Game g(12);g.configure(config(Kind::Holdem,5));deal(g);auto before=rulesOnly(g.snapshot(0));
        const ChipValues custom{50,100,1,500,1000};QVERIFY(g.setChipValues(custom));QCOMPARE(g.config().chipValues,custom);QCOMPARE(g.snapshot()["chipValues"].toArray(),chipValuesJson(custom));
        auto after=rulesOnly(g.snapshot(0));QCOMPARE(after,before);
        for(ChipValues bad:{ChipValues{0,10,1,25,100},ChipValues{5,-1,1,25,100},ChipValues{5,10,1,25,1000001}}){QVERIFY(!g.setChipValues(bad));QCOMPARE(g.config().chipValues,custom);}
        QCOMPARE(readChipValues(QJsonArray{5,10,1.5,25,100}),DefaultChipValues);
        QCOMPARE(readChipValues(QJsonArray{5,10,1,25}),DefaultChipValues);
        QVERIFY(g.setChipValues({1000000,1000000,1000000,1000000,1000000}));
    }
    void chipBreakdownPreservesExactAmounts() {
        auto standard=breakDownChips(141,DefaultChipValues);QCOMPARE(standard.counts,(std::array<int,5>{1,1,1,1,1}));QCOMPARE(standard.change,0);
        for(const ChipValues values:{DefaultChipValues,ChipValues{7,20,3,50,500},ChipValues{1,1,1,1,1},ChipValues{1000000,1000000,1000000,1000000,1000000}})for(int amount:{0,1,17,141,1000,5000000}) {
            auto parts=breakDownChips(amount,values);qint64 sum=parts.change;for(int i=0;i<5;++i){QVERIFY(parts.counts[i]>=0);sum+=qint64(parts.counts[i])*values[i];}QCOMPARE(sum,qint64(amount));QVERIFY(parts.change>=0);
        }
        auto parts=breakDownChips(62,{7,20,3,50,500});QCOMPARE(parts.change,2);QCOMPARE(parts.counts,(std::array<int,5>{1,0,1,1,0}));
    }
    void physicalChipInventoryKeepsColorsAndAwards() {
        const ChipValues values{5,10,100,25,1000};const ChipBreakdown original{{5,20,0,0,0},0};
        Game g(9);auto c=config(Kind::Blackjack,1);c.chipValues=values;c.players[0].startingBalance=225;c.players[0].startingInventory=original;g.configure(c);
        QVERIFY(g.placeWager(0,{0,10,0,0,0}));QCOMPARE(g.players()[0].bankChips.counts,(std::array<int,5>{5,10,0,0,0}));QVERIFY(g.clearWager(0));QCOMPARE(g.players()[0].bankChips.counts,original.counts);
        QVERIFY(g.placeWager(0,{0,10,0,0,0}));g.rigNextDeck({{10,0},{9,1},{13,0},{8,1}});QVERIFY(g.deal());QVERIFY(g.act(0,"stand"));
        // Return the ten blue chips that were bet, then add ONE white 100
        // chip as winnings. The original five red/twenty blue remain intact.
        QCOMPARE(g.players()[0].chips,325);QCOMPARE(g.players()[0].bankChips.counts,(std::array<int,5>{5,20,1,0,0}));QCOMPARE(g.snapshot(0)["players"].toArray()[0].toObject()["bankChips"].toArray(),QJsonArray({5,20,1,0,0}));
        QVERIFY(g.placeWager(0,{2,0,0,0,0}));QCOMPARE(g.players()[0].bankChips.counts,(std::array<int,5>{3,20,1,0,0}));QVERIFY(g.clearWager(0));QCOMPARE(g.players()[0].bankChips.counts,(std::array<int,5>{5,20,1,0,0}));
        c=config(Kind::Draw,2);c.firstCardSeat=0;c.ante=5;c.chipValues=values;for(auto &player:c.players){player.startingBalance=225;player.startingInventory=original;}g.configure(c);
        g.rigNextDeck({{10,0},{2,1},{11,0},{4,1},{12,0},{6,2},{13,0},{8,3},{14,0},{9,2}});QVERIFY(deal(g));QVERIFY(g.act(0,"raise",{},100,{0,10,0,0,0}));QVERIFY(g.act(1,"call",{},0,{0,10,0,0,0}));
        QCOMPARE(g.snapshot()["potChips"].toArray(),QJsonArray({2,20,0,0,0}));finish(g);
        QCOMPARE(g.players()[0].chips,330);QCOMPARE(g.players()[0].bankChips.counts,(std::array<int,5>{6,30,0,0,0}));QCOMPARE(g.players()[1].bankChips.counts,(std::array<int,5>{4,10,0,0,0}));
        // Exchange just one chip when the requested color is unavailable.
        ChipBreakdown bank{{5,20,1,0,0},0};auto paid=takeChipInventory(bank,1,DefaultChipValues,{0,0,1,0,0});QCOMPARE(paid.counts,(std::array<int,5>{0,0,1,0,0}));QCOMPARE(bank.counts,(std::array<int,5>{5,20,0,0,0}));
        bank={{0,0,0,0,2},0};paid=takeChipInventory(bank,5,DefaultChipValues,{1,0,0,0,0});QCOMPARE(chipInventoryValue(bank,DefaultChipValues),195);QCOMPARE(bank.counts[4],1);QCOMPARE(paid.counts[0],1);
    }
    void exchangeChipStacksPreservesBalanceAndHands() {
        const ChipValues values{1,5,25,10,100};const std::array<int,5> chosen{100,100,0,40,70};
        auto handState=[](QJsonObject s){s=rulesOnly(s);s.remove("soundSequence");s.remove("soundEvents");return s;};
        for(Kind kind:{Kind::Blackjack,Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem}){
            Game g(72);auto c=config(kind,2);c.chipValues=values;c.bankroll=8000;c.players[0].startingBalance=8000.5;g.configure(c);
            QVERIFY(g.exchangeChips(0,chosen));QCOMPARE(g.players()[0].bankChips.counts,chosen);QCOMPARE(g.players()[0].chips,8000);QCOMPARE(g.players()[0].halfUnit,1);
            auto before=g.snapshot();for(auto bad:{std::array<int,5>{-1,0,0,0,0},std::array<int,5>{0,0,0,0,81},std::array<int,5>{0,0,0,0,2147483647}}){QVERIFY(!g.exchangeChips(0,bad));QCOMPARE(g.snapshot(),before);}QVERIFY(!g.exchangeChips(-1,chosen));QVERIFY(!g.exchangeChips(2,chosen));
            QVERIFY(g.exchangeChips(0,{5,19,0,5,0}));QCOMPARE(chipInventoryValue(g.players()[0].bankChips,values),8000);QCOMPARE(g.players()[0].bankChips.change,7850);QCOMPARE(g.players()[0].halfUnit,1);
            QVERIFY(g.exchangeChips(0,chosen));if(kind==Kind::Blackjack)g.rigNextDeck({{10,0},{9,1},{10,1},{8,0},{8,2},{7,0}});QVERIFY(deal(g));QVERIFY(g.active());before=handState(g.snapshot());int unbet=g.players()[0].chips;
            auto wager=g.players()[0].committedChips;QVERIFY(g.exchangeChips(0,{unbet,0,0,0,0}));QCOMPARE(handState(g.snapshot()),before);QCOMPARE(g.players()[0].committedChips.counts,wager.counts);
            finish(g);QVERIFY(!g.active());for(auto &seat:g.players())QCOMPARE(chipInventoryValue(seat.bankChips,values),qint64(seat.chips));g.requestCashout();before=g.snapshot();QVERIFY(!g.exchangeChips(0,{0,0,0,0,0}));QCOMPARE(g.snapshot(),before);
        }
    }
    void exchangeDialogTotalsCancelAndApply() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;c.players[0].startingBalance=8000.5;c.chipValues={1,5,25,10,100};w.startConfig(c);w.resize(1886,939);w.show();QTest::qWait(20);auto geometry=w.canvas()->geometry();auto original=w.engine().players()[0].bankChips.counts;
        const std::array<int,5> wanted{100,100,0,40,70};
        auto open=[&](bool save){QTimer::singleShot(20,&w,[&]{auto *d=w.findChild<QDialog *>("exchangeChipsDialog");QVERIFY(d);auto *apply=d->findChild<QPushButton *>("confirmExchangeButton");auto *label=d->findChild<QLabel *>("exchangeTotal");QVERIFY(!apply->isEnabled());
            for(int i=0;i<5;++i)d->findChild<QSpinBox *>(QString("exchangeCount%1").arg(ChipNames[i]))->setValue(0);
            d->findChild<QSpinBox *>("exchangeCountRed")->setValue(8000);d->findChild<QSpinBox *>("exchangeCountYellow")->setValue(1);QVERIFY(!apply->isEnabled());QVERIFY(label->text().contains("over your available balance"));
            for(int i=0;i<5;++i)d->findChild<QSpinBox *>(QString("exchangeCount%1").arg(ChipNames[i]))->setValue(wanted[i]);QVERIFY(apply->isEnabled());QVERIFY(label->text().contains(formatUnits(8000)));QVERIFY(label->text().contains(formatUnits(.5)));
            QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())QVERIFY(d->grab().save(dir+"/casino-chip-exchange.png"));if(save)apply->click();else d->reject();});w.exchangeChips();};
        QVERIFY(w.findChild<QPushButton *>("exchangeChipsButton")->isEnabled());QVERIFY(w.findChild<QAction *>("exchangeChipsAction")->isEnabled());open(false);QCOMPARE(w.engine().players()[0].bankChips.counts,original);open(true);QCOMPARE(w.engine().players()[0].bankChips.counts,wanted);QCOMPARE(w.engine().players()[0].chips,8000);QCOMPARE(w.engine().players()[0].halfUnit,1);QCOMPARE(w.canvas()->geometry(),geometry);
    }
    void networkChipExchangeValidatesAndSynchronizes() {
        Game g(4),unused;auto c=config(Kind::Blackjack,2);c.bankroll=8000;c.chipValues={1,5,25,10,100};for(auto &seat:c.players)seat.control=Control::Remote;g.configure(c);Network host(&g),b(&unused);Window w(false,false);auto &a=*w.findChild<Network *>();QVERIFY(host.host(0));a.join("127.0.0.1",host.port(),host.roomCode(),"A");QTRY_VERIFY(!a.remoteState().isEmpty());b.join("127.0.0.1",host.port(),host.roomCode(),"B");QTRY_VERIFY(a.ready());QSignalSpy replies(&a,&Network::chipsExchanged);
        const std::array<int,5> wanted{100,100,0,40,70};auto old=a.remoteState();QVERIFY(a.sendExchange(wanted,old));QTRY_COMPARE(replies.size(),1);QVERIFY(replies[0][0].toBool());QCOMPARE(g.players()[0].bankChips.counts,wanted);QTRY_COMPARE(b.remoteState()["players"].toArray()[0].toObject()["bankChips"].toArray(),chipValuesJson(wanted));QCOMPARE(g.totalChips(),16000.0);
        auto before=g.snapshot();QVERIFY(a.sendExchange({8000,0,0,0,0},old));QTRY_COMPARE(replies.size(),2);QVERIFY(!replies[1][0].toBool());QCOMPARE(g.snapshot(),before);
        QVERIFY(a.sendExchange({0,0,0,0,81},a.remoteState()));QTRY_COMPARE(replies.size(),3);QVERIFY(!replies[2][0].toBool());QCOMPARE(g.snapshot(),before);
        old=a.remoteState();QVERIFY(g.setChipValues({5,10,1,25,100}));host.publish();QTRY_COMPARE(a.remoteState()["chipValues"].toArray(),chipValuesJson(g.config().chipValues));before=g.snapshot();QVERIFY(a.sendExchange(wanted,old));QTRY_COMPARE(replies.size(),4);QVERIFY(!replies[3][0].toBool());QCOMPARE(g.snapshot(),before);
        QVERIFY(a.sendExchange({0,0,8000,0,0},a.remoteState()));QTRY_COMPARE(replies.size(),5);QVERIFY(replies[4][0].toBool());QCOMPARE(g.players()[0].bankChips.counts[2],8000);QCOMPARE(g.totalChips(),16000.0);
        QTimer::singleShot(20,&w,[&]{auto *d=w.findChild<QDialog *>("exchangeChipsDialog");QVERIFY(d);QTimer::singleShot(3000,d,&QDialog::reject);d->findChild<QSpinBox *>("exchangeCountWhite")->setValue(1000);d->findChild<QSpinBox *>("exchangeCountYellow")->setValue(70);d->findChild<QPushButton *>("confirmExchangeButton")->click();});w.exchangeChips();
        QTRY_COMPARE(replies.size(),6);QVERIFY(replies[5][0].toBool());QCOMPARE(g.players()[0].bankChips.counts,(std::array<int,5>{0,0,1000,0,70}));QCOMPARE(w.canvas()->state()["players"].toArray()[0].toObject()["bankChips"].toArray(),QJsonArray({0,0,1000,0,70}));QCOMPARE(g.totalChips(),16000.0);
    }
    void computerHouseLoansKeepAllInRulesAndResults() {
        for(Kind kind:{Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem}){
            Game g(93);auto c=config(kind,2);c.firstCardSeat=0;c.bankroll=600;c.players[0].control=Control::Human;c.players[0].startingBalance=2000;c.players[1].startingBalance=40;g.configure(c);
            QVERIFY(!g.fundBustedComputers());
            for(int round=0;round<2;++round){
                int first=round?g.snapshot()["button"].toInt():0;Cards deck;
                const std::array<Cards,2> hands=kind==Kind::Holdem?std::array<Cards,2>{{{{14,0},{13,0}},{{2,1},{3,1}}}}:std::array<Cards,2>{{{{10,0},{11,0},{12,0},{13,0},{14,0},{2,1},{3,1}},{{2,2},{4,3},{6,1},{8,2},{9,3},{5,2},{7,3}}}};
                int count=kind==Kind::Holdem?2:kind==Kind::Stud7?7:5;
                for(int card=0;card<count;++card)for(int offset=0;offset<2;++offset)deck.push_back(hands[(first+offset)%2][card]);
                if(kind==Kind::Holdem){Cards community{{4,1},{12,0},{11,0},{10,0},{5,1},{7,2},{6,1},{9,2}};deck.insert(deck.end(),community.begin(),community.end());}
                g.rigNextDeck(deck);QVERIFY(deal(g));QVERIFY(g.active());QVERIFY(g.act(g.current(),"allin"));
                QVERIFY(g.active());auto live=g.snapshot();QVERIFY(!g.fundBustedComputers());QCOMPARE(g.snapshot(),live);finish(g);
                QVERIFY(!g.active());QCOMPARE(g.players()[1].chips,0);QCOMPARE(g.players()[1].houseDebt,qint64(round*600));
                auto before=g.snapshot();auto humanBank=g.players()[0].bankChips;double total=g.totalChips();QVERIFY(g.fundBustedComputers());
                QCOMPARE(g.totalChips(),total+600);QCOMPARE(g.players()[1].chips,600);QCOMPARE(g.players()[1].houseDebt,qint64((round+1)*600));
                QCOMPARE(chipInventoryValue(g.players()[1].bankChips,c.chipValues),qint64(600));QCOMPARE(g.players()[0].bankChips.counts,humanBank.counts);
                auto after=g.snapshot();for(const char *key:{"hand","button","board","dealer","cardsRemaining","phase","turn"})QCOMPARE(after[key],before[key]);
                auto oldPeople=before["players"].toArray(),newPeople=after["players"].toArray();QCOMPARE(newPeople[0],oldPeople[0]);
                for(const char *key:{"cards","hands","outcome","status","roundNet"})QCOMPARE(newPeople[1].toObject()[key],oldPeople[1].toObject()[key]);
                QCOMPARE(newPeople[1].toObject()["houseDebt"].toInteger(),qint64((round+1)*600));QVERIFY(after["history"].toArray().last().toString().contains("owes house"));
                QVERIFY(!g.fundBustedComputers());QCOMPARE(g.snapshot(),after);
            }
            QCOMPARE(g.players()[0].chips,2640);g.requestCashout();QVERIFY(g.sessionEnded());QVERIFY(g.snapshot()["message"].toString().contains("owes house"));QVERIFY(!g.fundBustedComputers());
            g.configure(c);QCOMPARE(g.players()[1].houseDebt,qint64(0));
        }
    }
    void computerLoansRespectPendingBetsHumansAndCashout() {
        for(int ending:{0,1}){
            Game g(15);auto c=config(Kind::Blackjack,3);c.bankroll=600;c.players[0]={"Human",Control::Human,5};c.players[1]={"Computer",Control::Computer,5.5};c.players[2]={"Remote",Control::Remote,5};g.configure(c);
            for(int i=0;i<3;++i)QVERIFY(g.placeWager(i,{1,0,0,0,0}));QVERIFY(!g.fundBustedComputers());
            g.rigNextDeck({{6,0},{7,0},{8,0},{13,0},{10,1},{9,1},{9,2},{12,1}});QVERIFY(g.deal());QVERIFY(!g.fundBustedComputers());
            if(ending==1)g.requestCashout();finish(g);QCOMPARE(g.players()[0].chips,0);QCOMPARE(g.players()[2].chips,0);
            if(ending==1){QVERIFY(g.sessionEnded());QVERIFY(!g.fundBustedComputers());QCOMPARE(g.players()[1].houseDebt,qint64(0));continue;}
            QVERIFY(g.fundBustedComputers());QCOMPARE(g.players()[1].chips,600);QCOMPARE(g.players()[1].halfUnit,1);QCOMPARE(g.players()[1].houseDebt,qint64(600));
            QCOMPARE(g.players()[0].chips,0);QCOMPARE(g.players()[0].houseDebt,qint64(0));QCOMPARE(g.players()[2].chips,0);QCOMPARE(g.players()[2].houseDebt,qint64(0));
            // Putting the entire new stack in a pending wager must not borrow again.
            QVERIFY(g.placeWager(1,{0,0,0,0,6}));QVERIFY(!g.fundBustedComputers());QCOMPARE(g.players()[1].houseDebt,qint64(600));QVERIFY(g.clearWager(1));
        }
        Game g(16);auto c=config(Kind::Draw,2);c.players[0].control=Control::Human;c.players[1].startingBalance=5;c.firstCardSeat=0;c.cashoutTarget=1005;g.configure(c);
        g.rigNextDeck({{10,0},{2,1},{11,0},{4,1},{12,0},{6,2},{13,0},{8,3},{14,0},{9,2}});QVERIFY(deal(g));finish(g);QVERIFY(g.sessionEnded());QCOMPARE(g.players()[1].chips,0);QVERIFY(!g.fundBustedComputers());QCOMPARE(g.players()[1].houseDebt,qint64(0));
    }
    void computerLoanSynchronizesAndContinuesTable() {
        Window w(false,false);auto c=config(Kind::Draw,3);c.firstCardSeat=0;c.bankroll=600;c.players[0]={"You",Control::Human};c.players[1]={"Ruby",Control::Computer,5};c.players[2]={"Guest",Control::Remote,5};w.startConfig(c,0);w.resize(1886,939);w.show();
        auto *host=w.findChild<Network *>();Game unused;Network client(&unused);client.join("127.0.0.1",host->port(),host->roomCode(),"Guest",5);QTRY_VERIFY(client.ready());
        auto &g=w.engine();g.rigNextDeck({{10,0},{2,2},{3,0},{11,0},{4,3},{5,0},{12,0},{6,1},{7,0},{13,0},{8,2},{9,0},{14,0},{9,3},{8,0}});QVERIFY(deal(g));finish(g);QVERIFY(!g.active());QCOMPARE(g.players()[1].chips,0);QCOMPARE(g.players()[2].chips,0);w.refresh();auto geometry=w.canvas()->geometry();
        QTRY_COMPARE(client.remoteState()["players"].toArray()[1].toObject()["houseDebt"].toInteger(),qint64(600));QTRY_VERIFY(g.players()[1].wagerReady);
        QCOMPARE(g.players()[0].chips,610);QCOMPARE(g.players()[1].chips+g.players()[1].wager,600);QCOMPARE(g.players()[2].chips,0);QCOMPARE(g.players()[2].houseDebt,qint64(0));
        QCOMPARE(w.canvas()->state()["players"].toArray()[1].toObject()["houseDebt"].toInteger(),qint64(600));QCOMPARE(w.canvas()->state()["players"].toArray()[1].toObject()["cards"].toArray().size(),5);QCOMPARE(w.canvas()->geometry(),geometry);
        QCOMPARE(QSettings().value("profile/balance").toDouble(),610.0);
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-house-loan.png"));
        QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());QTRY_COMPARE(g.handNumber(),2);QVERIFY(g.players()[1].inHand);QVERIFY(!g.players()[2].inHand);QCOMPARE(g.snapshot()["button"].toInt(),0);QCOMPARE(g.players()[1].houseDebt,qint64(600));QCOMPARE(w.canvas()->geometry(),geometry);
    }
    void chipInventoriesConserveThroughEveryGame() {
        auto verify=[](const Game &g){for(const auto &s:g.players()){QCOMPARE(chipInventoryValue(s.bankChips,g.config().chipValues),qint64(s.chips));QCOMPARE(chipInventoryValue(s.committedChips,g.config().chipValues),qint64(s.committed));for(int count:s.bankChips.counts)QVERIFY(count>=0);}auto state=g.snapshot();ChipBreakdown pot;auto counts=state["potChips"].toArray();for(int i=0;i<5;++i)pot.counts[i]=counts[i].toInt();pot.change=state["potChange"].toInt();QCOMPARE(chipInventoryValue(pot,g.config().chipValues),qint64(g.potValue()));};
        for(Kind kind:{Kind::Blackjack,Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem})for(bool jacks:{false,true}){
            Game g(329+int(kind));auto c=config(kind,5,jacks);c.bankroll=10000;g.configure(c);verify(g);
            for(int round=0;round<20;++round){QVERIFY(deal(g));verify(g);int guard=0;while(g.active()&&++guard<500){g.computerStep();verify(g);}QVERIFY(!g.active());if(kind!=Kind::Blackjack)QCOMPARE(g.totalChips()-houseLoans(g),50000.0);verify(g);}
            g.requestCashout();verify(g);
        }
        for(ChipValues values:{ChipValues{7,20,3,50,500},ChipValues{1,1,1,1,1},ChipValues{1000000,1000000,1000000,1000000,1000000}}){
            ChipBreakdown bank=breakDownChips(54321,values);for(int amount:{1,7,23,100,5000}){auto total=chipInventoryValue(bank,values);auto paid=takeChipInventory(bank,amount,values);QCOMPARE(chipInventoryValue(paid,values),qint64(amount));QCOMPARE(chipInventoryValue(bank,values),total-amount);addChipInventory(bank,paid);QCOMPARE(chipInventoryValue(bank,values),total);}
        }
    }
    void drawAnteSelectionAndAutomaticDeal() {
        Window w(false,false);auto c=config(Kind::Draw,2);c.firstCardSeat=0;c.ante=7;c.players[0].control=Control::Human;w.startConfig(c);w.resize(1886,939);w.show();QTest::qWait(20);auto geometry=w.canvas()->geometry();
        auto *table=w.canvas();auto *blue=w.findChild<QToolButton *>("betChipBlue");auto *place=w.findChild<QPushButton *>("placeWagerButton");QVERIFY(blue->isVisible());QVERIFY(place->text().contains("7"));QVERIFY(w.findChild<QPushButton *>("dealButton")->isHidden());
        for(const char *name:{"callButton","foldButton","raiseButton","allinButton"})QVERIFY(w.findChild<QPushButton *>(name)->isVisible());
        QTest::qWait(600);QCOMPARE(w.engine().handNumber(),0);QVERIFY(w.engine().players()[1].wagerReady);QVERIFY(!w.engine().players()[0].wagerReady);QVERIFY(w.engine().players()[0].cards.empty());
        QTest::mouseClick(table,Qt::RightButton,Qt::NoModifier,table->rect().center());QCOMPARE(w.engine().handNumber(),0);
        QVERIFY(w.findChild<QLabel *>("wagerLabel")->text().contains("click the table to accept"));
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-draw-ante.png"));
        QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,table->rect().center());QCOMPARE(w.engine().players()[0].chips,993);QTRY_COMPARE_WITH_TIMEOUT(w.engine().handNumber(),1,1500);QCOMPARE(w.engine().potValue(),14);QCOMPARE(w.engine().current(),0);QCOMPARE(w.engine().players()[0].cards.size(),size_t(5));QCOMPARE(table->geometry(),geometry);
        QVERIFY(w.findChild<QPushButton *>("foldButton")->isEnabled());QVERIFY(w.findChild<QPushButton *>("raiseButton")->isEnabled());QVERIFY(w.findChild<QPushButton *>("allinButton")->isEnabled());QVERIFY(!w.findChild<QPushButton *>("callButton")->isEnabled());
        QTest::qWait(20);if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-draw-betting.png"));
        // Placing a full opening bet is an action, with no second confirmation.
        QTest::mouseClick(blue,Qt::LeftButton);QTest::mouseClick(blue,Qt::LeftButton);QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,table->rect().center());QCOMPARE(w.engine().current(),1);QCOMPARE(w.engine().players()[0].streetBet,20);
        QVERIFY(w.engine().act(1,"raise",{},40));w.refresh();QVERIFY(w.findChild<QPushButton *>("callButton")->isEnabled());w.findChild<QPushButton *>("callButton")->click();QCOMPARE(w.engine().stage(),QString("draw"));
        finish(w.engine());w.refresh();QVERIFY(!w.engine().active());QCOMPARE(w.engine().totalChips(),2000.0);QCOMPARE(table->geometry(),geometry);QVERIFY(blue->isVisible());QVERIFY(w.findChild<QPushButton *>("dealButton")->isHidden());QVERIFY(!w.engine().deal());
    }
    void tableAnteRejoinsBustedComputer() {
        for(Kind kind:{Kind::Draw,Kind::Stud5,Kind::Stud7}){
            Window w(false,false);auto c=config(kind,3);c.firstCardSeat=0;c.ante=5;c.players[0].control=Control::Human;c.players[1].startingBalance=5;c.players[2].startingBalance=5;w.startConfig(c);
            const std::array<Cards,3> hands{{{{10,0},{11,0},{12,0},{13,0},{14,0},{2,1},{3,1}},{{2,2},{4,3},{6,1},{8,2},{9,3},{5,2},{7,3}},{{3,0},{5,0},{7,0},{9,0},{8,0},{4,1},{6,2}}}};
            Cards deck;for(int card=0;card<(kind==Kind::Stud7?7:5);++card)for(int seat=0;seat<3;++seat)deck.push_back(hands[seat][card]);
            auto &g=w.engine();g.rigNextDeck(deck);QVERIFY(deal(g));finish(g);QVERIFY(!g.active());QCOMPARE(g.players()[1].chips,0);QCOMPARE(g.players()[2].chips,0);
            auto cards=g.players()[1].cards;w.refresh();w.show();QTest::qWait(20);auto geometry=w.canvas()->geometry();
            QTRY_COMPARE(g.players()[1].houseDebt,qint64(c.bankroll));QCOMPARE(g.players()[2].houseDebt,qint64(c.bankroll));QCOMPARE(g.players()[1].chips+g.players()[1].wager,c.bankroll);QCOMPARE(g.players()[1].cards,cards);
            // No chip selection or ante order is required. The busted computer
            // borrows, antes and returns to the normal clockwise rotation.
            QVERIFY(w.findChild<QPushButton *>("placeWagerButton")->isEnabled());QCOMPARE(w.canvas()->state()["draftAmount"].toInt(),0);
            QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());QCOMPARE(g.players()[0].wager,5);QVERIFY(g.players()[0].wagerReady);
            QTRY_COMPARE_WITH_TIMEOUT(g.handNumber(),2,2000);QVERIFY(g.players()[1].inHand);QVERIFY(!g.players()[1].cards.empty());QCOMPARE(g.snapshot()["button"].toInt(),0);
            QVector<int> dealtTo;for(const auto &value:g.snapshot()["soundEvents"].toArray()){auto event=value.toObject();if(event["cue"]=="round")dealtTo.clear();else if(event["cue"]=="deal")dealtTo.append(event["seat"].toInt());}
            QVERIFY(!dealtTo.isEmpty());QCOMPARE(dealtTo.first(),1);QVERIFY(dealtTo.contains(1));finish(g);QVERIFY(!g.active());QCOMPARE(w.canvas()->geometry(),geometry);
        }
        // A selected amount never changes the fixed ante, even if below it.
        for(int chip:{-1,0,1}){
            Window w(false,false);auto c=config(Kind::Draw,2);c.ante=7;c.players[0].control=Control::Human;c.players[0].startingBalance=chip<0?2:1000;w.startConfig(c);w.show();QTest::qWait(20);
            if(chip>=0)QTest::mouseClick(w.findChild<QToolButton *>(chip?"betChipBlue":"betChipRed"),Qt::LeftButton);
            QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());QCOMPARE(w.engine().players()[0].wager,chip<0?2:7);QCOMPARE(w.engine().players()[0].chips,chip<0?0:993);
        }
    }
    void anteGateRefundsAndShortStacks() {
        for(Kind kind:{Kind::Draw,Kind::Stud5,Kind::Stud7}){
            Game g(7);auto c=config(kind,2);c.ante=7;g.configure(c);QVERIFY(!g.deal());QVERIFY(g.postAnte(0));auto before=g.snapshot();QVERIFY(!g.postAnte(0));QCOMPARE(g.snapshot(),before);QVERIFY(!g.deal());QVERIFY(g.postAnte(1));QVERIFY(g.betsReady());QCOMPARE(g.potValue(),14);QCOMPARE(g.totalChips(),2000.0);
            QVERIFY(g.setAnte(9));QCOMPARE(g.potValue(),0);QCOMPARE(g.players()[0].chips,1000);QVERIFY(!g.betsReady());QVERIFY(!g.setAnte(0));QVERIFY(!g.setAnte(1000001));QVERIFY(g.postAnte(0));QVERIFY(g.clearWager(0));QVERIFY(g.postAnte(0));g.requestCashout();QCOMPARE(g.players()[0].chips,1000);QCOMPARE(g.potValue(),0);
            g.configure(c);QVERIFY(g.postAnte(0));QVERIFY(g.updateProfile(0,"New balance",20));QCOMPARE(g.players()[0].chips,20);QVERIFY(!g.players()[0].wagerReady);QCOMPARE(g.potValue(),0);
            c.players[0].startingBalance=2;g.configure(c);QCOMPARE(g.anteDue(0),2);QVERIFY(g.postAnte(0));QVERIFY(g.postAnte(1));QVERIFY(g.betsReady());QVERIFY(g.deal());if(g.active()){QVERIFY(!g.postAnte(0));QVERIFY(!g.clearWager(1));}finish(g);QVERIFY(!g.active());QCOMPARE(g.totalChips(),1002.0);
        }
    }
    void anteSettingsPersistAndRespectCancel() {
        Window w(false,false);auto c=config(Kind::Draw,2);w.startConfig(c);QVERIFY(w.engine().postAnte(0));w.refresh();
        QTimer::singleShot(0,&w,[&]{auto *d=w.findChild<QDialog *>("anteSettingsDialog");if(d){d->findChild<QSpinBox *>("pokerAnteAmount")->setValue(12);d->accept();}});w.anteSettings();QCOMPARE(w.engine().anteValue(),12);QCOMPARE(w.engine().players()[0].chips,1000);QCOMPARE(QSettings().value("poker/ante").toInt(),12);
        QTimer::singleShot(0,&w,[&]{auto *d=w.findChild<QDialog *>("anteSettingsDialog");if(d){d->findChild<QSpinBox *>("pokerAnteAmount")->setValue(99);d->reject();}});w.anteSettings();QCOMPARE(w.engine().anteValue(),12);Window reopened(false,false);QCOMPARE(reopened.engine().anteValue(),12);
    }
    void hostGetsFirstCardAndDealerRotates() {
        for(Kind kind:{Kind::Blackjack,Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem})for(int count=2;count<=5;++count){
            Game g(6);auto c=config(kind,count);c.firstCardSeat=0;g.configure(c);
            for(int hand=0;hand<4;++hand){QVERIFY(deal(g));if(kind!=Kind::Blackjack)QCOMPARE(g.snapshot()["button"].toInt(),(hand+count-1)%count);int first=-1;for(const auto &value:g.snapshot()["soundEvents"].toArray()){auto event=value.toObject();if(event["cue"]=="round")first=-1;else if(event["cue"]=="deal"&&first<0)first=event["seat"].toInt();}QCOMPARE(first,kind==Kind::Blackjack?0:hand%count);if(kind==Kind::Draw)QCOMPARE(g.current(),hand%count);finish(g);QVERIFY(!g.active());}
        }
    }
    void computerOpensJacksOrBetter() {
        for(Kind kind:{Kind::Draw,Kind::Stud5,Kind::Stud7})for(int pair=10;pair<=14;++pair)for(int seed=1;seed<=12;++seed)for(bool shortStack:{false,true}){
            Game g(seed);auto c=config(kind,2,true);c.players[0].control=Control::Human;if(shortStack)c.players[1].startingBalance=15;g.configure(c);
            Cards opening{{pair,0},{2,1},{pair,1},{7,2}};if(kind!=Kind::Stud5){opening.push_back({4,2});opening.push_back({9,3});}if(kind==Kind::Draw){opening.push_back({6,3});opening.push_back({11,2});opening.push_back({8,0});opening.push_back({13,3});}
            g.rigNextDeck(opening);QVERIFY(deal(g));if(g.current()==0)QVERIFY(g.act(0,"check"));QCOMPARE(g.current(),1);QCOMPARE(g.players()[1].streetBet,0);g.computerStep();
            if(pair>=11){QCOMPARE(g.players()[1].streetBet,shortStack?10:20);QVERIFY(g.active());QCOMPARE(g.current(),0);QVERIFY(g.legalActions().contains("call"));QCOMPARE(g.snapshot()["carriedPot"].toInt(),0);if(shortStack)QCOMPARE(g.players()[1].chips,0);}
            else{QCOMPARE(g.players()[1].streetBet,0);if(kind==Kind::Draw){if(g.active()){QCOMPARE(g.current(),0);QVERIFY(g.act(0,"check"));}QVERIFY(!g.active());QCOMPARE(g.snapshot()["carriedPot"].toInt(),10);}}
        }
    }
    void jacksPotCarriesUntilAnOpenerAndSplitsExactly() {
        auto rig=[](Game &g,const std::vector<Cards> &hands){Cards deck;int first=(g.config().firstCardSeat+g.handNumber())%int(hands.size());for(int card=0;card<5;++card)for(int i=0;i<int(hands.size());++i)deck.push_back(hands[(first+i)%hands.size()][card]);g.rigNextDeck(deck);};
        const std::vector<Cards> low{{{10,0},{10,1},{2,2},{5,3},{8,0}},{{9,0},{9,1},{3,2},{6,3},{12,0}}};
        const std::vector<Cards> royals{{{10,0},{11,0},{12,0},{13,0},{14,0}},{{10,1},{11,1},{12,1},{13,1},{14,1}}};
        Game g(4);auto c=config(Kind::Draw,2,true);c.firstCardSeat=0;c.ante=7;g.configure(c);
        for(int round=1;round<=2;++round){rig(g,low);QVERIFY(deal(g));QVERIFY(!g.legalActions().contains("raise"));QVERIFY(g.act(g.current(),"check"));QVERIFY(g.act(g.current(),"check"));QVERIFY(!g.active());QCOMPARE(g.potValue(),14*round);QCOMPARE(g.players()[0].chips,1000-7*round);QCOMPARE(g.snapshot()["carriedPot"].toInt(),14*round);QCOMPARE(g.snapshot()["payoutSound"].toInt(),0);QCOMPARE(g.totalChips(),2000.0);QVERIFY(!g.deal());}
        rig(g,royals);QVERIFY(deal(g));QCOMPARE(g.potValue(),42);QVERIFY(g.act(g.current(),"raise"));QVERIFY(g.act(g.current(),"call"));finish(g);QCOMPARE(g.potValue(),0);QCOMPARE(g.players()[0].chips,1000);QCOMPARE(g.players()[1].chips,1000);QCOMPARE(g.players()[0].outcome,QString("Tied · split pot"));QVERIFY(g.players()[0].status.contains("Royal flush"));
        // An odd carried pot merges with the new main pot before splitting.
        c=config(Kind::Draw,3,true);c.firstCardSeat=0;g.configure(c);auto low3=low;low3.push_back({{8,1},{8,2},{4,0},{6,0},{13,3}});rig(g,low3);QVERIFY(deal(g));finish(g);QCOMPARE(g.potValue(),15);
        auto hands3=royals;hands3.push_back({{8,1},{8,2},{4,0},{6,0},{13,3}});rig(g,hands3);QVERIFY(deal(g));QCOMPARE(g.current(),1);QVERIFY(g.act(1,"raise"));QVERIFY(g.act(2,"fold"));QVERIFY(g.act(0,"call"));finish(g);QCOMPARE(g.players()[0].chips,1005);QCOMPARE(g.players()[1].chips,1005);QCOMPARE(g.players()[2].chips,990);QCOMPARE(g.totalChips(),3000.0);
        // Cashing out an unopened table refunds carried and newly posted antes.
        g.configure(c);rig(g,low3);QVERIFY(deal(g));finish(g);QVERIFY(g.postAnte(0));g.requestCashout();QVERIFY(g.sessionEnded());QCOMPARE(g.potValue(),0);for(const auto &seat:g.players())QCOMPARE(seat.chips,1000);
    }
    void networkAntesAndCarriedPot() {
        Game g(4);auto c=config(Kind::Draw,2,true);c.firstCardSeat=0;c.ante=7;c.players[0].control=Control::Human;c.players[1].control=Control::Remote;g.configure(c);Network host(&g);QVERIFY(host.host(0));
        Window w(false,false);auto *client=w.findChild<Network *>();client->join("127.0.0.1",host.port(),host.roomCode(),"Remote ante",1000);QTRY_VERIFY(host.ready());QTRY_COMPARE(w.canvas()->state()["ante"].toInt(),7);w.show();QTest::qWait(20);
        QVERIFY(w.findChild<QPushButton *>("dealButton")->isHidden());auto *blue=w.findChild<QToolButton *>("betChipBlue");QVERIFY(blue->isVisible());QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());QTRY_COMPARE(g.players()[1].wager,7);QCOMPARE(g.players()[1].chips,993);QVERIFY(!g.deal());
        client->sendAnte();QTest::qWait(30);QCOMPARE(g.players()[1].wager,7);QVERIFY(g.postAnte(0));
        g.rigNextDeck({{10,0},{9,0},{10,1},{9,1},{2,2},{3,2},{5,3},{6,3},{8,0},{12,0}});QVERIFY(g.deal());host.publish();QTRY_COMPARE(client->remoteState()["hand"].toInt(),1);QCOMPARE(g.current(),0);
        QVERIFY(g.act(0,"check"));host.publish();QTRY_VERIFY(w.findChild<QPushButton *>("checkButton")->isEnabled());w.findChild<QPushButton *>("checkButton")->click();QTRY_VERIFY(!g.active());QTRY_COMPARE(client->remoteState()["carriedPot"].toInt(),14);QCOMPARE(g.totalChips(),2000.0);
        QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());QTRY_COMPARE(g.players()[1].wager,7);QCOMPARE(g.potValue(),21);QVERIFY(!g.deal());
        auto peers=host.findChild<QTcpServer *>()->findChildren<QTcpSocket *>();QVERIFY(!peers.isEmpty());peers.first()->abort();QTRY_VERIFY(!host.ready());QTRY_VERIFY_WITH_TIMEOUT(host.ready(),5000);QTRY_VERIFY(client->ready());QCOMPARE(g.players()[1].wager,7);QCOMPARE(g.potValue(),21);
        g.requestCashout();host.publish();QTRY_VERIFY(client->remoteState()["cashedOut"].toBool());QCOMPARE(g.players()[0].chips,1000);QCOMPARE(g.players()[1].chips,1000);QCOMPARE(g.potValue(),0);QCOMPARE(QSettings().value("profile/balance").toDouble(),1000.0);
    }
    void drawReraisesAndFoldedPlayerWatches() {
        Window w(false,false);auto c=config(Kind::Draw,3);c.firstCardSeat=0;c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(deal(w.engine()));w.refresh();w.show();QTest::qWait(20);
        auto &g=w.engine();QCOMPARE(g.current(),0);QVERIFY(g.act(0,"raise",{},20));QVERIFY(g.act(1,"raise",{},40));QVERIFY(g.act(2,"raise",{},80));QCOMPARE(g.current(),0);QCOMPARE(g.stage(),QString("betting"));
        QVERIFY(!g.act(0,"raise",{},100));QVERIFY(g.act(0,"call"));QCOMPARE(g.current(),1);QVERIFY(g.act(1,"raise",{},120));QVERIFY(g.act(2,"call"));QCOMPARE(g.current(),0);QCOMPARE(g.stage(),QString("betting"));w.refresh();
        const auto cards=g.players()[0].cards;const int stack=g.players()[0].chips,committed=g.players()[0].committed;QCOMPARE(committed,85);
        w.findChild<QPushButton *>("foldButton")->click();QVERIFY(g.players()[0].folded);QCOMPARE(g.players()[0].chips,stack);QCOMPARE(g.potValue(),335);QCOMPARE(g.stage(),QString("draw"));QVERIFY(g.active());
        for(const char *name:{"callButton","raiseButton","allinButton","foldButton"})QVERIFY(!w.findChild<QPushButton *>(name)->isEnabled());QVERIFY(!g.act(0,"call"));QVERIFY(!g.act(0,"draw",{0}));QVERIFY(w.canvas()->isVisible());
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-folded-watching.png"));
        w.findChild<QCheckBox *>("fastTurns")->setChecked(true);QTRY_VERIFY_WITH_TIMEOUT(!g.active(),15000);QCOMPARE(g.players()[0].cards,cards);QCOMPARE(g.players()[0].chips,stack);QCOMPARE(g.players()[0].roundNet,-double(committed));QCOMPARE(g.players()[0].outcome,QString("Folded"));QCOMPARE(g.totalChips(),3000.0);
        QTest::mouseClick(w.findChild<QToolButton *>("betChipRed"),Qt::LeftButton);QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());QTRY_COMPARE_WITH_TIMEOUT(g.handNumber(),2,3000);QVERIFY(!g.players()[0].folded);
    }
    void allInDrawStillAllowsNormalReplacementAndSidePots() {
        Game g(5);auto c=config(Kind::Draw,3);c.firstCardSeat=0;c.players[0].startingBalance=25;g.configure(c);QVERIFY(deal(g));QVERIFY(g.act(0,"allin"));QCOMPARE(g.players()[0].chips,0);QCOMPARE(g.players()[0].committed,25);QVERIFY(!g.act(0,"raise"));
        QVERIFY(g.act(1,"raise",{},40));QVERIFY(g.act(2,"raise",{},80));QVERIFY(g.act(1,"call"));QCOMPARE(g.stage(),QString("draw"));QCOMPARE(g.current(),0);
        const auto old=g.players()[0].cards;QVERIFY(g.act(0,"draw",{0,1,2,3,4}));QCOMPARE(g.players()[0].cards.size(),size_t(5));for(int i=0;i<5;++i)QVERIFY(g.players()[0].cards[i]!=old[i]);QVERIFY(g.act(1,"draw"));QVERIFY(g.act(2,"draw"));
        QCOMPARE(g.current(),1);QVERIFY(g.snapshot(0)["actions"].toArray().isEmpty());QVERIFY(g.act(1,"raise",{},40));QVERIFY(g.act(2,"call"));QVERIFY(!g.active());QCOMPARE(g.totalChips(),2025.0);
    }
    void sevenCardStudAllInOnFourthStreetFinishesCards() {
        for(bool everyone:{false,true}){
            Game g(2);auto c=config(Kind::Stud7,3,true);c.firstCardSeat=0;c.players[0].startingBalance=50;g.configure(c);
            g.rigNextDeck({{11,0},{13,0},{12,0},{11,1},{13,1},{12,1},{14,0},{2,0},{3,0},{4,0},{5,0},{6,0}});QVERIFY(deal(g));QVERIFY(g.act(0,"raise",{},20));QVERIFY(g.act(1,"call"));QVERIFY(g.act(2,"call"));QCOMPARE(g.players()[0].cards.size(),size_t(4));QCOMPARE(g.current(),0);
            auto firstFour=g.players()[0].cards;QVERIFY(g.act(0,"allin"));QCOMPARE(g.players()[0].chips,0);QVERIFY(!g.act(0,"raise"));QVERIFY(!g.act(0,"call"));
            if(everyone){QVERIFY(g.act(1,"allin"));QVERIFY(g.act(2,"call"));QVERIFY(!g.active());}else finish(g);
            QVERIFY(!g.active());QCOMPARE(g.players()[0].cards.size(),size_t(7));for(int i=0;i<4;++i)QCOMPARE(g.players()[0].cards[i],firstFour[i]);QCOMPARE(g.players()[0].up,(std::vector<bool>{false,false,true,true,true,true,false}));QCOMPARE(g.totalChips(),2050.0);
        }
    }
    void pokerHierarchyAndKickers() {
        const std::vector<Cards> ascending{
            {{14,0},{11,1},{8,2},{5,3},{2,0}},{{2,0},{2,1},{14,2},{9,3},{5,0}},{{2,0},{2,1},{3,2},{3,3},{14,0}},
            {{2,0},{2,1},{2,2},{14,3},{13,0}},{{2,0},{3,1},{4,2},{5,3},{6,0}},{{2,0},{4,0},{7,0},{10,0},{13,0}},
            {{2,0},{2,1},{2,2},{3,0},{3,1}},{{2,0},{2,1},{2,2},{2,3},{3,0}},{{2,0},{3,0},{4,0},{5,0},{6,0}},{{10,0},{11,0},{12,0},{13,0},{14,0}}};
        for(size_t i=1;i<ascending.size();++i)QVERIFY(rankHand(ascending[i])>rankHand(ascending[i-1]));QCOMPARE(rankName(rankHand(ascending.back())),QString("Royal flush"));
        QVERIFY(rankHand({{11,0},{11,1},{14,0},{8,1},{2,2}})>rankHand({{11,2},{11,3},{13,0},{8,2},{2,3}}));
        QCOMPARE(rankHand({{14,0},{14,1},{13,0},{13,1},{12,0},{12,1},{2,0}}),(Rank{2,14,13,12,0,0})); // best five of seven: two pair, not three pairs
    }
    void pokerRankings() {
        QCOMPARE(rankHand({{14,0},{2,1},{3,2},{4,3},{5,0}}),(Rank{4,5,0,0,0,0}));
        QVERIFY(rankHand({{14,0},{14,1},{13,2},{8,1},{4,2}})>rankHand({{14,2},{14,3},{12,2},{11,1},{10,2}}));
        QCOMPARE(rankHand({{14,3},{13,3},{12,3},{11,3},{10,3},{2,0},{2,1}}),(Rank{8,14,0,0,0,0}));
        QVERIFY(rankHand({{5,0},{5,1},{5,2},{4,0},{4,1}})>rankHand({{4,2},{4,3},{4,0},{14,0},{14,1}}));
        QCOMPARE(rankHand({{14,0},{14,1},{14,2},{13,0},{13,1},{13,2},{2,0}}),(Rank{6,14,13,0,0,0}));
        QVERIFY(jacksOrBetter({{11,0},{11,1},{2,2},{5,3},{8,0}}));
        QVERIFY(!jacksOrBetter({{10,0},{10,1},{14,2},{13,3},{12,0}}));
        QVERIFY(jacksOrBetter({{2,0},{2,1},{3,2},{3,3},{8,0}}));
    }
    void exhaustiveFiveCardRankCounts() {
        Cards deck;for(int s=0;s<4;++s)for(int r=2;r<=14;++r)deck.push_back({r,s});
        std::array<int,9> counts{};
        for(int a=0;a<48;++a)for(int b=a+1;b<49;++b)for(int c=b+1;c<50;++c)for(int d=c+1;d<51;++d)for(int e=d+1;e<52;++e)++counts[rankHand({deck[a],deck[b],deck[c],deck[d],deck[e]})[0]];
        QCOMPARE(counts,(std::array<int,9>{1302540,1098240,123552,54912,10200,5108,3744,624,40}));
    }
    void blackjackAceValues(){bool soft=false;QCOMPARE(blackjackValue({{14,0},{14,1},{9,2}},&soft),21);QVERIFY(soft);QCOMPARE(blackjackValue({{14,0},{14,1},{9,2},{5,3}},&soft),16);QVERIFY(!soft);}
    void softAceTotalsAndHardConversion() {
        QCOMPARE(blackjackTotalText({{14,0},{5,1}}),QString("6 / 16"));
        QCOMPARE(blackjackTotalText({{14,0},{5,1},{13,2}}),QString("16"));
        QCOMPARE(blackjackTotalText({{14,0},{14,1},{5,2}}),QString("7 / 17"));
        QCOMPARE(blackjackTotalText({{10,0},{9,1},{5,2}}),QString("Bust 24"));
        Game g(3);g.configure(config(Kind::Blackjack,1));g.rigNextDeck({{14,0},{10,1},{5,0},{7,1},{13,2}});QVERIFY(deal(g));QCOMPARE(g.snapshot()["players"].toArray()[0].toObject()["hands"].toArray()[0].toObject()["totalText"].toString(),QString("6 / 16"));QVERIFY(g.act(0,"hit"));QCOMPARE(g.players()[0].hands[0].cards.size(),size_t(3));QCOMPARE(blackjackValue(g.players()[0].hands[0].cards),16);QVERIFY(!g.legalActions().contains("double"));QVERIFY(g.act(0,"stand"));QCOMPARE(g.players()[0].chips,980);
    }
    void softTwentyOneRemainsPlayable() {
        // The reported 4, A, 2, 4 must remain the player's decision at 11 / 21.
        for(bool hitAgain:{false,true}){
            Game g(7);g.configure(config(Kind::Blackjack,1));g.rigNextDeck({{4,0},{10,0},{14,1},{10,1},{2,0},{4,1},{8,0}});
            QVERIFY(deal(g));QVERIFY(g.act(0,"hit"));QVERIFY(g.act(0,"hit"));
            QCOMPARE(blackjackTotalText(g.players()[0].hands[0].cards),QString("11 / 21"));QVERIFY(g.active());QVERIFY(g.legalActions().contains("hit"));QVERIFY(g.legalActions().contains("stand"));
            if(hitAgain){QVERIFY(g.act(0,"hit"));QCOMPARE(blackjackTotalText(g.players()[0].hands[0].cards),QString("19"));}
            QVERIFY(g.act(0,"stand"));QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,hitAgain?980:1020);
        }
        Game g(7);g.configure(config(Kind::Blackjack,1));g.rigNextDeck({{10,0},{10,1},{5,0},{10,2},{6,0}});QVERIFY(deal(g));QVERIFY(g.act(0,"hit"));QVERIFY(!g.active());QCOMPARE(g.players()[0].hands[0].result,QString("Won")); // hard 21
    }
    void splitAcesRemainPlayableAndBlackjackPays() {
        for(int nextCard:{8,6}){
            Game g(8);g.configure(config(Kind::Blackjack,1));g.rigNextDeck({{14,0},{10,0},{14,1},{10,1},{4,0},{13,0},{nextCard,2}});
            QVERIFY(deal(g));QVERIFY(g.act(0,"split"));QVERIFY(g.active());QCOMPARE(g.players()[0].hands.size(),size_t(2));QCOMPARE(g.players()[0].chips,960);
            QCOMPARE(blackjackTotalText(g.players()[0].hands[0].cards),QString("5 / 15"));QVERIFY(g.legalActions().contains("hit"));QVERIFY(g.legalActions().contains("double"));
            QCOMPARE(g.players()[0].hands[1].result,QString("Blackjack"));QVERIFY(g.players()[0].hands[1].done);
            QVERIFY(g.act(0,"hit"));QCOMPARE(blackjackTotalText(g.players()[0].hands[0].cards),nextCard==8?QString("13"):QString("11 / 21"));QVERIFY(g.active());
            QVERIFY(g.act(0,"stand"));QVERIFY(!g.active());QCOMPARE(g.players()[0].hands[0].result,nextCard==8?QString("Lost"):QString("Won"));QCOMPARE(g.players()[0].hands[1].result,QString("Blackjack · 3:2"));QCOMPARE(g.players()[0].hands[1].returned,50.0);QCOMPARE(g.players()[0].chips,nextCard==8?1010:1050);
        }
        // An odd split blackjack keeps its half-unit payout.
        Game g(8);g.configure(config(Kind::Blackjack,1));g.rigNextDeck({{14,0},{10,0},{14,1},{10,1},{4,0},{13,0},{6,2}});QVERIFY(g.placeWager(0,{1,0,0,0,0}));QVERIFY(g.deal());QVERIFY(g.act(0,"split"));QVERIFY(g.act(0,"double"));QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,1017);QCOMPARE(g.players()[0].halfUnit,1);QCOMPARE(g.players()[0].hands[0].bet,10);
        // Resplitting aces is allowed, with the same four-hand limit.
        g.configure(config(Kind::Blackjack,1));g.rigNextDeck({{14,0},{10,0},{14,1},{7,0},{14,2},{14,3},{14,0},{14,1},{14,2},{14,3}});QVERIFY(deal(g));
        for(int i=0;i<3;++i)QVERIFY(g.act(0,"split"));QCOMPARE(g.players()[0].hands.size(),size_t(4));QVERIFY(!g.legalActions().contains("split"));
        for(int i=0;i<4;++i){QCOMPARE(g.snapshot()["handIndex"].toInt(),i);QVERIFY(g.legalActions().contains("hit"));QVERIFY(g.act(0,"stand"));}QVERIFY(!g.active());
    }
    void individualHandWagersAndSettlement() {
        Game g(8);g.configure(config(Kind::Blackjack,1));const HandWagers bets{{1,0,0,0,0},{0,1,0,0,0},{0,0,0,1,0},{0,0,0,0,1}};
        QVERIFY(g.placeHandWagers(0,bets));QCOMPARE(g.players()[0].wager,140);QCOMPARE(g.players()[0].chips,860);QCOMPARE(g.snapshot()["players"].toArray()[0].toObject()["wagerHandChips"].toArray(),handWagersJson(bets));
        auto before=g.snapshot();
        for(const HandWagers bad:{HandWagers{},HandWagers{{1,0,0,0,0},{0,0,0,0,0}},HandWagers{{-1,0,0,0,0}},HandWagers{{0,0,0,0,11}},HandWagers(5,{1,0,0,0,0})}){QVERIFY(!g.placeHandWagers(0,bad));QCOMPARE(g.snapshot(),before);}
        g.rigNextDeck({{14,0},{10,0},{8,0},{10,1},{10,2},{13,0},{9,0},{8,1},{6,0},{7,2},{3,0},{2,0},{10,3}});QVERIFY(g.deal());
        QCOMPARE(g.players()[0].hands[0].bet,5);QCOMPARE(g.players()[0].hands[1].bet,10);QCOMPARE(g.players()[0].hands[2].bet,25);QCOMPARE(g.players()[0].hands[3].bet,100);
        QVERIFY(g.act(0,"stand"));QVERIFY(g.act(0,"split"));QCOMPARE(g.players()[0].hands[2].bet,25);QCOMPARE(g.players()[0].hands[3].bet,25);QCOMPARE(g.players()[0].hands[4].bet,100);
        QVERIFY(g.act(0,"double"));QCOMPARE(g.players()[0].hands[2].bet,50);finish(g);QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,942);QCOMPARE(g.players()[0].halfUnit,1);QCOMPARE(g.players()[0].roundNet,-57.5);
        QVERIFY(g.placeHandWagers(0,bets));QVERIFY(g.clearWager(0));QCOMPARE(g.players()[0].chips,942);QCOMPARE(g.players()[0].halfUnit,1);QVERIFY(g.players()[0].wagerHandChips.empty());
    }
    void individualHandChipControls() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;c.players[0].startingBalance=40;w.startConfig(c);w.resize(1886,939);w.show();QTest::qWait(20);
        auto *count=w.findChild<QSpinBox *>("blackjackHandCount");auto *selector=w.findChild<QComboBox *>("blackjackBettingHand");auto *place=w.findChild<QPushButton *>("placeWagerButton");auto *table=w.canvas();QRect geometry=table->geometry();
        auto chip=[&](const char *name,Qt::MouseButton button=Qt::LeftButton){QTest::mouseClick(w.findChild<QToolButton *>(name),button);};
        count->setValue(3);chip("betChipRed");QVERIFY(!place->isEnabled());selector->setCurrentIndex(1);chip("betChipBlue");QVERIFY(!place->isEnabled());selector->setCurrentIndex(2);chip("betChipGreen");QVERIFY(place->isEnabled());QCOMPARE(table->state()["draftAmount"].toInt(),40);
        chip("betChipWhite");QCOMPARE(table->state()["draftAmount"].toInt(),40); // shared bankroll limit
        selector->setCurrentIndex(0);chip("betChipRed",Qt::RightButton);QCOMPARE(table->state()["draftAmount"].toInt(),35);QVERIFY(!place->isEnabled());chip("betChipRed");
        selector->setCurrentIndex(1);w.findChild<QPushButton *>("clearWagerButton")->click();QCOMPARE(table->state()["draftAmount"].toInt(),30);chip("betChipBlue");
        count->setValue(2);QCOMPARE(table->state()["draftAmount"].toInt(),15);count->setValue(3);QVERIFY(!place->isEnabled());selector->setCurrentIndex(2);chip("betChipGreen");QTest::qWait(20);QCOMPARE(table->geometry(),geometry);
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-individual-bets.png"));
        w.engine().rigNextDeck({{10,0},{10,1},{10,2},{10,3},{7,0},{8,0},{9,0},{7,1}});QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,table->rect().center());QCOMPARE(w.engine().players()[0].chips,0);QCOMPARE(w.engine().players()[0].wager,40);
        QTRY_COMPARE_WITH_TIMEOUT(w.engine().handNumber(),1,1500);QCOMPARE(w.engine().players()[0].hands[0].bet,5);QCOMPARE(w.engine().players()[0].hands[1].bet,10);QCOMPARE(w.engine().players()[0].hands[2].bet,25);QCOMPARE(table->geometry(),geometry);
    }
    void multipleWagersAndPayouts() {
        Game g(4);g.configure(config(Kind::Blackjack,1));QVERIFY(!g.placeWager(0,{1,0,0,0,0},0));QVERIFY(!g.placeWager(0,{1,0,0,0,0},5));QVERIFY(!g.placeWager(0,{0,0,0,0,3},4));
        QVERIFY(g.placeWager(0,{1,0,0,0,0},4));QCOMPARE(g.players()[0].chips,980);QCOMPARE(g.players()[0].wager,20);QCOMPARE(g.players()[0].wagerHands,4);QCOMPARE(g.totalChips(),1000.0);
        QVERIFY(g.placeWager(0,{2,0,0,0,0},2));QCOMPARE(g.players()[0].chips,980);QVERIFY(g.clearWager(0));QCOMPARE(g.players()[0].chips,1000);QVERIFY(g.placeWager(0,{1,0,0,0,0},4));
        g.rigNextDeck({{14,0},{10,0},{14,1},{10,1},{10,2},{13,0},{9,0},{12,1},{9,1},{7,2}});QVERIFY(g.deal());QCOMPARE(g.players()[0].hands.size(),size_t(4));QCOMPARE(g.snapshot()["handIndex"].toInt(),1);QVERIFY(g.act(0,"stand"));QCOMPARE(g.snapshot()["handIndex"].toInt(),3);QVERIFY(g.act(0,"stand"));QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,1025);QCOMPARE(g.players()[0].halfUnit,0);QVERIFY(!g.deal());
    }
    void multipleHandsSplitDoubleAndProgression() {
        Game g(8);g.configure(config(Kind::Blackjack,1));QVERIFY(g.placeWager(0,{0,1,0,0,0},2));
        g.rigNextDeck({{8,0},{14,2},{10,0},{8,1},{5,0},{7,3},{3,2},{2,1},{10,2},{13,1}});QVERIFY(g.deal());QVERIFY(g.act(0,"split"));QCOMPARE(g.players()[0].hands.size(),size_t(3));QCOMPARE(g.players()[0].hands[0].origin,0);QCOMPARE(g.players()[0].hands[1].origin,0);QCOMPARE(g.players()[0].hands[2].origin,1);
        QVERIFY(g.act(0,"double"));QCOMPARE(g.players()[0].hands[0].bet,20);QCOMPARE(g.snapshot()["handIndex"].toInt(),1);QVERIFY(g.act(0,"stand"));QCOMPARE(g.snapshot()["handIndex"].toInt(),2);QCOMPARE(blackjackTotalText(g.players()[0].hands[2].cards),QString("6 / 16"));QVERIFY(g.act(0,"hit"));QCOMPARE(blackjackTotalText(g.players()[0].hands[2].cards),QString("16"));QVERIFY(g.act(0,"stand"));QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,1000);
    }
    void splitLimitAppliesToEachOriginalHand() {
        Game g(8);g.configure(config(Kind::Blackjack,1));QVERIFY(g.placeWager(0,{0,1,0,0,0},4));
        g.rigNextDeck({{8,0},{9,0},{10,0},{6,0},{7,0},{8,1},{9,1},{10,1},{6,1},{10,2},{8,2},{8,3},{8,0},{8,1},{8,2},{8,3},{2,0},{3,0}});QVERIFY(g.deal());
        for(int i=0;i<3;++i)QVERIFY(g.act(0,"split"));QCOMPARE(g.players()[0].hands.size(),size_t(7));QVERIFY(!g.legalActions().contains("split"));
        for(int i=0;i<4;++i){QVERIFY(!g.legalActions().contains("split"));QVERIFY(g.act(0,"stand"));}
        QCOMPARE(g.snapshot()["handIndex"].toInt(),4);QVERIFY(g.legalActions().contains("split"));QVERIFY(g.act(0,"split"));QCOMPARE(g.players()[0].hands.size(),size_t(8));finish(g);QVERIFY(!g.active());
    }
    void multipleNetworkHands() {
        Game g(6),unused;auto c=config(Kind::Blackjack,2);c.players[0].control=Control::Human;c.players[1].control=Control::Remote;g.configure(c);Network host(&g),client(&unused);QVERIFY(host.host(0));client.join("127.0.0.1",host.port(),host.roomCode(),"Three hands");QTRY_VERIFY(host.ready());QVERIFY(g.placeWager(0,{0,1,0,0,0}));client.sendWager({0,1,0,0,0},false,3);QTRY_VERIFY(g.betsReady());QCOMPARE(g.players()[1].wager,30);QCOMPARE(g.players()[1].wagerHands,3);
        g.rigNextDeck({{9,0},{10,0},{10,1},{10,2},{10,3},{9,1},{8,0},{8,1},{8,2},{7,0},{2,0}});QVERIFY(g.deal());QVERIFY(g.act(0,"stand"));host.publish();QTRY_COMPARE(client.remoteState()["turn"].toInt(),1);QCOMPARE(client.remoteState()["players"].toArray()[1].toObject()["hands"].toArray().size(),3);
        client.sendAction("double");QTRY_COMPARE(client.remoteState()["handIndex"].toInt(),1);client.sendAction("stand");QTRY_COMPARE(client.remoteState()["handIndex"].toInt(),2);client.sendAction("stand");QTRY_VERIFY(!g.active());QCOMPARE(g.players()[1].chips,1040);
        QTRY_COMPARE(client.remoteState()["payoutSound"].toInt(),1);QCOMPARE(client.remoteState()["betSound"].toInt(),3);QCOMPARE(client.remoteState()["soundSession"],g.snapshot()["soundSession"]);QCOMPARE(client.remoteState()["soundEvents"],g.snapshot()["soundEvents"]);QCOMPARE(client.remoteState()["soundSequence"],g.snapshot()["soundSequence"]);
        auto remote=client.remoteState()["players"].toArray()[1].toObject();QCOMPARE(remote["roundNet"].toDouble(),40.0);QCOMPARE(remote["hands"].toArray()[0].toObject()["result"].toString(),QString("Won"));
    }
    void networkIndividualBetsAndSplitAces() {
        Game g(9),unused;auto c=config(Kind::Blackjack,2);c.players[0].control=Control::Human;c.players[1].control=Control::Remote;g.configure(c);Network host(&g),client(&unused);QVERIFY(host.host(0));client.join("127.0.0.1",host.port(),host.roomCode(),"Split aces");QTRY_VERIFY(host.ready());QTRY_VERIFY(client.ready());
        const HandWagers bets{{1,0,0,0,0},{0,1,0,0,0},{0,0,0,1,0}};client.sendHandWagers(bets);QTRY_COMPARE(g.players()[1].wager,40);QTRY_COMPARE(client.remoteState()["players"].toArray()[1].toObject()["wagerHandChips"].toArray(),handWagersJson(bets));
        client.sendHandWagers({{0,0,0,0,11}});QTest::qWait(80);QCOMPARE(g.players()[1].wager,40);client.sendHandWagers({{1,0,0,0,0},{0,0,0,0,0}});QTest::qWait(80);QCOMPARE(g.players()[1].wager,40);
        QVERIFY(g.placeWager(0,{0,1,0,0,0}));g.rigNextDeck({{10,0},{14,0},{9,0},{10,1},{10,2},{8,0},{14,1},{9,1},{6,0},{10,3},{4,0},{13,0},{6,1}});QVERIFY(g.deal());QVERIFY(g.act(0,"stand"));host.publish();QTRY_COMPARE(client.remoteState()["turn"].toInt(),1);
        client.sendAction("split");QTRY_COMPARE(g.players()[1].hands.size(),size_t(4));QTRY_VERIFY(client.remoteState()["actions"].toArray().contains("hit"));QCOMPARE(g.players()[1].hands[0].bet,5);QCOMPARE(g.players()[1].hands[1].bet,5);QCOMPARE(g.players()[1].hands[2].bet,10);QCOMPARE(g.players()[1].hands[3].bet,25);
        client.sendAction("hit");QTRY_COMPARE(g.players()[1].hands[0].cards.size(),size_t(3));QTRY_COMPARE(client.remoteState()["players"].toArray()[1].toObject()["hands"].toArray()[0].toObject()["totalText"].toString(),QString("11 / 21"));QVERIFY(g.active());
        client.sendAction("stand");QTRY_COMPARE(client.remoteState()["handIndex"].toInt(),2);client.sendAction("stand");QTRY_COMPARE(client.remoteState()["handIndex"].toInt(),3);client.sendAction("stand");QTRY_VERIFY(!g.active());QTRY_COMPARE(client.remoteState()["phase"].toString(),QString("finished"));
        QCOMPARE(g.players()[1].chips,977);QCOMPARE(g.players()[1].halfUnit,1);auto results=client.remoteState()["players"].toArray()[1].toObject()["hands"].toArray();QCOMPARE(results[0].toObject()["result"].toString(),QString("Won"));QCOMPARE(results[1].toObject()["result"].toString(),QString("Blackjack · 3:2"));
    }
    void animatedSplitAcesCanHitAndStand() {
        Window w(false,true);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.resize(1886,939);w.show();QTest::qWait(20);QRect geometry=w.canvas()->geometry();
        auto *sound=w.findChild<ChipSounds *>();auto *split=w.findChild<QPushButton *>("splitButton");auto *hit=w.findChild<QPushButton *>("hitButton");auto *stand=w.findChild<QPushButton *>("standButton");
        w.engine().rigNextDeck({{14,0},{10,0},{14,1},{10,1},{4,0},{13,0},{6,0}});QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());
        QTRY_VERIFY_WITH_TIMEOUT(split->isEnabled(),6000);split->click();QCOMPARE(w.engine().players()[0].hands.size(),size_t(2));QVERIFY(w.engine().active());
        auto hands=[&]{return w.canvas()->state()["players"].toArray()[0].toObject()["hands"].toArray();};
        QTRY_COMPARE_WITH_TIMEOUT(hands().size(),2,2000);QVERIFY(sound->isPresenting());QVERIFY(!hit->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!sound->isPresenting(),3000);QVERIFY(hit->isEnabled());QVERIFY(stand->isEnabled());QCOMPARE(hands()[0].toObject()["totalText"].toString(),QString("5 / 15"));QCOMPARE(hands()[1].toObject()["result"].toString(),QString("Blackjack"));QCOMPARE(w.canvas()->geometry(),geometry);
        // Both hands are real table targets; viewing the completed one cannot
        // accidentally send an action to the other, still active hand.
        QTest::qWait(20);auto *table=w.canvas();qreal scale=std::min(table->width()/1280.0,table->height()/820.0);QPointF offset((table->width()-1280*scale)/2,(table->height()-820*scale)/2);
        QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,(offset+QPointF(710,624)*scale).toPoint());QCOMPARE(table->displayedHand(),1);QVERIFY(!hit->isEnabled());
        QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,(offset+QPointF(565,624)*scale).toPoint());QCOMPARE(table->displayedHand(),0);QVERIFY(hit->isEnabled());
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-split-aces.png"));
        hit->click();QTRY_VERIFY_WITH_TIMEOUT(!sound->isPresenting(),2000);QCOMPARE(hands()[0].toObject()["totalText"].toString(),QString("11 / 21"));QVERIFY(hit->isEnabled());QVERIFY(stand->isEnabled());stand->click();
        QTRY_VERIFY_WITH_TIMEOUT(!sound->isPresenting(),2000);QCOMPARE(hands()[0].toObject()["result"].toString(),QString("Won"));QCOMPARE(hands()[1].toObject()["result"].toString(),QString("Blackjack · 3:2"));QCOMPARE(w.engine().players()[0].chips,1025);QCOMPARE(table->geometry(),geometry);
        if(!dir.isEmpty()){QVERIFY(w.grab().save(dir+"/casino-split-results.png"));w.resize(1040,800);QTest::qWait(20);QVERIFY(w.grab().save(dir+"/casino-split-results-small.png"));}
    }
    void tableCardsAndHandControls() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();w.refresh();
        QVERIFY(!w.findChild<QWidget *>("yourCards"));QVERIFY(!w.findChild<QComboBox *>("yourHandSelector"));
        auto *count=w.findChild<QSpinBox *>("blackjackHandCount");auto *split=w.findChild<QPushButton *>("splitButton");auto *dbl=w.findChild<QPushButton *>("doubleButton");QVERIFY(count&&split&&dbl);QVERIFY(split->isVisible());QVERIFY(dbl->isVisible());QVERIFY(!split->isEnabled());QVERIFY(!dbl->isEnabled());
        count->setValue(2);QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QCOMPARE(w.canvas()->state()["draftAmount"].toInt(),10);QVERIFY(!w.findChild<QPushButton *>("placeWagerButton")->isEnabled());w.findChild<QComboBox *>("blackjackBettingHand")->setCurrentIndex(1);QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QCOMPARE(w.canvas()->state()["draftAmount"].toInt(),20);w.findChild<QPushButton *>("placeWagerButton")->click();QCOMPARE(w.engine().players()[0].wagerHands,2);
        w.engine().rigNextDeck({{8,0},{14,2},{10,0},{8,1},{5,0},{7,3},{3,2},{2,1},{10,2},{13,1}});QVERIFY(w.findChild<QPushButton *>("dealButton")->isHidden());QTRY_COMPARE_WITH_TIMEOUT(w.engine().handNumber(),1,1500);QVERIFY(split->isEnabled());QVERIFY(dbl->isEnabled());
        auto beforeNavigation=w.engine().snapshot();tableClick(w.canvas(),{679,495},Qt::RightButton);QCOMPARE(w.engine().snapshot(),beforeNavigation);
        w.canvas()->setDisplayedHand(1);QVERIFY(!dbl->isEnabled());QVERIFY(dbl->toolTip().contains("select Hand 1"));
        w.canvas()->setDisplayedHand(0);split->click();QCOMPARE(w.engine().players()[0].hands.size(),size_t(3));dbl->click();QCOMPARE(w.canvas()->displayedHand(),1);w.findChild<QPushButton *>("standButton")->click();QCOMPARE(w.canvas()->displayedHand(),2);QVERIFY(!split->isEnabled());QVERIFY(dbl->isEnabled());
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty()){QTest::qWait(20);QVERIFY(w.grab().save(dir+"/casino-table-cards.png"));w.resize(1040,800);QTest::qWait(20);QVERIFY(w.grab().save(dir+"/casino-table-cards-small.png"));}
        w.findChild<QPushButton *>("hitButton")->click();QVERIFY(!dbl->isEnabled());QVERIFY(dbl->isVisible());QVERIFY(dbl->toolTip().contains("two cards"));
        w.findChild<QPushButton *>("standButton")->click();QVERIFY(w.findChild<QLabel *>("roundStatus")->text().contains("Lost"));
        w.canvas()->setDisplayedHand(0);QVERIFY(w.findChild<QLabel *>("roundStatus")->text().contains("Won"));
        if(!dir.isEmpty()){QTest::qWait(20);QVERIFY(w.grab().save(dir+"/casino-round-result.png"));}
    }
    void drawCardsRemainSelectableOnTable() {
        for(int count:{0,2,5}){
            Window w(false,false);auto c=config(Kind::Draw,2);c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(deal(w.engine()));QVERIFY(w.engine().act(1,"check"));QVERIFY(w.engine().act(0,"check"));QVERIFY(w.engine().act(1,"draw"));w.refresh();w.show();QTest::qWait(20);
            auto *table=w.canvas();auto &g=w.engine();auto before=g.snapshot();auto cards=g.players()[0].cards;auto geometry=table->geometry();
            tableClick(table,{534,599});QCOMPARE(table->discards(),std::vector<int>{0});QVERIFY(w.findChild<QPushButton *>("drawButton")->text().contains("Draw 1"));tableClick(table,{534,599});QVERIFY(table->discards().empty());
            for(int i=0;i<count;++i)tableClick(table,{534+53.0*i,599});QCOMPARE(int(table->discards().size()),count);QCOMPARE(g.snapshot(),before);
            QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(count==2&&!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-felt-draw-v116.png"));
            tableClick(table,{640,430},Qt::RightButton);tableClick(table,{0,0});QCOMPARE(g.snapshot(),before);
            auto *network=w.findChild<Network *>();network->setPaused(true);w.refresh();tableClick(table,{640,430});QCOMPARE(g.snapshot(),before);network->setPaused(false);w.refresh();
            tableClick(table,{640,430});QCOMPARE(g.stage(),QString("betting"));QCOMPARE(g.current(),1);QVERIFY(table->discards().empty());QCOMPARE(table->geometry(),geometry);
            for(int i=0;i<5;++i)QCOMPARE(g.players()[0].cards[i]==cards[i],i>=count);
            auto after=g.snapshot();int replacements=0;for(auto value:after["soundEvents"].toArray()){auto event=value.toObject();if(event["id"].toInt()>before["soundSequence"].toInt()&&event["cue"]=="deal")++replacements;}QCOMPARE(replacements,count);
            // The other player's turn cannot be confirmed by clicking the felt.
            tableClick(table,{640,430});QCOMPARE(g.snapshot(),after);
        }
    }
    void pokerFeltCallsAndChecks() {
        for(Kind kind:{Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem})for(bool jacks:{false,true}){
            if(kind==Kind::Holdem&&jacks)continue;
            Window w(false,false);auto c=config(kind,2,jacks);c.firstCardSeat=0;c.players[0].control=Control::Human;w.startConfig(c);auto &g=w.engine();g.rigNextDeck(pokerClickDeck(kind));QVERIFY(deal(g));
            if(g.current()==0)QVERIFY(g.act(0,g.legalActions().contains("call")?"call":"check"));QCOMPARE(g.current(),1);QVERIFY(g.act(1,"raise"));QCOMPARE(g.current(),0);
            w.refresh();w.show();QTest::qWait(20);auto *table=w.canvas();QVERIFY(w.findChild<QPushButton *>("callButton")->isEnabled());auto before=g.snapshot();int chips=g.players()[0].chips,due=before["currentBet"].toInt()-g.players()[0].streetBet;
            // Partial selections still need correction; they never silently call.
            QTest::mouseClick(w.findChild<QToolButton *>("betChipWhite"),Qt::LeftButton);tableClick(table,{640,430});QCOMPARE(g.snapshot(),before);QCOMPARE(table->state()["draftAmount"].toInt(),1);w.findChild<QPushButton *>("clearWagerButton")->click();
            tableClick(table,{640,430},Qt::RightButton);tableClick(table,{610,620});tableClick(table,{0,0});QCOMPARE(g.snapshot(),before);
            auto *network=w.findChild<Network *>();network->setPaused(true);w.refresh();tableClick(table,{640,430});QCOMPARE(g.snapshot(),before);network->setPaused(false);w.refresh();
            tableClick(table,{640,430});QCOMPARE(g.players()[0].chips,chips-due);QCOMPARE(g.snapshot()["betSound"].toInt(),before["betSound"].toInt()+1);
            while(g.stage()=="draw")QVERIFY(g.act(g.current(),"draw"));
            if(g.current()!=0)QVERIFY(g.act(g.current(),"check"));QCOMPARE(g.current(),0);w.refresh();QVERIFY(w.findChild<QPushButton *>("checkButton")->isEnabled());before=g.snapshot();chips=g.players()[0].chips;
            tableClick(table,{640,430});QVERIFY(g.snapshot()!=before);QCOMPARE(g.players()[0].chips,chips);QCOMPARE(g.snapshot()["betSound"],before["betSound"]);
        }
    }
    void blackjackFeltHitsStandsAndKeepsChipControls() {
        Window w(false,true);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();QTest::qWait(20);auto &g=w.engine();auto *table=w.canvas();auto *sounds=w.findChild<ChipSounds *>();
        g.rigNextDeck({{8,0},{10,2},{7,0},{7,2},{2,1},{3,0}});auto *blue=w.findChild<QToolButton *>("betChipBlue");QTest::mouseClick(blue,Qt::LeftButton);QCOMPARE(table->state()["draftAmount"].toInt(),10);QTest::mouseClick(blue,Qt::RightButton);QCOMPARE(table->state()["draftAmount"].toInt(),0);
        QTest::mouseClick(blue,Qt::LeftButton);tableClick(table,{640,430},Qt::RightButton);QVERIFY(!g.players()[0].wagerReady);tableClick(table,{640,430});QCOMPARE(g.players()[0].wager,10);
        QTRY_VERIFY(sounds->isPresenting());auto before=g.snapshot();tableClick(table,{640,430});tableClick(table,{640,430},Qt::RightButton);QCOMPARE(g.snapshot(),before);QTRY_VERIFY_WITH_TIMEOUT(!sounds->isPresenting(),6000);
        QCOMPARE(g.players()[0].hands[0].cards.size(),size_t(2));QCOMPARE(blackjackValue(g.players()[0].hands[0].cards),15);before=g.snapshot();
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-felt-blackjack-v116.png"));
        tableClick(table,{610,620});tableClick(table,{610,620},Qt::RightButton);tableClick(table,{0,0});QCOMPARE(g.snapshot(),before);
        auto *network=w.findChild<Network *>();network->setPaused(true);w.refresh();tableClick(table,{640,430});tableClick(table,{640,430},Qt::RightButton);QCOMPARE(g.snapshot(),before);network->setPaused(false);w.refresh();
        tableClick(table,{640,430});QCOMPARE(g.players()[0].hands[0].cards.size(),size_t(3));QCOMPARE(blackjackValue(g.players()[0].hands[0].cards),17);QVERIFY(sounds->isPresenting());before=g.snapshot();
        tableClick(table,{640,430});tableClick(table,{640,430},Qt::RightButton);QCOMPARE(g.snapshot(),before);QTRY_VERIFY_WITH_TIMEOUT(!sounds->isPresenting(),1500);
        tableClick(table,{640,430},Qt::RightButton);QVERIFY(!g.active());QCOMPARE(g.players()[0].hands[0].result,QString("Tied"));QCOMPARE(g.players()[0].chips,1000);
    }
    void networkFeltConfirmsPokerAndBlackjack() {
        for(Kind kind:{Kind::Blackjack,Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem}){
            Game g(61);auto c=config(kind,2);c.firstCardSeat=0;c.players[0].control=Control::Remote;c.players[1].control=Control::Human;g.configure(c);Network host(&g);QVERIFY(host.host(0));
            Window w(false,false);auto *client=w.findChild<Network *>();client->join("127.0.0.1",host.port(),host.roomCode(),"Remote");QTRY_VERIFY(client->ready());w.show();QTest::qWait(20);auto *table=w.canvas();
            if(kind==Kind::Blackjack){
                g.rigNextDeck({{8,0},{10,1},{10,3},{7,0},{6,1},{7,2},{2,0}});QVERIFY(deal(g));host.publish();QTRY_VERIFY(w.findChild<QPushButton *>("hitButton")->isEnabled());
                tableClick(table,{640,430});QTRY_COMPARE(g.players()[0].hands[0].cards.size(),size_t(3));QTRY_COMPARE(table->state()["players"].toArray()[0].toObject()["hands"].toArray()[0].toObject()["cards"].toArray().size(),3);
                tableClick(table,{640,430},Qt::RightButton);QTRY_COMPARE(g.current(),1);QTRY_COMPARE(table->state()["turn"].toInt(),1);auto before=g.snapshot();tableClick(table,{640,430});tableClick(table,{640,430},Qt::RightButton);QTest::qWait(50);QCOMPARE(g.snapshot(),before);continue;
            }
            g.rigNextDeck(pokerClickDeck(kind));QVERIFY(deal(g));if(g.current()==0)QVERIFY(g.act(0,g.legalActions().contains("call")?"call":"check"));QVERIFY(g.act(1,"raise"));host.publish();QTRY_VERIFY(w.findChild<QPushButton *>("callButton")->isEnabled());
            int chips=g.players()[0].chips,due=g.snapshot()["currentBet"].toInt()-g.players()[0].streetBet;host.setInputBlocked(true);QTRY_VERIFY(!w.findChild<QPushButton *>("callButton")->isEnabled());tableClick(table,{640,430});QTest::qWait(30);QCOMPARE(g.players()[0].chips,chips);host.setInputBlocked(false);QTRY_VERIFY(w.findChild<QPushButton *>("callButton")->isEnabled());
            tableClick(table,{640,430});QTRY_COMPARE(g.players()[0].chips,chips-due);
            if(kind==Kind::Draw){
                QTRY_VERIFY(w.findChild<QPushButton *>("drawButton")->isEnabled());auto cards=g.players()[0].cards;tableClick(table,{534,599});tableClick(table,{640,599});QCOMPARE(table->discards(),(std::vector<int>{0,2}));tableClick(table,{640,430});QTRY_COMPARE(g.current(),1);QVERIFY(g.players()[0].cards[0]!=cards[0]);QVERIFY(g.players()[0].cards[2]!=cards[2]);for(int i:{1,3,4})QCOMPARE(g.players()[0].cards[i],cards[i]);QVERIFY(g.act(1,"draw"));
            }
            if(g.current()!=0)QVERIFY(g.act(g.current(),"check"));host.publish();QTRY_VERIFY(w.findChild<QPushButton *>("checkButton")->isEnabled());auto before=g.snapshot();tableClick(table,{640,430});QTRY_VERIFY(g.snapshot()!=before);QCOMPARE(g.snapshot()["betSound"],before["betSound"]);
        }
    }
    void numberedPipsAndCardSheet() {
        for(int rank=2;rank<=10;++rank){auto positions=cardPips(rank);QCOMPARE(positions.size(),rank);QSet<QString> unique;for(auto pos:positions){QVERIFY(pos.x()>0&&pos.x()<1&&pos.y()>0&&pos.y()<1);unique.insert(QString::number(pos.x())+"/"+QString::number(pos.y()));}QCOMPARE(unique.size(),rank);}
        QCOMPARE(cardPips(14).size(),1);
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty()){
            QPixmap sheet(1060,400);sheet.fill(QColor("#15282b"));QPainter p(&sheet);for(int i=0;i<13;++i){int rank=i==12?14:i+2;paintCard(p,QRectF(15+(i%7)*148,15+(i/7)*190,126,175),{{"rank",rank},{"suit",i%4}},0,0);}p.end();QVERIFY(sheet.save(dir+"/casino-card-pips.png"));
            QPixmap faces(1200,1000);faces.fill(QColor("#15282b"));QPainter f(&faces);for(int i=0;i<20;++i){int x=(i%5)*240,y=(i/5)*250;f.setPen(QColor("#e5e9dc"));f.drawText(QRectF(x,y+4,240,28),Qt::AlignCenter,themes()[i].name);paintCard(f,QRectF(x+22,y+40,96,134),{{"rank",13},{"suit",2}},i,i);paintCard(f,QRectF(x+142,y+44,52,74),{{"rank",10},{"suit",3}},i,i);paintCard(f,QRectF(x+142,y+126,52,74),{{"rank",2},{"suit",1}},i,i);f.drawText(QRectF(x,y+212,240,24),Qt::AlignCenter,"Preview + table sizes");}f.end();QVERIFY(faces.save(dir+"/casino-card-faces.png"));
        }
    }
    void blackjackResultsAndImmediateBust() {
        Game g(15);auto c=config(Kind::Blackjack,2);g.configure(c);g.rigNextDeck({{10,0},{10,1},{10,2},{9,0},{8,1},{7,2},{5,0}});QVERIFY(deal(g));QVERIFY(g.act(0,"hit"));QVERIFY(g.active());QCOMPARE(g.players()[0].hands[0].result,QString("Bust"));QVERIFY(g.players()[0].status.contains("Bust"));QVERIFY(g.act(1,"stand"));QCOMPARE(g.players()[1].outcome,QString("Won"));QCOMPARE(g.players()[0].roundNet,-20.0);QCOMPARE(g.players()[1].roundNet,20.0);
        c=config(Kind::Blackjack,1);g.configure(c);g.rigNextDeck({{10,0},{10,1},{8,0},{8,1}});QVERIFY(deal(g));QVERIFY(g.act(0,"stand"));QCOMPARE(g.players()[0].hands[0].result,QString("Tied"));QCOMPARE(g.players()[0].roundNet,0.0);QCOMPARE(g.players()[0].hands[0].returned,20.0);
        g.configure(c);g.rigNextDeck({{10,0},{10,1},{7,0},{8,1}});QVERIFY(deal(g));finish(g);QCOMPARE(g.players()[0].outcome,QString("Lost"));
        g.configure(c);g.rigNextDeck({{14,0},{10,1},{13,0},{8,1}});QVERIFY(deal(g));QVERIFY(g.players()[0].outcome.contains("Blackjack"));QCOMPARE(g.players()[0].roundNet,30.0);
    }
    void chipAudioEventsMuteAndReconnect() {
        Game g(5);g.configure(config(Kind::Blackjack,1));ChipSounds sounds;sounds.setVolume(0);QSignalSpy queued(&sounds,&ChipSounds::cueQueued);sounds.observe(g.snapshot());
        QVERIFY(g.placeWager(0,{0,1,0,0,0}));sounds.observe(g.snapshot());QCOMPARE(queued.size(),1);QCOMPARE(queued[0][0].toString(),QString("bet"));sounds.observe(g.snapshot());QCOMPARE(queued.size(),1);
        g.rigNextDeck({{14,0},{10,1},{13,0},{8,1}});QVERIFY(g.deal());sounds.observe(g.snapshot());QCOMPARE(queued.size(),9);QCOMPARE(queued.last()[0].toString(),QString("payout"));
        auto offline=g.snapshot();offline["connected"]=false;sounds.observe(offline);QVERIFY(!sounds.isBusy());sounds.observe(g.snapshot());QCOMPARE(queued.size(),9);
        sounds.setEnabled(false);QVERIFY(g.placeWager(0,{0,1,0,0,0}));sounds.observe(g.snapshot());QCOMPARE(queued.size(),9);sounds.setEnabled(true);sounds.observe(g.snapshot());QCOMPARE(queued.size(),9);
        QVERIFY(g.clearWager(0));sounds.observe(g.snapshot());QCOMPARE(queued.size(),9);QVERIFY(g.placeWager(0,{0,2,0,0,0}));sounds.observe(g.snapshot());QCOMPARE(queued.size(),10);
        g.configure(config(Kind::Blackjack,1));QVERIFY(g.placeWager(0,{0,1,0,0,0}));sounds.observe(g.snapshot());QCOMPARE(queued.size(),10);QVERIFY(!sounds.isBusy());
        QVERIFY(sounds.samplesReady());
    }
    void simultaneousBetPayoutAndLossAudio() {
        Game g(5);g.configure(config(Kind::Blackjack,1));ChipSounds sounds;sounds.setVolume(0);QSignalSpy played(&sounds,&ChipSounds::cueRequested),queued(&sounds,&ChipSounds::cueQueued);sounds.observe(g.snapshot());
        g.placeWager(0,{0,1,0,0,0});g.rigNextDeck({{14,0},{10,1},{13,0},{8,1}});g.deal();sounds.observe(g.snapshot());QCOMPARE(played.size(),1);
        const QStringList expected{"bet","shuffle","deck-tap","deal","deal","deal","deal","collect","payout"};
        QCOMPARE(queued.size(),expected.size());for(int i=0;i<expected.size();++i)QCOMPARE(queued[i][0].toString(),expected[i]);
        QTRY_COMPARE_WITH_TIMEOUT(played.size(),expected.size(),12000);for(int i=0;i<expected.size();++i)QCOMPARE(played[i][0].toString(),expected[i]);
        sounds.reset();played.clear();queued.clear();g.configure(config(Kind::Blackjack,1));sounds.observe(g.snapshot());g.placeWager(0,{0,1,0,0,0});sounds.observe(g.snapshot());g.rigNextDeck({{10,0},{10,1},{7,0},{8,1}});g.deal();finish(g);sounds.observe(g.snapshot());
        QCOMPARE(queued.size(),8);QCOMPARE(queued.last()[0].toString(),QString("collect"));for(const auto &cue:queued)QVERIFY(cue[0].toString()!="payout");
        // A mute cancels every pending cue, and unmuting does not replay them.
        sounds.setEnabled(false);QVERIFY(!sounds.isBusy());int count=played.size();QTest::qWait(250);QCOMPARE(played.size(),count);sounds.setEnabled(true);sounds.observe(g.snapshot());QCOMPARE(played.size(),count);
    }
    void pokerAudioEventsAndHiddenBurns() {
        Game g(8);g.configure(config(Kind::Holdem,2));ChipSounds sounds;sounds.setVolume(0);QSignalSpy queued(&sounds,&ChipSounds::cueQueued);sounds.observe(g.snapshot());
        QVERIFY(g.deal());sounds.observe(g.snapshot());QCOMPARE(queued.size(),8); // shuffle, tap, four hole cards, two blinds
        auto sequence=g.snapshot()["soundSequence"].toInt();QVERIFY(!g.act(g.current(),"raise",{},1));QCOMPARE(g.snapshot()["soundSequence"].toInt(),sequence);
        QVERIFY(g.act(g.current(),"allin"));sounds.observe(g.snapshot());QCOMPARE(queued.last()[0].toString(),QString("allin"));
        QVERIFY(g.act(g.current(),"call"));sounds.observe(g.snapshot());QVERIFY(!g.active());
        QStringList names;for(const auto &cue:queued)names<<cue[0].toString();
        QCOMPARE(names.count("allin"),2);QCOMPARE(names.count("deal"),9); // four private + five community; no three burn cues
        QCOMPARE(names.count("collect"),1);QCOMPARE(names[names.size()-2],QString("collect"));QCOMPARE(names.last(),QString("payout"));
        // Per-card animation payloads retain the same private-card boundary as the table.
        for(int viewer=0;viewer<2;++viewer)for(const auto &value:g.snapshot(viewer)["soundEvents"].toArray()){
            auto event=value.toObject();QVERIFY(!event.contains("privateSeat"));
            if(event["cue"]=="deal"&&event["target"]=="player"){
                auto card=event["card"].toObject();if(event["seat"].toInt()==viewer)QVERIFY(card.contains("rank"));else{QVERIFY(card["hidden"].toBool());QVERIFY(!card.contains("rank"));}
            }
        }
        sounds.observe(g.snapshot());QCOMPARE(queued.size(),names.size());
    }
    void drawStudAndBlackjackActionAudio() {
        auto cues=[](const Game &g,int after){QStringList result;for(const auto &value:g.snapshot()["soundEvents"].toArray()){auto event=value.toObject();if(event["id"].toInt()>after)result<<event["cue"].toString();}return result;};
        auto sequence=[](const Game &g){return g.snapshot()["soundSequence"].toInt();};
        Game g(3);g.configure(config(Kind::Blackjack,1));g.rigNextDeck({{8,0},{10,0},{8,1},{7,1},{2,0},{3,0},{2,1}});QVERIFY(deal(g));int before=sequence(g);
        QVERIFY(g.act(0,"split"));QCOMPARE(cues(g,before),QStringList({"bet","split","deal","deal"}));before=sequence(g);QVERIFY(g.act(0,"double"));QCOMPARE(cues(g,before),QStringList({"bet","deal"}));
        for(Kind kind:{Kind::Draw,Kind::Stud5,Kind::Stud7}){
            g.configure(config(kind,2));QVERIFY(deal(g));
            const int initial=kind==Kind::Draw?10:kind==Kind::Stud7?6:4;QCOMPARE(cues(g,0).count("deal"),initial);
            if(kind==Kind::Draw){
                while(g.stage()!="draw"){auto legal=g.legalActions();QVERIFY(g.act(g.current(),legal.contains("check")?"check":"call"));}
                before=sequence(g);QVERIFY(!g.act(g.current(),"draw",{0,0}));QCOMPARE(sequence(g),before);
                QVERIFY(g.act(g.current(),"draw",{0,2}));QCOMPARE(cues(g,before),QStringList({"deal","deal"}));
            }else{
                finish(g);QCOMPARE(cues(g,0).count("deal"),kind==Kind::Stud7?14:10);QCOMPARE(cues(g,0).count("shuffle"),1);QCOMPARE(cues(g,0).count("collect"),1);QCOMPARE(cues(g,0).last(),QString("payout"));
            }
        }
    }
    void soundHistoryIsBoundedAndLegacyCompatible() {
        Game g(2);g.configure(config(Kind::Blackjack,1));for(int i=0;i<300;++i){QVERIFY(g.placeWager(0,{0,1,0,0,0}));QVERIFY(g.clearWager(0));}
        QCOMPARE(g.snapshot()["soundEvents"].toArray().size(),256);QCOMPARE(g.snapshot()["soundSequence"].toInt(),300);
        ChipSounds sounds;sounds.setVolume(0);QSignalSpy played(&sounds,&ChipSounds::cueQueued);sounds.observe(g.snapshot());QCOMPARE(played.size(),0);
        QVERIFY(g.placeWager(0,{0,1,0,0,0}));sounds.observe(g.snapshot());QCOMPARE(played.size(),1);
        QJsonObject legacy{{"soundSession","old-host"},{"betSound",0},{"payoutSound",0}};sounds.observe(legacy);legacy["betSound"]=1;legacy["payoutSound"]=1;sounds.observe(legacy);QCOMPARE(played.size(),3);QCOMPARE(played.last()[0].toString(),QString("payout"));
    }
    void soundSettingPersists() {
        Window w(false,false);auto *toggle=w.findChild<QAction *>("chipSoundsAction");QVERIFY(toggle);QVERIFY(!toggle->isChecked());toggle->setChecked(true);QCOMPARE(QSettings().value("sound/enabled").toBool(),true);Window other(false,false);QVERIFY(other.findChild<QAction *>("chipSoundsAction")->isChecked());toggle->setChecked(false);Window muted(false,false);QVERIFY(!muted.findChild<QAction *>("chipSoundsAction")->isChecked());
    }
    void chipSelectionAndConfirmationSounds() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();QTest::qWait(10);
        auto *sounds=w.findChild<ChipSounds *>();QVERIFY(sounds);sounds->setVolume(0);w.findChild<QAction *>("chipSoundsAction")->setChecked(true);QSignalSpy played(sounds,&ChipSounds::cueRequested);
        auto *blue=w.findChild<QToolButton *>("betChipBlue");QVERIFY(blue);
        QTest::mouseClick(blue,Qt::RightButton);QCOMPARE(played.size(),0);
        QTest::mouseClick(blue,Qt::LeftButton);QCOMPARE(played.size(),1);
        QTest::mouseClick(blue,Qt::LeftButton);QCOMPARE(played.size(),2);
        QTest::mouseClick(blue,Qt::RightButton);QCOMPARE(played.size(),3);
        w.findChild<QPushButton *>("placeWagerButton")->click();QCOMPARE(played.size(),4);QCOMPARE(w.engine().players()[0].wager,10);
        w.refresh();QCOMPARE(played.size(),4);
        w.findChild<QPushButton *>("clearWagerButton")->click();w.findChild<QAction *>("chipSoundsAction")->setChecked(false);
        QTest::mouseClick(blue,Qt::LeftButton);w.findChild<QPushButton *>("placeWagerButton")->click();QCOMPARE(played.size(),4);
    }
    void audioPlaybackSmoke() {
        if(!qEnvironmentVariableIsSet("CASINO_AUDIO_SMOKE"))QSKIP("Audio output capture is a separate opt-in check.");
        // Use the production audio application identity, with QSettings still isolated
        // in the test directory. Exercise real chip and Place bet buttons.
        const QString previousName=QCoreApplication::applicationName();
        auto restore=qScopeGuard([&]{QCoreApplication::setApplicationName(previousName);});
        QCoreApplication::setApplicationName("VelvetCasino");QSettings().setValue("sound/enabled",true);
        Window w(false,true);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();QTest::qWait(10);
        auto *sounds=w.findChild<ChipSounds *>();QVERIFY(sounds);QVERIFY(sounds->samplesReady());
        const QByteArray deviceId=qgetenv("CASINO_AUDIO_DEVICE");
        if(!deviceId.isEmpty()){
            QAudioDevice device;
            auto findDevice=[&]{for(const auto &candidate:QMediaDevices::audioOutputs())if(candidate.id()==deviceId){device=candidate;return true;}return false;};
            QTRY_VERIFY_WITH_TIMEOUT(findDevice(),3000);sounds->setAudioDevice(device);
        }
        QSignalSpy played(sounds,&ChipSounds::cueRequested);
        QVERIFY(sounds->introReady());
        QTimer::singleShot(700,&w,[&]{auto *dialog=w.findChild<QDialog *>("setupDialog");if(dialog)dialog->reject();});
        w.setup();auto *intro=sounds->findChild<QAudioSink *>("introAudioOutput");QVERIFY(intro);QCOMPARE(intro->state(),QAudio::StoppedState);QCOMPARE(played.first()[0].toString(),QString("intro"));played.clear();
        QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QCOMPARE(played.size(),1);
        auto *bet=sounds->findChild<QAudioSink *>("betAudioOutput");QVERIFY(bet);QVERIFY(bet->volume()>0);QCOMPARE(bet->error(),QAudio::NoError);QTest::qWait(400);
        w.engine().rigNextDeck({{14,0},{10,1},{13,0},{8,1}});
        w.findChild<QPushButton *>("placeWagerButton")->click();QCOMPARE(played.size(),2);QCOMPARE(bet->error(),QAudio::NoError);QTest::qWait(400);
        // Wait for the confirmed bet, then verify actual shuffle -> tap -> cards -> payout output.
        QTRY_VERIFY_WITH_TIMEOUT(played.size()>2,3000);
        QCOMPARE(played[2][0].toString(),QString("shuffle"));QTRY_VERIFY_WITH_TIMEOUT(!sounds->isBusy(),12000);
        QStringList names;for(const auto &cue:played)names<<cue[0].toString();
        QCOMPARE(names,QStringList({"bet","bet","shuffle","deck-tap","deal","deal","deal","deal","collect","payout"}));
        for(const QString &cue:QStringList{"bet","shuffle","deck-tap","deal","collect","payout"}){
            auto *output=sounds->findChild<QAudioSink *>(cue+"AudioOutput");QVERIFY(output);QVERIFY(output->volume()>0);QCOMPARE(output->error(),QAudio::NoError);
        }
        c=config(Kind::Holdem,2);for(auto &p:c.players)p.control=Control::Human;w.startConfig(c);w.findChild<QPushButton *>("dealButton")->click();QTRY_VERIFY_WITH_TIMEOUT(!sounds->isBusy(),12000);
        QVERIFY(w.engine().act(w.engine().current(),"allin"));w.refresh();QCOMPARE(played.last()[0].toString(),QString("allin"));
        auto *allin=sounds->findChild<QAudioSink *>("allinAudioOutput");QVERIFY(allin);QCOMPARE(allin->error(),QAudio::NoError);QTRY_VERIFY_WITH_TIMEOUT(!sounds->isBusy(),3000);
        // Exercise the poker UI's own ante, selected-chip raise, all-in and
        // payout path against the same bundled recordings as blackjack.
        c=config(Kind::Draw,2);c.firstCardSeat=0;c.players[0].control=Control::Human;w.startConfig(c);played.clear();
        // A known human winner exercises the payout; request cash-out below
        // to keep automatic house loans and next-hand antes out of this capture.
        w.engine().rigNextDeck({{10,0},{2,1},{11,0},{4,1},{12,0},{6,2},{13,0},{8,3},{14,0},{9,1}});
        QList<int> shownCounts;QList<qint64> dealTimes;QElapsedTimer dealClock;dealClock.start();
        auto observedCards=connect(sounds,&ChipSounds::cueRequested,&w,[&](const QString &cue){if(cue=="deal"){int count=0;for(auto person:w.canvas()->state()["players"].toArray())count+=person.toObject()["cards"].toArray().size();shownCounts<<count;dealTimes<<dealClock.elapsed();}});
        QVERIFY(w.engine().postAnte(1));w.refresh();QTest::mouseClick(w.findChild<QToolButton *>("betChipRed"),Qt::LeftButton);QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());
        QTRY_COMPARE_WITH_TIMEOUT(w.engine().handNumber(),1,3000);QTRY_VERIFY_WITH_TIMEOUT(!sounds->isBusy(),15000);QCOMPARE(w.engine().current(),0);
        disconnect(observedCards);QCOMPARE(shownCounts,QList<int>({1,2,3,4,5,6,7,8,9,10}));for(int i=1;i<dealTimes.size();++i)QVERIFY(dealTimes[i]-dealTimes[i-1]>=300);
        QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());QCOMPARE(w.engine().players()[0].streetBet,20);
        QVERIFY(w.engine().act(1,"call"));QVERIFY(w.engine().act(0,"draw"));QVERIFY(w.engine().act(1,"draw"));w.refresh();QTRY_VERIFY_WITH_TIMEOUT(!sounds->isBusy(),3000);
        w.findChild<QPushButton *>("allinButton")->click();QCOMPARE(w.engine().players()[0].chips,0);w.engine().requestCashout();QVERIFY(w.engine().act(1,"call"));QVERIFY(w.engine().sessionEnded());w.refresh();QTRY_VERIFY_WITH_TIMEOUT(!sounds->isBusy(),8000);
        QStringList pokerCues;for(const auto &cue:played)pokerCues<<cue[0].toString();QCOMPARE(pokerCues.count("shuffle"),1);QCOMPARE(pokerCues.count("deck-tap"),1);QCOMPARE(pokerCues.count("deal"),10);QVERIFY(pokerCues.count("bet")>=5);QCOMPARE(pokerCues.count("allin"),2);QCOMPARE(pokerCues.count("collect"),1);QCOMPARE(pokerCues[pokerCues.size()-2],QString("collect"));QCOMPARE(pokerCues.last(),QString("payout"));
        sounds->setEnabled(false);for(auto *output:sounds->findChildren<QAudioSink *>())QCOMPARE(output->state(),QAudio::StoppedState);
    }

    void blackjackShoeDeckSettingsAndReshuffle() {
        auto shuffleCount=[](const QJsonObject &state,int after){int n=0;for(const auto &value:state["soundEvents"].toArray()){auto e=value.toObject();if(e["id"].toInt()>after&&e["cue"]=="shuffle")++n;}return n;};
        Game g(42);auto c=config(Kind::Blackjack,1);c.blackjackDecks=1;g.configure(c);QVERIFY(deal(g));QCOMPARE(shuffleCount(g.snapshot(),0),1);finish(g);
        int remaining=g.snapshot()["cardsRemaining"].toInt(),before=g.snapshot()["soundSequence"].toInt();QVERIFY(deal(g));QCOMPARE(shuffleCount(g.snapshot(),before),0);QVERIFY(g.snapshot()["cardsRemaining"].toInt()<remaining);finish(g);
        bool reshuffled=false;for(int round=0;round<25&&!reshuffled;++round){before=g.snapshot()["soundSequence"].toInt();QVERIFY(deal(g));reshuffled=shuffleCount(g.snapshot(),before)>0;finish(g);}QVERIFY(reshuffled);
        QVERIFY(g.placeWager(0,{0,2,0,0,0}));double total=g.totalChips();QVERIFY(g.setBlackjackDecks(3));QCOMPARE(g.totalChips(),total);QVERIFY(!g.players()[0].wagerReady);QCOMPARE(g.snapshot()["cardsRemaining"].toInt(),0);
        before=g.snapshot()["soundSequence"].toInt();QVERIFY(deal(g));QCOMPARE(shuffleCount(g.snapshot(),before),1);QVERIFY(g.snapshot()["cardsRemaining"].toInt()<=156);QVERIFY(!g.setBlackjackDecks(0));QVERIFY(!g.setBlackjackDecks(9));
        for(Kind kind:{Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem}){
            g.configure(config(kind,2));QVERIFY(deal(g));QCOMPARE(g.snapshot()["deckCount"].toInt(),1);finish(g);before=g.snapshot()["soundSequence"].toInt();QVERIFY(deal(g));QCOMPARE(shuffleCount(g.snapshot(),before),1);
        }
        // Small shoes reserve enough cards for every active hand; no duplicates or exhaustion.
        for(int decks=1;decks<=8;++decks){
            c=config(Kind::Blackjack,5);c.blackjackDecks=decks;c.bankroll=1000000;g.configure(c);
            for(int round=0;round<20;++round){
                for(int i=0;i<5;++i)QVERIFY(g.placeWager(i,{0,1,0,0,0},g.maxStartingHands(i)));
                QVERIFY(g.deal());int guard=0;while(g.active()&&++guard<2000)g.computerStep();QVERIFY(!g.active());QVERIFY(g.snapshot()["cardsRemaining"].toInt()>0);
            }
        }
    }
    void deckSettingPersistsAndRefunds() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(w.engine().placeWager(0,{0,2,0,0,0}));
        QTimer::singleShot(0,&w,[&]{auto *dialog=w.findChild<QDialog *>("deckSettingsDialog");if(dialog){dialog->findChild<QSpinBox *>("blackjackDeckCount")->setValue(2);dialog->accept();}});w.deckSettings();
        QCOMPARE(w.engine().config().blackjackDecks,2);QCOMPARE(w.engine().players()[0].chips,1000);QVERIFY(!w.engine().players()[0].wagerReady);Window reopened(false,false);QCOMPARE(reopened.engine().config().blackjackDecks,2);
        QTimer::singleShot(0,&w,[&]{auto *dialog=w.findChild<QDialog *>("deckSettingsDialog");if(dialog){dialog->findChild<QSpinBox *>("blackjackDeckCount")->setValue(6);dialog->reject();}});w.deckSettings();QCOMPARE(w.engine().config().blackjackDecks,2);
        w.startConfig(config(Kind::Holdem,2));QVERIFY(!w.findChild<QAction *>("blackjackDecksAction")->isEnabled());
    }
    void tableClickConfirmsBet() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();QTest::qWait(20);
        auto *table=w.canvas();QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,table->rect().center());QVERIFY(!w.engine().players()[0].wagerReady);
        QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QTest::mouseClick(table,Qt::RightButton,Qt::NoModifier,table->rect().center());QVERIFY(!w.engine().players()[0].wagerReady);
        QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,table->rect().center());QCOMPARE(w.engine().players()[0].wager,10);QCOMPARE(w.engine().players()[0].chips,990);
        QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,table->rect().center());QCOMPARE(w.engine().players()[0].wager,10);
        QTRY_COMPARE_WITH_TIMEOUT(w.engine().handNumber(),1,1500);
        c=config(Kind::Holdem,2);c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(w.engine().deal());w.refresh();QTest::qWait(20);int before=w.engine().players()[0].streetBet;
        QTest::mouseClick(w.findChild<QToolButton *>("betChipGreen"),Qt::LeftButton);QTest::mouseClick(w.findChild<QToolButton *>("betChipRed"),Qt::LeftButton);QVERIFY(w.findChild<QPushButton *>("raiseButton")->isEnabled());QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,table->rect().center());QVERIFY(w.engine().players()[0].streetBet>before);
    }
    void animatedDealAndMuteKeepCardsInOrder() {
        Window w(false,true);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();
        auto *sound=w.findChild<ChipSounds *>();QVERIFY(sound);sound->setVolume(0);sound->setEnabled(true);QSignalSpy played(sound,&ChipSounds::cueRequested);
        QVector<int> shuffledShoe;connect(sound,&ChipSounds::eventStarted,&w,[&](const QJsonObject &event){if(event["cue"]=="shuffle")shuffledShoe.append(w.canvas()->state()["cardsRemaining"].toInt());},Qt::QueuedConnection);
        auto cardCount=[&]{auto state=w.canvas()->state();int count=state["dealer"].toArray().size();for(auto value:state["players"].toArray())for(auto hand:value.toObject()["hands"].toArray())count+=hand.toObject()["cards"].toArray().size();return count;};
        w.engine().rigNextDeck({{8,0},{10,1},{7,0},{6,1},{2,0},{5,2}});
        QVERIFY(w.engine().placeWager(0,{0,1,0,0,0}));w.refresh();QTRY_VERIFY_WITH_TIMEOUT(sound->isPresenting(),2000);QCOMPARE(cardCount(),0);QVERIFY(!w.findChild<QPushButton *>("hitButton")->isEnabled());
        QTRY_COMPARE_WITH_TIMEOUT(cardCount(),1,6000);QCOMPARE(played.last()[0].toString(),QString("deal"));QCOMPARE(shuffledShoe,QVector<int>{416});QCOMPARE(w.canvas()->state()["cardsRemaining"].toInt(),415);QTest::qWait(100);QCOMPARE(cardCount(),1);
        QString firstFrame=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!firstFrame.isEmpty())QVERIFY(w.grab().save(firstFrame+"/casino-first-card.png"));
        QVERIFY(w.engine().updateProfile(0,"Live during animation"));w.refresh();QCOMPARE(w.canvas()->state()["players"].toArray()[0].toObject()["name"].toString(),QString("Live during animation"));QCOMPARE(cardCount(),1);
        sound->setEnabled(false);int count=played.size();QTRY_VERIFY_WITH_TIMEOUT(!sound->isPresenting(),4000);QCOMPARE(cardCount(),4);QCOMPARE(w.canvas()->state()["cardsRemaining"].toInt(),412);QCOMPARE(played.size(),count);QVERIFY(w.findChild<QPushButton *>("hitButton")->isEnabled());
        QVERIFY(w.canvas()->state()["dealer"].toArray()[1].toObject()["hidden"].toBool());
        w.findChild<QPushButton *>("hitButton")->click();QCOMPARE(cardCount(),5);QCOMPARE(w.canvas()->state()["cardsRemaining"].toInt(),411);QVERIFY(sound->isPresenting());QVERIFY(!w.findChild<QPushButton *>("hitButton")->isEnabled());QTRY_VERIFY_WITH_TIMEOUT(!sound->isPresenting(),1500);
        QCOMPARE(w.canvas()->state()["players"].toArray()[0].toObject()["hands"].toArray()[0].toObject()["value"].toInt(),17);
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-animated-deal.png"));
    }
    void animatedNetworkDealBlocksEarlyActions() {
        Window hostWindow(false,true);auto c=config(Kind::Blackjack,2);c.players[0].control=Control::Human;c.players[1].control=Control::Remote;hostWindow.startConfig(c,0);Game unused;Network client(&unused);auto *host=hostWindow.findChild<Network *>();QVERIFY(host);client.join("127.0.0.1",host->port(),host->roomCode(),"Remote");QTRY_VERIFY(host->ready());
        hostWindow.engine().rigNextDeck({{14,0},{10,1},{10,2},{13,0},{8,1},{7,2}});
        QVERIFY(hostWindow.engine().placeWager(0,{0,1,0,0,0}));hostWindow.refresh();QTest::qWait(600);QCOMPARE(hostWindow.engine().handNumber(),0);
        client.sendWager({0,1,0,0,0});QTRY_VERIFY(client.remoteState()["inputBlocked"].toBool());QCOMPARE(hostWindow.engine().current(),1);
        client.sendAction("stand");QTest::qWait(100);QVERIFY(hostWindow.engine().active());
        QTRY_VERIFY_WITH_TIMEOUT(!client.remoteState()["inputBlocked"].toBool(),5000);client.sendAction("stand");QTRY_VERIFY(!hostWindow.engine().active());
    }
    void setupIntroStartsAndStops() {
        Window w(false,false);auto *sounds=w.findChild<ChipSounds *>();sounds->setVolume(0);sounds->setEnabled(true);QSignalSpy played(sounds,&ChipSounds::cueRequested);
        if(!sounds->introReady())QSKIP("The optional Pixabay intro is absent from the CC0-only build.");
        QTimer::singleShot(100,&w,[&]{auto *dialog=w.findChild<QDialog *>("setupDialog");if(dialog)dialog->reject();});w.setup();QCOMPARE(played.size(),1);QCOMPARE(played[0][0].toString(),QString("intro"));
        sounds->setEnabled(false);QTimer::singleShot(100,&w,[&]{auto *dialog=w.findChild<QDialog *>("setupDialog");if(dialog)dialog->reject();});w.setup();QCOMPARE(played.size(),1);
        sounds->setEnabled(true);QTimer::singleShot(100,&w,[&]{auto *dialog=w.findChild<QDialog *>("setupDialog");if(dialog)dialog->findChild<QPushButton *>("primary")->click();});w.setup();QCOMPARE(played.size(),2);QVERIFY(!sounds->isBusy());
    }
    void pokerChipsConfirmCallRaiseAllInAndFold() {
        Window w(false,false);auto c=config(Kind::Holdem,2);c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(w.engine().deal());w.refresh();w.show();QTest::qWait(20);
        auto clickChip=[&](const char *name){QTest::mouseClick(w.findChild<QToolButton *>(name),Qt::LeftButton);};
        auto confirm=[&]{QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());};
        clickChip("betChipRed");confirm();QCOMPARE(w.engine().current(),0);QCOMPARE(w.engine().players()[0].streetBet,10); // incomplete call stays a draft
        clickChip("betChipRed");confirm();QCOMPARE(w.engine().current(),1);QCOMPARE(w.engine().players()[0].streetBet,20);
        QVERIFY(w.engine().act(1,"check"));w.refresh();QCOMPARE(w.canvas()->state()["board"].toArray().size(),3); // flop needs no Deal click
        QVERIFY(w.engine().act(1,"check"));w.refresh();clickChip("betChipGreen");confirm();QCOMPARE(w.engine().players()[0].streetBet,25);QCOMPARE(w.engine().current(),1);
        QVERIFY(w.engine().act(1,"call"));w.refresh();QCOMPARE(w.canvas()->state()["board"].toArray().size(),4);
        // Place every remaining chip. The table recognizes an all-in and runs out
        // the board after the opponent calls, without any second confirmation.
        QVERIFY(w.engine().act(1,"check"));w.refresh();
        for(int i=0;i<9;++i)clickChip("betChipYellow");for(int i=0;i<2;++i)clickChip("betChipGreen");clickChip("betChipRed");
        confirm();QCOMPARE(w.engine().players()[0].chips,0);QCOMPARE(w.engine().current(),1);
        QVERIFY(w.engine().act(1,"call"));w.refresh();QVERIFY(!w.engine().active());QCOMPARE(w.canvas()->state()["board"].toArray().size(),5);QCOMPARE(w.engine().totalChips(),2000.0);
        // A folded local player can watch the remaining computer opponents finish.
        c=config(Kind::Holdem,3);c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(w.engine().deal());w.refresh();w.findChild<QCheckBox *>("fastTurns")->setChecked(true);
        w.findChild<QPushButton *>("foldButton")->click();QVERIFY(w.engine().players()[0].folded);QVERIFY(w.engine().current()!=0);
        QTRY_VERIFY_WITH_TIMEOUT(!w.engine().active(),15000);QCOMPARE(w.engine().totalChips(),3000.0);
    }
    void tableGeometryStaysFixedThroughResults() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.resize(1280,940);w.show();QTest::qWait(20);
        const QRect tableRect=w.canvas()->geometry();auto *bar=w.findChild<QWidget *>("wagerBar");
        QCOMPARE(bar->geometry().center().x(),tableRect.center().x());
        w.engine().rigNextDeck({{10,0},{10,1},{7,0},{7,1},{9,0}});QVERIFY(w.engine().placeWager(0,{0,1,0,0,0}));QVERIFY(w.engine().deal());w.refresh();QTest::qWait(20);
        QVERIFY(bar->isHidden());QCOMPARE(w.canvas()->geometry(),tableRect);
        w.findChild<QPushButton *>("hitButton")->click();QTest::qWait(20);QVERIFY(!w.engine().active());QCOMPARE(w.canvas()->geometry(),tableRect);
        auto *status=w.findChild<QLabel *>("roundStatus");status->setText(QString("Bust · Lost · Blackjack · Tied · ").repeated(20));QTest::qWait(20);QCOMPARE(w.canvas()->geometry(),tableRect);
        w.refresh();QTest::qWait(20);QCOMPARE(w.canvas()->geometry(),tableRect);
    }
    void blackjackPayouts() {
        Game g(5);auto c=config(Kind::Blackjack,1);g.configure(c);
        g.rigNextDeck({{14,0},{10,0},{13,1},{7,2}});QVERIFY(deal(g));QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,1030);
        g.configure(c);g.rigNextDeck({{10,0},{14,0},{9,1},{13,2}});deal(g);QCOMPARE(g.players()[0].chips,980);
        g.configure(c);g.rigNextDeck({{14,0},{14,1},{13,1},{13,2}});deal(g);QCOMPARE(g.players()[0].chips,1000);
        g.configure(c);g.rigNextDeck({{10,0},{10,1},{8,0},{8,1}});deal(g);QVERIFY(g.act(0,"stand"));QCOMPARE(g.players()[0].chips,1000);
    }
    void blackjackDoubleSplitSoft17() {
        Game g(5);auto c=config(Kind::Blackjack,1);g.configure(c);
        g.rigNextDeck({{5,0},{10,0},{6,1},{7,2},{13,2}});deal(g);QVERIFY(g.act(0,"double"));QCOMPARE(g.players()[0].chips,1040);QVERIFY(!g.active());
        g.configure(c);g.rigNextDeck({{14,0},{10,0},{14,1},{8,2},{13,2},{12,2}});deal(g);QVERIFY(g.act(0,"split"));QCOMPARE(g.players()[0].hands.size(),size_t(2));QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,1060); // house rule: split blackjack pays 3:2
        g.configure(c);g.rigNextDeck({{10,0},{14,0},{8,1},{6,1}});deal(g);g.act(0,"stand");QCOMPARE(g.snapshot()["dealer"].toArray().size(),2);QCOMPARE(g.players()[0].chips,1020);
    }
    void blackjackSplitTurnProgression() {
        Game g(3);g.configure(config(Kind::Blackjack,2));
        g.rigNextDeck({{8,0},{10,0},{6,0},{8,1},{7,0},{10,1},{3,0},{2,0},{10,2}});deal(g);
        QCOMPARE(g.current(),0);QVERIFY(g.act(0,"split"));QCOMPARE(g.current(),0);g.act(0,"stand");QCOMPARE(g.current(),0);QCOMPARE(g.snapshot()["handIndex"].toInt(),1);g.act(0,"stand");QCOMPARE(g.current(),1);g.act(1,"stand");QVERIFY(!g.active());
    }
    void studLayoutsAndPrivacy() {
        for(auto kind:{Kind::Stud5,Kind::Stud7}) {
            Game g(82);g.configure(config(kind,5));deal(g);int initial=kind==Kind::Stud7?3:2;
            for(int i=0;i<5;++i){auto hidden=g.snapshot(-1)["players"].toArray()[i].toObject()["cards"].toArray();QCOMPARE(hidden.size(),initial);QCOMPARE(int(std::count_if(hidden.begin(),hidden.end(),[](auto c){return c.toObject()["hidden"].toBool();})),initial-1);for(auto c:g.snapshot(i)["players"].toArray()[i].toObject()["cards"].toArray())QVERIFY(!c.toObject()["hidden"].toBool());}
            finish(g);QVERIFY(!g.active());
            for(const auto &s:g.players()){QCOMPARE(s.cards.size(),kind==Kind::Stud7?size_t(7):size_t(5));QCOMPARE(int(std::count(s.up.begin(),s.up.end(),true)),4);QVERIFY(!s.up.front());if(kind==Kind::Stud7){QVERIFY(!s.up[1]);QVERIFY(!s.up.back());}}
        }
    }
    void drawSelectionValidation() {
        Game g(41);g.configure(config(Kind::Draw,5));deal(g);
        while(g.stage()=="betting")g.act(g.current(),"check");QCOMPARE(g.stage(),QString("draw"));
        int seat=g.current();auto before=g.players()[seat].cards;
        QVERIFY(!g.act(seat,"draw",{0,0}));QVERIFY(!g.act(seat,"draw",{5}));QCOMPARE(g.players()[seat].cards,before);
        QVERIFY(g.act(seat,"draw",{1,3}));QCOMPARE(g.players()[seat].cards[0],before[0]);QVERIFY(g.players()[seat].cards[1]!=before[1]);QCOMPARE(g.players()[seat].cards[2],before[2]);
        while(g.stage()=="draw")g.act(g.current(),"draw",{0,1,2,3,4});finish(g);QVERIFY(!g.active());QCOMPARE(g.totalChips(),5000);
    }
    void normalDrawCheckReturnsToCallerBeforeDrawing() {
        Game g(12);auto c=config(Kind::Draw,3);c.firstCardSeat=0;g.configure(c);QVERIFY(deal(g));
        QVERIFY(g.act(0,"check"));QVERIFY(g.act(1,"raise",{},20));QVERIFY(g.act(2,"call"));
        QCOMPARE(g.stage(),QString("betting"));QCOMPARE(g.current(),0);QVERIFY(g.legalActions().contains("call"));QVERIFY(g.legalActions().contains("raise"));QVERIFY(g.legalActions().contains("fold"));QVERIFY(!g.legalActions().contains("draw"));
        QVERIFY(g.act(0,"call"));QCOMPARE(g.stage(),QString("draw"));QCOMPARE(g.current(),0);finish(g);QCOMPARE(g.totalChips(),3000.0);
        g.configure(c);QVERIFY(deal(g));for(int i=0;i<3;++i)QVERIFY(g.act(i,"check"));QCOMPARE(g.stage(),QString("draw"));QCOMPARE(g.current(),0);
    }
    void jacksOpeningAndCarry() {
        Game g(3);g.configure(config(Kind::Draw,2,true));
        // Draw starts left of the button: player 2 has tens; player 1 has jacks.
        g.rigNextDeck({{10,0},{11,0},{10,1},{11,1},{2,0},{3,0},{5,1},{6,1},{8,2},{9,2}});deal(g);QCOMPARE(g.current(),1);QVERIFY(!g.legalActions().contains("raise"));g.act(1,"check");QVERIFY(g.legalActions().contains("raise"));g.act(0,"raise");QVERIFY(g.legalActions().contains("call"));finish(g);QCOMPARE(g.totalChips(),2000);
        g.configure(config(Kind::Draw,2,true));g.rigNextDeck({{10,0},{9,0},{10,1},{9,1},{2,0},{3,0},{5,1},{6,1},{8,2},{12,2}});deal(g);g.act(1,"check");g.act(0,"check");QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,995);QCOMPARE(g.players()[1].chips,995);QCOMPARE(g.potValue(),10);QCOMPARE(g.totalChips(),2000.0);
    }
    void holdemHeadsUpAndTiedBoard() {
        Game g(8);g.configure(config(Kind::Holdem));
        // p1(button)=5,6; p2=2,3; burns 4,7,8; royal board clubs.
        g.rigNextDeck({{2,1},{5,1},{3,2},{6,2},{4,1},{10,0},{11,0},{12,0},{7,1},{13,0},{8,1},{14,0}});deal(g);QCOMPARE(g.current(),0);QCOMPARE(g.players()[0].streetBet,10);QCOMPARE(g.players()[1].streetBet,20);
        g.act(0,"call");QCOMPARE(g.current(),1);g.act(1,"check");QCOMPARE(g.current(),1);QCOMPARE(g.snapshot()["board"].toArray().size(),3);
        finish(g);QCOMPARE(g.players()[0].chips,1000);QCOMPARE(g.players()[1].chips,1000);
        for(const auto &s:g.players()){QCOMPARE(s.outcome,QString("Tied · split pot"));QCOMPARE(s.roundNet,0.0);}
    }
    void noLimitRaisesAndAllIn() {
        Game g(19);g.configure(config(Kind::Holdem,3));g.deal();QVERIFY(!g.act((g.current()+1)%3,"call"));QVERIFY(!g.act(g.current(),"dance"));
        auto before=g.snapshot();QVERIFY(!g.act(g.current(),"raise",{},39));QCOMPARE(g.snapshot(),before);
        QVERIFY(g.act(g.current(),"raise",{},100));QCOMPARE(g.snapshot()["raiseTo"].toInt(),180);
        for(int i=0;i<4;++i){QVERIFY(g.legalActions().contains("raise"));QVERIFY(g.act(g.current(),"raise"));}
        QVERIFY(g.legalActions().contains("allin"));QVERIFY(g.act(g.current(),"allin"));finish(g);QCOMPARE(g.totalChips(),3000.0);
    }
    void shortAllInsAndCumulativeReopening() {
        Game g(19);auto c=config(Kind::Holdem,3);c.players[2].startingBalance=50;g.configure(c);g.deal();
        QVERIFY(g.act(0,"raise",{},40));QVERIFY(g.act(1,"call"));QVERIFY(g.act(2,"allin"));QCOMPARE(g.current(),0);QVERIFY(!g.legalActions().contains("raise"));QVERIFY(!g.legalActions().contains("allin"));finish(g);QCOMPARE(g.totalChips(),2050.0);
        c=config(Kind::Holdem,5);c.players[0].startingBalance=50;c.players[1].startingBalance=60;g.configure(c);g.deal();QCOMPARE(g.current(),3);
        QVERIFY(g.act(3,"raise",{},40));QVERIFY(g.act(4,"call"));QVERIFY(g.act(0,"allin"));QVERIFY(g.act(1,"allin"));QVERIFY(g.act(2,"call"));QCOMPARE(g.current(),3);QVERIFY(g.legalActions().contains("raise"));QVERIFY(g.act(3,"raise",{},80));finish(g);QCOMPARE(g.totalChips(),3110.0);
    }
    void sidePotsPayOnlyEligibleStacks() {
        Game g(12);auto c=config(Kind::Draw,3);c.bankroll=100;g.configure(c);
        g.rigNextDeck({{13,0},{12,0},{14,0},{13,1},{12,1},{14,1},{2,0},{3,0},{4,0},{5,1},{6,1},{7,1},{8,2},{9,2},{10,2}});
        deal(g);QCOMPARE(g.current(),1);g.act(1,"raise");g.act(2,"raise");g.act(0,"call");g.act(1,"call");
        while(g.stage()=="draw")g.act(g.current(),"draw");
        QCOMPARE(g.current(),1);g.act(1,"raise",{},40);g.act(2,"fold");g.act(0,"call");
        QCOMPARE(g.players()[0].chips,230);QCOMPARE(g.players()[1].chips,15);QCOMPARE(g.players()[2].chips,55);
        // Short player has a royal flush, deep player aces, middle player kings.
        g.rigNextDeck({{13,0},{14,0},{10,3},{13,1},{14,1},{11,3},{2,0},{3,0},{12,3},{5,1},{6,1},{13,3},{8,2},{9,2},{14,3}});
        deal(g);QCOMPARE(g.current(),2);g.act(2,"raise");g.act(0,"raise");g.act(1,"call");g.act(2,"call");
        while(g.stage()=="draw")g.act(g.current(),"draw");
        QCOMPARE(g.current(),2);g.act(2,"allin");g.act(0,"call");
        QVERIFY(!g.active());QCOMPARE(g.players()[0].chips,255);QCOMPARE(g.players()[1].chips,45);QCOMPARE(g.players()[2].chips,0);
        QCOMPARE(g.players()[0].outcome,QString("Won"));QCOMPARE(g.players()[1].outcome,QString("Won"));QCOMPARE(g.players()[2].outcome,QString("Lost"));
    }
    void simulationsConservePokerChips() {
        for(auto kind:{Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem})for(bool jacks:{false,true})for(int count=2;count<=5;++count) {
            Game g(400+int(kind)*50+count+(jacks?1:0));auto c=config(kind,count,jacks);c.bankroll=100;g.configure(c);
            for(int hand=0;hand<70 && deal(g);++hand) {
                int actions=0;
                while(g.active()&&actions++<600){QVERIFY(!g.legalActions().empty());int before=g.totalChips();g.computerStep();QCOMPARE(g.totalChips(),before);for(auto &s:g.players())QVERIFY(s.chips>=0);}
                QVERIFY2(!g.active(),qPrintable(QString("Stalled %1/%2/%3: %4").arg(int(kind)).arg(count).arg(jacks).arg(g.stage())));
                QCOMPARE(g.totalChips()-houseLoans(g),count*100);
                std::set<std::pair<int,int>> cards;
                for(auto &s:g.players())for(auto card:s.cards)QVERIFY(cards.emplace(card.rank,card.suit).second);
                for(auto c:g.snapshot()["board"].toArray()){auto card=c.toObject();QVERIFY(cards.emplace(card["rank"].toInt(),card["suit"].toInt()).second);}
            }
        }
    }
    void blackjackFiveSeatSimulation(){Game g(425);auto c=config(Kind::Blackjack,5);c.bankroll=100000;g.configure(c);for(int n=0;n<500;++n){QVERIFY(deal(g));int moves=0;while(g.active()&&++moves<500)g.computerStep();QVERIFY(!g.active());for(auto &s:g.players())QVERIFY(s.chips>=0);}}
    void chooseWagersBeforeEveryDeal() {
        Game g(5);auto c=config(Kind::Blackjack,3);c.players[0].control=Control::Human;c.players[1].control=Control::Remote;g.configure(c);
        QVERIFY(!g.deal());g.computerStep();QVERIFY(g.players()[2].wagerReady);QVERIFY(!g.players()[0].wagerReady);QVERIFY(!g.players()[1].wagerReady);QVERIFY(!g.deal());
        QVERIFY(!g.placeWager(0,{}));QVERIFY(!g.placeWager(0,{-1,0,0,0,0}));QVERIFY(!g.placeWager(0,{1000000,0,0,0,0}));
        QVERIFY(g.placeWager(0,{3,0,0,0,0}));QCOMPARE(g.players()[0].chips,985);QCOMPARE(g.totalChips(),3000.0);QVERIFY(!g.deal());
        QVERIFY(g.clearWager(0));QCOMPARE(g.players()[0].chips,1000);QVERIFY(g.placeWager(0,{2,0,0,0,0}));QVERIFY(g.placeWager(1,{0,4,0,0,0}));QVERIFY(g.betsReady());g.rigNextDeck({{10,0},{9,0},{8,0},{10,1},{8,1},{8,2},{8,3},{7,1}});QVERIFY(g.deal());QVERIFY(g.active());
        QVERIFY(!g.placeWager(0,{1,0,0,0,0}));QVERIFY(!g.clearWager(0));finish(g);QVERIFY(!g.betsReady());QVERIFY(!g.deal());
        QVERIFY(g.placeWager(0,{1,0,0,0,0}));double total=g.totalChips();QVERIFY(g.setChipValues({50,10,1,25,100}));QCOMPARE(g.totalChips(),total);QVERIFY(!g.players()[0].wagerReady);
    }
    void cashoutAndHalfUnitPayouts() {
        Game g(5);auto c=config(Kind::Blackjack,1);c.cashoutTarget=1005;g.configure(c);g.rigNextDeck({{14,0},{10,0},{13,1},{7,2}});QVERIFY(g.placeWager(0,{1,0,0,0,0}));QVERIFY(g.deal());QVERIFY(g.sessionEnded());QCOMPARE(g.players()[0].chips,1007);QCOMPARE(g.players()[0].halfUnit,1);QCOMPARE(g.totalChips(),1007.5);QVERIFY(!g.deal());
        c.cashoutTarget=0;g.configure(c);QVERIFY(g.placeWager(0,{2,0,0,0,0}));g.requestCashout();QVERIFY(g.sessionEnded());QCOMPARE(g.totalChips(),1000.0);QCOMPARE(g.players()[0].chips,1000);QVERIFY(!g.clearWager(0));
        c=config(Kind::Holdem,3);g.configure(c);g.deal();g.requestCashout();QVERIFY(!g.sessionEnded());finish(g);QVERIFY(g.sessionEnded());QCOMPARE(g.totalChips(),3000.0);QVERIFY(!g.deal());
    }
    void networkWagersProfileAndCashout() {
        Game g(5),unused;auto c=config(Kind::Blackjack,2);c.players[0].control=Control::Human;c.players[1].control=Control::Remote;g.configure(c);Network host(&g),client(&unused);QVERIFY(host.host(0));client.join("127.0.0.1",host.port(),host.roomCode(),"Saved player",750.5);QTRY_VERIFY(host.ready());QCOMPARE(g.players()[1].chips,750);QCOMPARE(g.players()[1].halfUnit,1);
        QVERIFY(g.placeWager(0,{2,0,0,0,0}));QVERIFY(!g.deal());client.sendWager({0,2,0,0,0});QTRY_VERIFY(g.betsReady());QCOMPARE(g.players()[1].chips,730);QCOMPARE(g.players()[1].wager,20);
        client.sendWager({},true);QTRY_VERIFY(!g.players()[1].wagerReady);QCOMPARE(g.players()[1].chips,750);client.sendWager({0,3,0,0,0});QTRY_VERIFY(g.betsReady());
        host.setPaused(true);client.sendWager({},true);QTest::qWait(40);QCOMPARE(g.players()[1].wager,30);host.setPaused(false);QTRY_VERIFY(client.ready());
        client.sendCashout();QTRY_VERIFY(g.sessionEnded());QTRY_VERIFY(client.remoteState()["cashedOut"].toBool());QCOMPARE(g.totalChips(),1750.5);QCOMPARE(g.players()[1].chips,750);
    }
    void chipMouseControlsAndProfilePersistence() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();w.refresh();
        auto *red=w.findChild<QToolButton *>("betChipRed");auto *blue=w.findChild<QToolButton *>("betChipBlue");auto *place=w.findChild<QPushButton *>("placeWagerButton");auto *dealButton=w.findChild<QPushButton *>("dealButton");QVERIFY(red);QVERIFY(!dealButton->isEnabled());
        QTest::mouseClick(red,Qt::LeftButton);QTest::mouseClick(blue,Qt::LeftButton);QCOMPARE(w.canvas()->state()["draftAmount"].toInt(),15);QTest::mouseClick(blue,Qt::RightButton);QCOMPARE(w.canvas()->state()["draftAmount"].toInt(),5);QTest::mouseClick(place,Qt::LeftButton);QCOMPARE(w.engine().players()[0].chips,995);QVERIFY(dealButton->isHidden());QVERIFY(!dealButton->isEnabled());
        w.engine().rigNextDeck({{14,0},{10,0},{13,1},{7,2}});QTRY_COMPARE_WITH_TIMEOUT(w.engine().handNumber(),1,1500);QCOMPARE(QSettings().value("profile/balance").toDouble(),1007.5);QCOMPARE(QSettings().value("profile/hands").toInt(),1);w.refresh();QCOMPARE(QSettings().value("profile/hands").toInt(),1);
        w.findChild<QPushButton *>("cashoutButton")->click();QCOMPARE(QSettings().value("profile/lastCashout").toDouble(),1007.5);QCOMPARE(QSettings().value("profile/cashouts").toInt(),1);
        Window reopened(false,false);QCOMPARE(reopened.engine().players()[0].chips,1007);QCOMPARE(reopened.engine().players()[0].halfUnit,1);
        QTimer::singleShot(20,&w,[&]{auto *d=w.findChild<QDialog *>("profileDialog");QVERIFY(d);d->findChild<QLineEdit *>("profileName")->setText("Local Ace");d->findChild<QDoubleSpinBox *>("profileBalance")->setValue(1234.5);d->accept();});w.profileSettings();
        Window changed(false,false);QCOMPARE(changed.engine().players()[0].chips,1234);QCOMPARE(changed.engine().players()[0].halfUnit,1);QCOMPARE(changed.engine().players()[0].config.name,QString("Local Ace"));
    }
    void pokerChipRaiseAndAllInButtons() {
        Window w(false,false);auto c=config(Kind::Holdem,2);c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(w.engine().deal());w.refresh();w.show();
        auto *blue=w.findChild<QToolButton *>("betChipBlue");auto *raise=w.findChild<QPushButton *>("raiseButton");auto *allin=w.findChild<QPushButton *>("allinButton");QVERIFY(raise->isVisible());QVERIFY(allin->isVisible());
        QTest::mouseClick(blue,Qt::LeftButton);QVERIFY(!raise->isEnabled());QTest::mouseClick(blue,Qt::LeftButton);QVERIFY(!raise->isEnabled());QTest::mouseClick(blue,Qt::LeftButton);QVERIFY(raise->isEnabled());QVERIFY(raise->text().contains("40"));QTest::mouseClick(raise,Qt::LeftButton);QCOMPARE(w.engine().players()[0].streetBet,40);QCOMPARE(w.engine().players()[0].chips,960);
        QVERIFY(w.engine().act(1,"call"));QVERIFY(w.engine().act(1,"check"));w.refresh();QTest::mouseClick(allin,Qt::LeftButton);QCOMPARE(w.engine().players()[0].chips,0);QVERIFY(w.engine().act(1,"call"));QVERIFY(!w.engine().active());QCOMPARE(w.engine().totalChips(),2000.0);
    }
    void profileSaveUpdatesCurrentSeat() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();
        auto edit=[&](const QString &name,double value,bool save,bool bankrollEnabled){
            QTimer::singleShot(20,&w,[&]{auto *d=w.findChild<QDialog *>("profileDialog");QVERIFY(d);auto *balance=d->findChild<QDoubleSpinBox *>("profileBalance");QCOMPARE(balance->isEnabled(),bankrollEnabled);d->findChild<QLineEdit *>("profileName")->setText(name);if(bankrollEnabled)balance->setValue(value);d->findChild<QDialogButtonBox *>()->button(save?QDialogButtonBox::Save:QDialogButtonBox::Cancel)->click();});w.profileSettings();
        };
        QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);edit("Live Ace",900.5,true,true);
        QCOMPARE(w.engine().players()[0].chips,900);QCOMPARE(w.engine().players()[0].halfUnit,1);QCOMPARE(w.engine().players()[0].config.name,QString("Live Ace"));QCOMPARE(w.engine().handNumber(),0);
        QCOMPARE(w.canvas()->state()["players"].toArray()[0].toObject()["name"].toString(),QString("Live Ace"));QCOMPARE(w.canvas()->state()["draftAmount"].toInt(),0);QCOMPARE(QSettings().value("profile/balance").toDouble(),900.5);
        edit("Cancel me",1,false,true);QCOMPARE(w.engine().players()[0].chips,900);QCOMPARE(QSettings().value("profile/name").toString(),QString("Live Ace"));
        QVERIFY(w.engine().placeWager(0,{0,1,0,0,0}));w.refresh();edit("Live Ace",750.5,true,true);QCOMPARE(w.engine().players()[0].chips,750);QVERIFY(!w.engine().players()[0].wagerReady);QCOMPARE(w.engine().players()[0].committed,0);
        w.engine().rigNextDeck({{10,0},{10,1},{8,0},{7,1}});QVERIFY(deal(w.engine()));w.refresh();auto cards=w.engine().players()[0].hands[0].cards;edit("During hand",0,true,false);
        QCOMPARE(w.engine().players()[0].config.name,QString("During hand"));QCOMPARE(w.engine().players()[0].hands[0].cards,cards);QVERIFY(w.engine().active());
        finish(w.engine());w.refresh();edit("After hand",333.5,true,true);QCOMPARE(w.engine().players()[0].chips,333);QCOMPARE(w.engine().players()[0].halfUnit,1);QCOMPARE(w.engine().handNumber(),1);QCOMPARE(QSettings().value("profile/balance").toDouble(),333.5);
        w.refresh();QCOMPARE(QSettings().value("profile/balance").toDouble(),333.5);Window reopened(false,false);QCOMPARE(reopened.engine().players()[0].chips,333);QCOMPARE(reopened.engine().players()[0].config.name,QString("After hand"));
    }
    void profileRejectsInvalidBankrollWithoutChangingHand() {
        Game g;g.configure(config(Kind::Blackjack,1));auto original=g.snapshot(0);
        for(double bad:{-1.0,0.1,10000001.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){QVERIFY(!g.updateProfile(0,"Wrong",bad));QCOMPARE(g.snapshot(0),original);}
        QVERIFY(!g.updateProfile(-1,"Wrong",100));QVERIFY(!g.updateProfile(0," ",100));
        g.rigNextDeck({{8,0},{10,1},{7,0},{7,1}});QVERIFY(deal(g));QVERIFY(g.active());{auto before=g.snapshot(0);QVERIFY(!g.updateProfile(0,"Wrong",100));QCOMPARE(g.snapshot(0),before);}
        finish(g);QVERIFY(g.updateProfile(0,"New name",0.5));QCOMPARE(g.players()[0].chips,0);QCOMPARE(g.players()[0].halfUnit,1);
    }
    void networkProfileUpdatesSeatAndSurvivesReconnect() {
        Game g;auto c=config(Kind::Blackjack,2);c.players[0].control=Control::Human;c.players[1].control=Control::Remote;g.configure(c);Network host(&g);QVERIFY(host.host(0));
        Window w(false,false);auto *client=w.findChild<Network *>();client->join("127.0.0.1",host.port(),host.roomCode(),"Remote",1000);QTRY_VERIFY(host.ready());QTRY_COMPARE(client->remoteState()["viewer"].toInt(-1),1);
        QSignalSpy saved(client,&Network::profileSaved),errors(client,&Network::notice);
        QTimer::singleShot(20,&w,[&]{auto *d=w.findChild<QDialog *>("profileDialog");QVERIFY(d);d->findChild<QLineEdit *>("profileName")->setText("Network Ace");d->findChild<QDoubleSpinBox *>("profileBalance")->setValue(500.5);d->accept();});w.profileSettings();
        QTRY_COMPARE(saved.size(),1);QCOMPARE(g.players()[1].config.name,QString("Network Ace"));QCOMPARE(g.players()[1].chips,500);QCOMPARE(g.players()[1].halfUnit,1);QCOMPARE(g.players()[0].chips,1000);QCOMPARE(QSettings().value("profile/balance").toDouble(),500.5);
        QTRY_COMPARE(w.canvas()->state()["players"].toArray()[1].toObject()["name"].toString(),QString("Network Ace"));
        g.rigNextDeck({{10,0},{9,1},{10,2},{8,0},{7,1},{7,2}});QVERIFY(deal(g));host.setInputBlocked(true);host.publish();QTRY_COMPARE(client->remoteState()["phase"].toString(),QString("blackjack"));
        QVERIFY(client->sendProfile("Renamed mid-deal"));QTRY_COMPARE(saved.size(),2);QCOMPARE(g.players()[1].config.name,QString("Renamed mid-deal"));
        QTRY_COMPARE(w.canvas()->state()["players"].toArray()[1].toObject()["name"].toString(),QString("Renamed mid-deal"));
        const auto before=g.snapshot(1);QVERIFY(client->sendProfile("Should reject",20));QTRY_VERIFY(!errors.isEmpty());QCOMPARE(g.snapshot(1),before);QCOMPARE(QSettings().value("profile/name").toString(),QString("Renamed mid-deal"));
        auto sockets=host.findChildren<QTcpSocket *>();QVERIFY(!sockets.isEmpty());sockets.first()->abort();QTRY_VERIFY(!host.ready());QTRY_VERIFY_WITH_TIMEOUT(host.ready(),5000);
        QCOMPARE(g.players()[1].config.name,QString("Renamed mid-deal"));QCOMPARE(g.players()[1].chips,480);QCOMPARE(g.players()[1].halfUnit,1);
    }
    void finishedCardsClearBeforeNextBet() {
        Window w(false,false);auto c=config(Kind::Blackjack,1);c.players[0].control=Control::Human;w.startConfig(c);w.show();QTest::qWait(20);const auto geometry=w.canvas()->geometry();
        w.engine().rigNextDeck({{10,0},{10,1},{8,0},{7,1}});QVERIFY(deal(w.engine()));finish(w.engine());w.refresh();auto finalDealer=w.canvas()->state()["dealer"].toArray();QVERIFY(!finalDealer.isEmpty());
        const double balance=QSettings().value("profile/balance").toDouble();QTest::qWait(2600);QCOMPARE(w.canvas()->state()["dealer"].toArray(),finalDealer);
        QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QVERIFY(w.canvas()->state()["dealer"].toArray().isEmpty());QVERIFY(w.canvas()->state()["players"].toArray()[0].toObject()["hands"].toArray().isEmpty());QCOMPARE(w.canvas()->geometry(),geometry);QCOMPARE(QSettings().value("profile/balance").toDouble(),balance);QCOMPARE(w.canvas()->state()["draftAmount"].toInt(),10);
        w.refresh();QVERIFY(w.canvas()->state()["dealer"].toArray().isEmpty());w.engine().rigNextDeck({{8,0},{10,1},{7,0},{7,1}});QTest::mouseClick(w.canvas(),Qt::LeftButton,Qt::NoModifier,w.canvas()->rect().center());QTRY_COMPARE(w.engine().handNumber(),2);QVERIFY(!w.canvas()->state()["dealer"].toArray().isEmpty());QVERIFY(w.engine().active());
        for(Kind kind:{Kind::Draw,Kind::Stud5,Kind::Stud7,Kind::Holdem}){
            c=config(kind,2);c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(deal(w.engine()));finish(w.engine());w.refresh();QJsonValue opponentResult=w.canvas()->state()["players"].toArray()[1].toObject()["status"];auto finalCards=w.canvas()->state()["players"].toArray()[0].toObject()["cards"].toArray();auto finalBoard=w.canvas()->state()["board"].toArray();QVERIFY(!finalCards.isEmpty());QTest::qWait(2600);QCOMPARE(w.canvas()->state()["players"].toArray()[0].toObject()["cards"].toArray(),finalCards);QCOMPARE(w.canvas()->state()["board"].toArray(),finalBoard);QCOMPARE(w.canvas()->state()["players"].toArray()[1].toObject()["status"],opponentResult);
            if(kind==Kind::Holdem){QCOMPARE(finalBoard.size(),5);auto before=w.engine().snapshot();QTest::mouseClick(w.findChild<QToolButton *>("betChipBlue"),Qt::LeftButton);QVERIFY(w.canvas()->state()["board"].toArray().isEmpty());QVERIFY(w.canvas()->state()["players"].toArray()[0].toObject()["cards"].toArray().isEmpty());QCOMPARE(w.engine().snapshot(),before);w.findChild<QPushButton *>("dealButton")->click();QCOMPARE(w.engine().handNumber(),2);QVERIFY(w.canvas()->state()["board"].toArray().isEmpty());}
            else{QTest::mouseClick(w.findChild<QToolButton *>("betChipRed"),Qt::LeftButton);for(auto value:w.canvas()->state()["players"].toArray())QVERIFY(value.toObject()["cards"].toArray().isEmpty());QCOMPARE(w.canvas()->state()["draftAmount"].toInt(),0);QVERIFY(w.findChild<QToolButton *>("betChipRed")->text().contains("×1"));QCOMPARE(w.engine().handNumber(),1);w.refresh();QVERIFY(w.canvas()->state()["players"].toArray()[0].toObject()["cards"].toArray().isEmpty());}
            QCOMPARE(w.canvas()->geometry(),geometry);
        }
    }
    void networkRaiseAmountsAndAllIn() {
        Game g(2),unused;auto c=config(Kind::Holdem,2);for(auto &p:c.players)p.control=Control::Remote;g.configure(c);Network host(&g),a(&unused),b(&unused);QVERIFY(host.host(0));a.join("127.0.0.1",host.port(),host.roomCode(),"A");QTRY_VERIFY(!a.remoteState().isEmpty());b.join("127.0.0.1",host.port(),host.roomCode(),"B");QTRY_VERIFY(host.ready());QVERIFY(g.deal());host.publish();QTRY_COMPARE(a.remoteState()["hand"].toInt(),1);QTRY_COMPARE(b.remoteState()["hand"].toInt(),1);
        a.sendAction("raise",{},70,{1,0,0,0,0});QTest::qWait(100);QCOMPARE(g.players()[0].streetBet,10); // wrong payment is rejected
        a.sendAction("raise",{},70,{12,0,0,0,0});QTRY_COMPARE(g.players()[0].streetBet,70);QCOMPARE(g.players()[0].committedChips.counts,(std::array<int,5>{12,1,0,0,0}));QCOMPARE(g.players()[0].bankChips.counts[1],5);QCOMPARE(g.players()[0].bankChips.counts[4],7);
        QTRY_COMPARE(b.remoteState()["players"].toArray()[0].toObject()["committedChips"].toArray(),QJsonArray({12,1,0,0,0}));QCOMPARE(g.snapshot()["raiseTo"].toInt(),120);QTRY_COMPARE(b.remoteState()["turn"].toInt(),1);b.sendAction("allin");QTRY_COMPARE(g.players()[1].chips,0);QTRY_COMPARE(a.remoteState()["turn"].toInt(),0);a.sendAction("call");QTRY_VERIFY(!g.active());QCOMPARE(g.totalChips(),2000.0);
        QTRY_COMPARE(a.remoteState()["phase"].toString(),QString("finished"));QCOMPARE(a.remoteState()["players"].toArray()[0].toObject()["bankChips"],g.snapshot()["players"].toArray()[0].toObject()["bankChips"]);
    }
    void networkPrivacyActionsReconnect() {
        Game hostGame(4),unused;auto c=config(Kind::Stud7,3);c.players[0].control=Control::Human;c.players[1].control=Control::Remote;c.players[2].control=Control::Remote;hostGame.configure(c);Network host(&hostGame),a(&unused),b(&unused);QVERIFY(host.host(0));QVERIFY(!host.ready());
        a.join("127.0.0.1",host.port(),host.roomCode(),"Network A");b.join("127.0.0.1",host.port(),host.roomCode(),"Network B");QTRY_VERIFY_WITH_TIMEOUT(host.ready(),3000);QVERIFY(deal(hostGame));host.publish();QTRY_COMPARE(a.remoteState()["hand"].toInt(),1);QTRY_COMPARE(b.remoteState()["hand"].toInt(),1);
        const ChipValues sharedValues{50,100,1,500,1000};QVERIFY(hostGame.setChipValues(sharedValues));host.publish();QTRY_COMPARE(a.remoteState()["chipValues"].toArray(),chipValuesJson(sharedValues));QTRY_COMPARE(b.remoteState()["chipValues"].toArray(),chipValuesJson(sharedValues));
        for(auto *client:{&a,&b}){auto state=client->remoteState();int viewer=state["viewer"].toInt();QVERIFY(viewer>0);for(int seat=0;seat<3;++seat){auto cards=state["players"].toArray()[seat].toObject()["cards"].toArray();QCOMPARE(cards[0].toObject()["hidden"].toBool(),seat!=viewer);QCOMPARE(cards[1].toObject()["hidden"].toBool(),seat!=viewer);QVERIFY(!cards[2].toObject()["hidden"].toBool());if(seat!=viewer)QVERIFY(!cards[0].toObject().contains("rank"));}}
        int turn=hostGame.current();int chips=hostGame.totalChips();a.sendAction("raise");b.sendAction("raise");QTest::qWait(80);QCOMPARE(hostGame.totalChips(),chips);QVERIFY(hostGame.current()!=turn || turn==0); // only the valid seat acts
        host.setPaused(true);int pot=hostGame.potValue();a.sendAction("call");b.sendAction("call");QTest::qWait(50);QCOMPARE(hostGame.potValue(),pot);host.setPaused(false);
        int aSeat=a.remoteState()["viewer"].toInt(),bSeat=b.remoteState()["viewer"].toInt();
        auto *server=host.findChild<QTcpServer *>();QVERIFY(server);auto peers=server->findChildren<QTcpSocket *>();QVERIFY(!peers.isEmpty());peers.first()->abort();QTRY_VERIFY(!host.ready());QTRY_VERIFY_WITH_TIMEOUT(host.ready(),5000);
        QTRY_VERIFY(a.ready());QTRY_VERIFY(b.ready());QCOMPARE(a.remoteState()["viewer"].toInt(),aSeat);QCOMPARE(b.remoteState()["viewer"].toInt(),bSeat);
        QCOMPARE(a.remoteState()["chipValues"].toArray(),chipValuesJson(sharedValues));QCOMPARE(b.remoteState()["chipValues"].toArray(),chipValuesJson(sharedValues));
        a.disconnectSession();QTRY_VERIFY(!host.ready());host.publish();QTRY_VERIFY(!b.ready());
    }
    void badRoomRejected(){Game g,unused;auto c=config(Kind::Blackjack);c.players[1].control=Control::Remote;g.configure(c);Network host(&g),client(&unused);QVERIFY(host.host(0));QSignalSpy notices(&client,&Network::notice);client.join("127.0.0.1",host.port(),"BADCODE1","Intruder");QTRY_VERIFY(notices.size()>0);QVERIFY(!host.ready());QVERIFY(client.remoteState().isEmpty());}
    void chipSettingsSaveCancelAndReopen() {
        Window w(false,false);w.show();w.engine().rigNextDeck({{10,0},{9,0},{10,1},{8,1},{8,2},{7,1}});QVERIFY(deal(w.engine()));auto before=rulesOnly(w.engine().snapshot(0));
        const ChipValues custom{50,100,1,500,1000};
        QTimer::singleShot(20,&w,[&]{auto *dialog=w.findChild<QDialog *>("chipSettingsDialog");if(!dialog)return;for(int i=0;i<5;++i){auto *input=dialog->findChild<QSpinBox *>(QString("chipValue%1").arg(ChipNames[i]));if(input)input->setValue(custom[i]);}QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty())dialog->grab().save(dir+"/casino-chip-settings.png");dialog->accept();});w.chipSettings();
        QCOMPARE(w.engine().config().chipValues,custom);auto after=rulesOnly(w.engine().snapshot(0));QCOMPARE(after,before);
        for(int i=0;i<5;++i){auto *label=w.findChild<QLabel *>(QString("chipLegend%1").arg(ChipNames[i]));QVERIFY(label);QVERIFY(label->text().contains(QLocale().toString(custom[i])));}
        QTimer::singleShot(20,&w,[&]{auto *dialog=w.findChild<QDialog *>("chipSettingsDialog");if(!dialog)return;dialog->findChild<QSpinBox *>("chipValueRed")->setValue(999);dialog->reject();});w.chipSettings();QCOMPARE(w.engine().config().chipValues,custom);
        Window reopened(false,false);QCOMPARE(reopened.engine().config().chipValues,custom);
        QTimer::singleShot(20,&w,[&]{auto *dialog=w.findChild<QDialog *>("chipSettingsDialog");if(!dialog)return;auto *box=dialog->findChild<QDialogButtonBox *>();box->button(QDialogButtonBox::RestoreDefaults)->click();dialog->accept();});w.chipSettings();QCOMPARE(w.engine().config().chipValues,DefaultChipValues);
    }
    void sideCardsDoNotConfirmBets() {
        Window w(false,false);auto c=config(Kind::Draw,5);c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(deal(w.engine()));w.refresh();w.resize(1886,939);w.show();QTest::qWait(20);
        auto *table=w.canvas();QSignalSpy bets(table,&Table::betConfirmed);qreal scale=std::min(table->width()/1280.0,table->height()/820.0);QPointF offset((table->width()-1280*scale)/2,(table->height()-820*scale)/2);
        for(QPointF card:{QPointF(316,318),QPointF(316,592),QPointF(964,318),QPointF(964,592),QPointF(640,624)})QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,(offset+card*scale).toPoint());
        QCOMPARE(bets.size(),0);QTest::mouseClick(table,Qt::LeftButton,Qt::NoModifier,(offset+QPointF(640,250)*scale).toPoint());QCOMPARE(bets.size(),1);
    }
    void uiThemesAndScreenshots() {
        QCOMPARE(themes().size(),20);QSet<QString> names;for(auto &t:themes()){QVERIFY(QColor(t.felt).isValid());names.insert(t.name);}QCOMPARE(names.size(),20);
        Window w(false,false);auto c=config(Kind::Holdem,5);c.players[0].control=Control::Human;w.startConfig(c);deal(w.engine());w.refresh();w.show();QTest::qWait(20);QVERIFY(w.canvas()->isVisible());
        QString dir=qEnvironmentVariable("CASINO_ARTIFACT_DIR");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(w.grab().save(dir+"/casino-holdem.png"));}
        while(w.engine().snapshot()["board"].toArray().size()<3){auto actions=w.engine().legalActions();QVERIFY(w.engine().act(w.engine().current(),actions.contains("call")?"call":"check"));}w.refresh();QTest::qWait(20);
        if(!dir.isEmpty()){QVERIFY(w.grab().save(dir+"/casino-holdem-pot.png"));auto size=w.size();w.resize(1040,800);QTest::qWait(20);QVERIFY(w.grab().save(dir+"/casino-holdem-pot-compact.png"));w.resize(size);QTest::qWait(20);}
        QTimer::singleShot(20,&w,[&]{auto *d=w.findChild<QDialog *>();QVERIFY(d);auto *combo=d->findChild<QComboBox *>("theme0");QVERIFY(combo);QCOMPARE(combo->count(),20);combo->setCurrentIndex(9);auto *face=d->findChild<QComboBox *>("theme1");auto *back=d->findChild<QComboBox *>("theme2");QVERIFY(face&&back);QCOMPARE(face->count(),20);QCOMPARE(back->count(),20);QVERIFY(face->iconSize().height()>40);auto *picture=d->findChild<QLabel *>("cardThemePreview");QVERIFY(picture);auto oldPicture=picture->pixmap().toImage();face->setCurrentIndex(3);QVERIFY(picture->pixmap().toImage()!=oldPicture);back->setCurrentIndex(9);if(!dir.isEmpty())QVERIFY(d->grab().save(dir+"/casino-themes.png"));d->reject();});w.appearance();
        QTimer::singleShot(20,&w,[&]{auto *d=w.findChild<QDialog *>("setupDialog");QVERIFY(d);auto *variant=d->findChild<QComboBox *>("pokerVariant");QVERIFY(variant);QCOMPARE(variant->count(),4);QVERIFY(variant->isHidden());QVERIFY(d->findChild<QCheckBox *>("jacksOption")->isHidden());d->findChild<QPushButton *>("choosePoker")->click();QVERIFY(!variant->isHidden());d->findChild<QPushButton *>("chooseBlackjack")->click();QVERIFY(variant->isHidden());auto *count=d->findChild<QSpinBox *>("playerCount");QCOMPARE(count->maximum(),5);QTest::qWait(20);if(!dir.isEmpty())QVERIFY(d->grab().save(dir+"/casino-setup.png"));d->reject();});w.setup();
        c=config(Kind::Stud7,5);c.players[0].control=Control::Human;w.startConfig(c);deal(w.engine());finish(w.engine());w.refresh();if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-stud.png"));
        for(Kind kind:{Kind::Draw,Kind::Holdem}){c=config(kind,4);c.firstCardSeat=0;c.players[0].control=Control::Human;w.startConfig(c);QVERIFY(deal(w.engine()));w.refresh();QTest::qWait(20);if(!dir.isEmpty())QVERIFY(w.grab().save(dir+QString("/casino-four-player-%1-first.png").arg(int(kind))));finish(w.engine());QVERIFY(deal(w.engine()));w.refresh();QTest::qWait(20);if(!dir.isEmpty())QVERIFY(w.grab().save(dir+QString("/casino-four-player-%1-rotated.png").arg(int(kind))));}
        c=config(Kind::Blackjack,5);c.players[0].control=Control::Human;w.startConfig(c);w.refresh();QTest::qWait(20);if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-betting.png"));w.resize(1886,939);QTest::qWait(20);if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-layout-large.png"));deal(w.engine());w.refresh();if(!dir.isEmpty())QVERIFY(w.grab().save(dir+"/casino-blackjack.png"));
        // Visual fixtures exercise every color, short/empty/rich stacks and
        // shoe fill extremes without changing the actual game state.
        auto original=w.canvas()->state(),preview=original;auto people=preview["players"].toArray();std::array<double,5> balances{3141.5,1000,80,0,10000000};
        for(int i=0;i<5;++i){auto person=people[i].toObject();person["chips"]=balances[i];auto chips=breakDownChips(int(balances[i]),readChipValues(preview["chipValues"].toArray()));person["bankChips"]=chipValuesJson(chips.counts);person["bankChange"]=chips.change+balances[i]-int(balances[i]);people[i]=person;}preview["players"]=people;
        for(int remaining:{0,104,416}){preview["cardsRemaining"]=remaining;w.canvas()->setState(preview);if(!dir.isEmpty())QVERIFY(w.canvas()->grab().save(dir+QString("/casino-stacks-shoe-%1.png").arg(remaining)));}
        w.canvas()->setState(original);
    }
};
QTEST_MAIN(CasinoTests)
#include "tests.moc"
