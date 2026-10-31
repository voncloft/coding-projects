#include "game.h"
#include "window.h"
#include <QtTest>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <set>

namespace Golf {
class GameTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static void start(Game &g) {
        for(int i=0;i<7;++i) QVERIFY(g.open(i));
        while(g.state().phase==Phase::Opening) g.computerStep();
    }
    static bool conserved(const Game &g) {
        std::set<int> ids; int n=0;
        auto add=[&](int id){ ++n; return id>=0 && id<108 && ids.insert(id).second; };
        for(int id:g.s.deck) if(!add(id))return false;
        for(int id:g.s.discards) if(!add(id))return false;
        for(auto h:g.s.hands) for(auto c:h) if(!add(c.card))return false;
        if(g.s.held>=0 && !add(g.s.held))return false;
        return n==108 && ids.size()==108;
    }
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName("TurnoverGolfTests"); QCoreApplication::setApplicationName("TurnoverGolfTests");
        QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,settings.path());
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    }
    void allDealtFaceDownAndSevenUp() {
        for(int opponents=1;opponents<=4;++opponents) {
            Game g(101); g.newMatch(opponents);
            QCOMPARE(g.players(),opponents+1); QVERIFY(conserved(g));
            for(int p=0;p<g.players();++p) QCOMPARE(g.hidden(p),8);
            std::array<int,14> counts{};
            for(int id=0;id<108;++id) ++counts[value(id)==-5?13:value(id)];
            for(int rank=0;rank<13;++rank) QCOMPARE(counts[rank],8);
            QCOMPARE(counts[13],4);
            QVERIFY(!g.draw(false)); QVERIFY(!g.open(-1)); QVERIFY(!g.open(8));
            QVERIFY(g.open(0)); QVERIFY(!g.open(0));
            for(int i=1;i<7;++i) QVERIFY(g.open(i));
            QCOMPARE(g.hidden(0),1); QCOMPARE(g.state().current,1);
            for(int p=1;p<g.players();++p) QCOMPARE(g.hidden(p),8);
            while(g.state().phase==Phase::Opening) g.computerStep();
            QCOMPARE(g.state().phase,Phase::Draw);
            for(int p=0;p<g.players();++p) QCOMPARE(g.hidden(p),1);
            QVERIFY(!g.open(7)); QVERIFY(conserved(g));
        }
    }
    void gamesAlwaysHaveAnOpponent() {
        Game g(77);g.newMatch(0,3);
        QCOMPARE(g.players(),2);QCOMPARE(g.state().holes,3);QVERIFY(conserved(g));
        for(int i=0;i<7;++i)QVERIFY(g.open(i));
        QCOMPARE(g.state().current,1);QCOMPARE(g.state().phase,Phase::Opening);

        QSettings config;const auto previous=config.value("opponents");
        config.setValue("opponents",0);
        Window w(false);
        if(previous.isValid())config.setValue("opponents",previous);else config.remove("opponents");
        QCOMPARE(w.game().players(),2);QVERIFY(w.session().isComputer(1));

        // A valid legacy solo save must not replace the new two-player match.
        Game solo(77);
        for(auto slot:solo.s.hands.back())solo.s.deck.push_back(slot.card);
        solo.s.opponents=0;solo.s.hands.resize(1);solo.s.turns.resize(1);solo.s.names={"You"};
        QVERIFY(conserved(solo));
        const auto before=g.save();
        QVERIFY(!g.restore(solo.save()));QCOMPARE(g.save(),before);
        QVERIFY(!g.restorePublicView(solo.publicView(0),0));QCOMPARE(g.save(),before);
    }
    void scoring_data() {
        QTest::addColumn<QList<int>>("cards"); QTest::addColumn<int>("expected"); QTest::addColumn<int>("bonus");
        QTest::newRow("unmatched") << QList<int>{1,2,3,4,5,6,7,8} << 36 << 0;
        QTest::newRow("one-pair") << QList<int>{12,1,2,3,12,4,5,6} << 21 << 0;
        QTest::newRow("horizontal-does-not-match") << QList<int>{7,7,2,3,1,4,5,6} << 35 << 0;
        QTest::newRow("two-same-pairs") << QList<int>{7,1,7,3,7,4,7,6} << 4 << -10;
        QTest::newRow("three-same-pairs") << QList<int>{11,11,2,11,11,11,3,11} << -10 << -15;
        QTest::newRow("four-same-pairs") << QList<int>{3,3,3,3,3,3,3,3} << -20 << -20;
        QTest::newRow("different-pairs-no-bonus") << QList<int>{7,6,2,3,7,6,5,4} << 14 << 0;
        QTest::newRow("minus-five-pair") << QList<int>{-5,0,0,0,-5,0,0,0} << -25 << -15;
        QTest::newRow("four-minus-five-no-vertical-pairs") << QList<int>{-5,-5,-5,-5,1,2,3,4} << -10 << 0;
        QTest::newRow("minus-five-pair-keeps-value") << QList<int>{-5,1,2,3,-5,4,5,6} << 11 << 0;
        QTest::newRow("four-minus-five") << QList<int>{-5,1,-5,2,-5,3,-5,4} << -20 << -10;
        QTest::newRow("two-different-bonus-groups") << QList<int>{2,3,2,3,2,3,2,3} << -20 << -20;
    }
    void scoring() {
        QFETCH(QList<int>,cards); QFETCH(int,expected); QFETCH(int,bonus);
        Hand h; for(int i=0;i<8;++i) h[i]={cards[i]==-5?104+i%4:cards[i]*8+i,true};
        QCOMPARE(Game::score(h).total,expected); QCOMPARE(Game::score(h).bonus,bonus);
    }
    void fourMinusFiveCardsContributeMinusThirty() {
        Hand hand{{{104,true},{8,true},{105,true},{16,true},{106,true},{24,true},{107,true},{32,true}}};
        auto score=Game::score(hand);
        QCOMPARE(score.columns[0]+score.columns[2],-20);QCOMPARE(score.bonus,-10);
        QCOMPARE(score.columns[0]+score.columns[2]+score.bonus,-30);
        QCOMPARE(score.total,-20); // The other four unmatched cards add 10.
    }
    void faceDownValueNeverLeaksIntoShownScoreOrAi() {
        Game g(55); start(g); Game other=g;
        int faceDown=-1; for(int i=0;i<8;++i) if(!g.s.hands[0][i].up) faceDown=i;
        other.s.hands[0][faceDown].card=104;
        g.s.hands[0][faceDown].card=96;
        QCOMPARE(Game::score(g.s.hands[0],true).total,Game::score(other.s.hands[0],true).total);
        QCOMPARE(g.estimate(g.s.hands[0]),other.estimate(other.s.hands[0]));
        for(int card=0;card<108;++card) QCOMPARE(g.bestReplacement(card),other.bestReplacement(card));
        QCOMPARE(g.unseenValues(),other.unseenValues());
        QCOMPARE(g.visibleValue(0,faceDown),Hidden);
    }
    void computerRejectsHighFinishEvenWithALead() {
        Game g(44);start(g);
        auto hand=[](std::array<int,8> ranks){Hand h;for(int i=0;i<8;++i)h[i]={ranks[i]==-5?104:ranks[i]*8,i!=7};return h;};
        // This draw would finish at 28, with no vertical matches, ahead of a poor opponent.
        g.s.hands[0]=hand({0,1,2,3,4,5,6,12});
        g.s.hands[1]=hand({12,11,10,9,8,7,6,5});
        g.s.current=0;g.s.phase=Phase::Place;g.s.held=7*8;g.s.fromDiscard=false;
        g.s.turns[0]=100;
        QVERIFY(!g.shouldFinish(g.s.held,0));g.computerStep();
        QCOMPARE(g.s.phase,Phase::Draw);QCOMPARE(g.hidden(0),1);QVERIFY(g.s.scores.empty());
        // A genuinely low finish is still deferred during the first few turns.
        g.s.hands[0]=hand({7,7,1,2,7,7,0,12});
        g.s.current=0;g.s.phase=Phase::Place;g.s.held=0;
        for(int turns=0;turns<6;++turns){g.s.turns[0]=turns;QVERIFY(!g.shouldFinish(g.s.held,0));}
        g.s.turns[0]=6;QVERIFY(g.shouldFinish(g.s.held,0));
    }
    void computerChasesMatchingBonuses() {
        Game g(44);start(g);
        // Two 7-pairs give -10. Replacing the 12 with another 7 earns that bonus.
        std::array<int,8> ranks{7,7,0,0,7,12,1,9};
        for(int i=0;i<8;++i)g.s.hands[0][i]={ranks[i]*8,i!=7};
        g.s.current=0;g.s.phase=Phase::Place;g.s.held=7*8;g.s.turns[0]=2;
        auto best=g.bestReplacement(g.s.held);QCOMPARE(best.first,5);QVERIFY(best.second>10);
        g.computerStep();QCOMPARE(g.visibleValue(0,5),7);QCOMPARE(Game::score(g.s.hands[0],true).bonus,-10);
        QCOMPARE(g.hidden(0),1);QVERIFY(g.s.scores.empty());
    }
    void computerCanFlipSafelyWithoutPeeking() {
        Game g(63);start(g);g.s.current=0;g.s.turns[0]=6;
        std::array<int,8> ranks{7,7,0,0,7,7,0,12};
        for(int i=0;i<8;++i)g.s.hands[0][i]={ranks[i]*8,i!=7};
        std::array<int,8> otherRanks{12,11,10,9,8,7,6,5};
        for(int i=0;i<8;++i)g.s.hands[1][i]={otherRanks[i]*8,i!=7};
        Game alternative=g;alternative.s.hands[0][7].card=104;
        QVERIFY(g.shouldFlipFinal());QCOMPARE(g.shouldFlipFinal(),alternative.shouldFlipFinal());
        const auto deck=g.s.deck,discards=g.s.discards;g.computerStep();
        QCOMPARE(g.s.phase,Phase::HoleOver);QCOMPARE(g.s.deck,deck);QCOMPARE(g.s.discards,discards);
        QCOMPARE(g.s.scores.back()[0],2);QCOMPARE(g.s.hands[0][7].card,96);
    }
    void finalReplacementEndsImmediatelyForEveryPlayer() {
        for(int n=1;n<=4;++n) for(int player=0;player<=n;++player) for(bool takeDiscard:{false,true}) {
            Game g(5); g.newMatch(n); start(g); g.s.current=player;
            int finalPosition=-1;
            for(int i=0;i<8;++i) if(!g.s.hands[player][i].up) finalPosition=i;
            QVERIFY(finalPosition>=0);
            QVERIFY(!g.replace(finalPosition)); QVERIFY(!g.open(finalPosition));
            int old=g.s.hands[player][finalPosition].card;
            auto before=g.s.turns;
            QVERIFY(g.draw(takeDiscard)); int drawn=g.s.held;
            if(takeDiscard) QVERIFY(g.replace(finalPosition)); else QVERIFY(g.finishHole());
            QCOMPARE(g.state().phase,Phase::HoleOver); QCOMPARE(g.s.endingBy,player);
            QCOMPARE(g.s.turns,before); QCOMPARE(g.s.discards.back(),old);
            QCOMPARE(g.s.hands[player][finalPosition].card,drawn);
            for(int p=0;p<g.players();++p) { QCOMPARE(g.hidden(p),0); QCOMPARE(g.s.scores[0][p],Game::score(g.s.hands[p]).total); }
            QVERIFY(conserved(g)); QVERIFY(!g.draw(false)); QVERIFY(!g.finishHole());
            QVERIFY(g.nextHole()); for(int p=0;p<g.players();++p) QCOMPARE(g.hidden(p),8);
        }
    }
    void flippingFinalCardEndsWithoutDrawingForEverySeat() {
        for(int opponents=1;opponents<=3;++opponents)for(int p=0;p<=opponents;++p) {
            Game g(91);g.newMatch(opponents,3);start(g);g.s.current=p;
            const auto deck=g.s.deck,discards=g.s.discards;const auto hands=g.s.hands;
            QVERIFY(g.finishHole());QCOMPARE(g.s.endingBy,p);QCOMPARE(g.s.phase,Phase::HoleOver);
            QCOMPARE(g.s.deck,deck);QCOMPARE(g.s.discards,discards);QCOMPARE(g.s.held,-1);
            for(int seat=0;seat<g.players();++seat){QCOMPARE(g.hidden(seat),0);for(int i=0;i<8;++i)QCOMPARE(g.s.hands[seat][i].card,hands[seat][i].card);}
            QVERIFY(conserved(g));Game copy;QVERIFY(copy.restore(g.save()));QVERIFY(g.nextHole());
        }
    }
    void discardMustBePlayedAndDeckDiscardKeepsCardDown() {
        Game g(18); start(g);
        QVERIFY(g.draw(true)); QVERIFY(!g.discardHeld());
        int incoming=g.state().held; QVERIFY(g.replace(0)); QCOMPARE(g.s.hands[0][0].card,incoming);
        QCOMPARE(g.hidden(0),1); QCOMPARE(g.s.phase,Phase::Draw); QCOMPARE(g.s.current,1); QVERIFY(conserved(g));
        Game deck(18); start(deck);
        QVERIFY(deck.draw(false));
        QVERIFY(!deck.draw(true));QVERIFY(!deck.draw(false)); // Cannot peek and switch piles or draw twice.
        QVERIFY(deck.discardHeld()); QCOMPARE(deck.hidden(0),1); QVERIFY(conserved(deck));
    }
    void exhaustedDeckReshufflesAndKeepsFinalCardsDown() {
        Game g(99); start(g);
        while(g.s.deck.size()>1) { g.s.discards.push_back(g.s.deck.back()); g.s.deck.pop_back(); }
        QVERIFY(g.draw(false)); QVERIFY(g.s.deck.empty()); QCOMPARE(g.s.phase,Phase::Place);
        QVERIFY(g.replace(0)); QCOMPARE(g.s.phase,Phase::Draw); QVERIFY(g.s.scores.empty());
        Game restored;QVERIFY(restored.restore(g.save()));
        int top=g.s.discards.back(), recycled=int(g.s.discards.size())-1;
        QVERIFY(g.draw(false));QCOMPARE(g.s.discards.size(),size_t(1));QCOMPARE(g.s.discards.back(),top);QCOMPARE(g.s.deck.size(),size_t(recycled-1));
        for(int p=0;p<g.players();++p)QCOMPARE(g.hidden(p),1);
        QVERIFY(g.s.scores.empty());QVERIFY(conserved(g));
        // Exercise more than two complete deck cycles without choosing to end the round.
        for(int turn=0;turn<250;++turn){QVERIFY(g.discardHeld());QVERIFY(g.draw(false));QVERIFY(conserved(g));}
        QVERIFY(g.s.scores.empty());for(int p=0;p<g.players();++p)QCOMPARE(g.hidden(p),1);
    }
    void saveRestoresEveryPhaseAndRejectsDamage() {
        Game g(47); g.newMatch(4);
        for(int step=0;step<2000 && g.s.phase!=Phase::MatchOver;++step) {
            Game copy(1); QVERIFY(copy.restore(g.save())); QCOMPARE(copy.save(),g.save());
            auto bad=g.save(); bad["format"]=1; QVERIFY(!copy.restore(bad));
            if(g.s.phase==Phase::HoleOver)g.nextHole(); else g.computerStep();
        }
        QVERIFY(g.s.phase==Phase::MatchOver);
        Game copy; QVERIFY(copy.restore(g.save()));
        auto bad=g.save(); auto hands=bad["hands"].toArray(); auto hand=hands[0].toArray(); hand[0]=hand[1]; hands[0]=hand; bad["hands"]=hands;
        QVERIFY(!copy.restore(bad)); QCOMPARE(copy.save(),g.save());
    }
    void completeMatchesAndConserveCards() {
        for(int opponents=1;opponents<=4;++opponents) for(unsigned seed=0;seed<12;++seed) {
            Game g(seed); g.newMatch(opponents);
            int steps=0;
            while(g.s.phase!=Phase::MatchOver && ++steps<20000) {
                if(g.s.phase==Phase::HoleOver) QVERIFY(g.nextHole()); else g.computerStep();
                QVERIFY(conserved(g));
                if(g.s.phase==Phase::Draw || g.s.phase==Phase::Place) for(int p=0;p<g.players();++p) QCOMPARE(g.hidden(p),1);
                if(g.s.phase==Phase::HoleOver || g.s.phase==Phase::MatchOver) {
                    QVERIFY2(g.s.turns[g.s.endingBy]>=6,"Computer ended before developing its hand");
                    QVERIFY2(g.s.scores.back()[g.s.endingBy]<=6,"Computer voluntarily ended on an unreasonable high score");
                }
            }
            QVERIFY2(steps<20000,"A match failed to terminate"); QCOMPARE(g.s.scores.size(),size_t(9));
            for(int p=0;p<g.players();++p) { int sum=0;for(auto row:g.s.scores)sum+=row[p];QCOMPARE(g.total(p),sum); }
            QVERIFY(!g.nextHole());
        }
    }
    void arbitraryRoundCountsEndAtSelectedTotal() {
        for(int count:{1,3,7,12,25,99}) {
            Game g(82); g.newMatch(2,count); QCOMPARE(g.s.holes,count);
            for(int round=1;round<=count;++round) {
                while(g.s.phase==Phase::Opening)g.computerStep();
                QVERIFY(g.draw(false)); QVERIFY(g.finishHole());
                QCOMPARE(g.s.scores.size(),size_t(round));
                Game restored; QVERIFY(restored.restore(g.save()));
                if(round<count) { QCOMPARE(g.s.phase,Phase::HoleOver); QVERIFY(g.nextHole()); }
                else { QCOMPARE(g.s.phase,Phase::MatchOver); QVERIFY(!g.nextHole()); }
            }
        }
    }
    void changingRoundCountKeepsCompletedScores() {
        Game g;g.newMatch(1,3);start(g);QVERIFY(g.draw(false));QVERIFY(g.finishHole());
        auto scores=g.state().scores;
        QVERIFY(g.setRounds(1));QCOMPARE(g.state().phase,Phase::MatchOver);
        QVERIFY(!g.setRounds(0));QVERIFY(!g.setRounds(100));
        QVERIFY(g.setRounds(5));QCOMPARE(g.state().phase,Phase::HoleOver);QCOMPARE(g.state().scores,scores);
        QVERIFY(g.nextHole());QVERIFY(!g.setRounds(1));QVERIFY(g.setRounds(2));QCOMPARE(g.state().phase,Phase::Opening);
        Game restored;QVERIFY(restored.restore(g.save()));QCOMPARE(restored.state().holes,2);
    }
    void uiRoundCountIsAvailableInSettings() {
        Window w(false);w.show();
        QTimer::singleShot(50,[&]{
            auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
            auto *rounds=dialog->findChild<QSpinBox *>("settingsRoundCount");QVERIFY(rounds);rounds->setValue(17);
            QTest::mouseClick(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        auto *action=w.findChild<QAction *>("roundSettings");QVERIFY(action);action->trigger();
        QCOMPARE(w.game().state().holes,17);QCOMPARE(w.findChild<QTableWidget *>("scores")->rowCount(),17);
    }
    void uiSevenCardsDrawAndFinalReveal() {
        QSettings().setValue("opponents",1); QSettings().setValue("fast",true);
        Window w(false); w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto card=[&](int i){return w.findChild<CardButton *>(QString("card%1").arg(i));};
        for(int i=0;i<7;++i) { QVERIFY(card(i)); QTest::mouseClick(card(i),Qt::LeftButton); }
        QTRY_COMPARE(w.game().state().phase,Phase::Draw); QCOMPARE(w.game().hidden(0),1);
        QVERIFY(card(7)->isEnabled());
        auto *stock=w.findChild<CardButton *>("stock"); QVERIFY(stock->isEnabled()); QTest::mouseClick(stock,Qt::LeftButton);
        auto *finish=w.findChild<QPushButton *>("finishHole"); QVERIFY(finish->isVisible()); QVERIFY(card(7)->isEnabled());
        QTest::qWait(30); w.capture(QDir::currentPath()+"/in-play.png");
        QTest::mouseClick(card(7),Qt::LeftButton); QCOMPARE(w.game().state().phase,Phase::HoleOver);
        for(int p=0;p<w.game().players();++p)QCOMPARE(w.game().hidden(p),0);
        QTest::mouseClick(w.findChild<QPushButton *>("nextHole"),Qt::LeftButton); QCOMPARE(w.game().state().phase,Phase::Opening); QCOMPARE(w.game().hidden(0),8);
    }
    void uiPlayerSelectionAndCompassSeats() {
        for(int players=2;players<=4;++players) {
            Window w(false);w.show();
            QTimer::singleShot(50,[&]{
                auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);
                auto *count=dialog->findChild<QComboBox *>("playerCount");QVERIFY(count);QCOMPARE(count->count(),3);QCOMPARE(count->findData(0),-1);count->setCurrentIndex(count->findData(players-1));
                auto *rounds=dialog->findChild<QSpinBox *>("roundCount");QVERIFY(rounds);rounds->setValue(7);
                QTest::mouseClick(dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
            });
            auto *settingsAction=w.findChild<QAction *>("playerSettings");QVERIFY(settingsAction);settingsAction->trigger();
            QCOMPARE(w.game().players(),players);QCOMPARE(w.game().state().holes,7);
            for(int i=0;i<7;++i)QTest::mouseClick(w.findChild<CardButton *>(QString("card%1").arg(i)),Qt::LeftButton);
            QTRY_COMPARE(w.game().state().phase,Phase::Draw);
            auto *south=w.findChild<CardButton *>("card0");auto *stock=w.findChild<CardButton *>("stock");
            QCOMPARE(south->orientation(),0);QVERIFY(south->mapToGlobal(QPoint(0,0)).y()>stock->mapToGlobal(QPoint(0,0)).y());
            QCOMPARE(w.findChild<QWidget *>("northSeat")->isVisible(),players>1);
            QCOMPARE(w.findChild<QWidget *>("westSeat")->isVisible(),players>=3);
            QCOMPARE(w.findChild<QWidget *>("eastSeat")->isVisible(),players>=4);
            for(int p=1;p<players;++p) {
                auto *first=w.findChild<CardButton *>(QString("opponent%1card0").arg(p));
                auto *partner=w.findChild<CardButton *>(QString("opponent%1card4").arg(p));QVERIFY(first);QVERIFY(partner);
                const int orientation=players==2?180:p==1?90:players==4 && p==3?270:180;
                QCOMPARE(first->orientation(),orientation);
                auto point=first->mapToGlobal(QPoint(0,0)),pair=partner->mapToGlobal(QPoint(0,0)),center=stock->mapToGlobal(QPoint(0,0));
                if(orientation==180){QVERIFY(point.y()<center.y());QCOMPARE(point.x(),pair.x());QVERIFY(point.y()>pair.y());}
                else{QCOMPARE(point.y(),pair.y());if(orientation==90){QVERIFY(point.x()<center.x());QVERIFY(point.x()>pair.x());}else{QVERIFY(point.x()>center.x());QVERIFY(point.x()<pair.x());}}
            }
            w.capture(QDir::currentPath()+QString("/compass-%1.png").arg(players));
            // South can finish by flipping the original card, with no draw.
            auto deck=w.game().state().deck,discards=w.game().state().discards;
            int original=w.game().state().hands[0][7].card;
            QTest::mouseClick(w.findChild<CardButton *>("card7"),Qt::LeftButton);
            QCOMPARE(w.game().state().phase,Phase::HoleOver);QCOMPARE(w.game().state().deck,deck);QCOMPARE(w.game().state().discards,discards);
            QCOMPARE(w.game().state().hands[0][7].card,original);QCOMPARE(w.game().hidden(0),0);
        }
    }

};
}
QTEST_MAIN(Golf::GameTest)
#include "game_test.moc"
