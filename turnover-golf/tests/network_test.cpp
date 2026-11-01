#include "network.h"
#include "window.h"
#include <QtTest>
#include <QAction>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QDialog>

namespace Golf {
class NetworkTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    static void sync(NetworkSession &host,NetworkSession &guest) {
        QTRY_COMPARE(guest.m_revision,host.m_revision);
        QVERIFY(!guest.pending());
    }
    static void openSeven(NetworkSession &player,NetworkSession &host) {
        for(int i=0;i<7;++i) {
            QVERIFY(player.act("open",i));
            if(&player!=&host)QTRY_VERIFY(!player.pending());
        }
    }
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName("TurnoverNetworkTests");
        QCoreApplication::setApplicationName("TurnoverNetworkTests");
        QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,settings.path());
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    }
    void publicViewsContainOnlyRevealedInformation() {
        Game g(56),view;
        auto opening=g.publicView(1);
        QVERIFY(!opening.contains("deck"));QVERIFY(!opening.contains("random"));QVERIFY(!opening.contains("turns"));
        for(auto hand:opening["hands"].toArray())for(auto card:hand.toArray())QVERIFY(card.isNull());
        QVERIFY(view.restorePublicView(opening,1));
        while(g.state().phase==Phase::Opening)g.computerStep();
        QVERIFY(g.draw(false));
        QVERIFY(g.publicView(1)["drawn"].isNull());
        QCOMPARE(g.publicView(0)["drawn"].toInt(),value(g.state().held));
        QVERIFY(view.restorePublicView(g.publicView(1),1));
        QCOMPARE(view.state().held,-2);
        for(auto card:view.state().deck)QCOMPARE(card,-1);
        for(int p=0;p<g.players();++p)for(int i=0;i<8;++i) {
            if(!g.state().hands[p][i].up)QCOMPARE(view.state().hands[p][i].card,-1);
            QCOMPARE(view.visibleValue(p,i),g.visibleValue(p,i));
        }
        auto before=view.publicView(1),bad=g.publicView(1);
        bad["drawn"]=12;QVERIFY(!view.restorePublicView(bad,1));QCOMPARE(view.publicView(1),before);
        bad=g.publicView(1);bad["stockCount"]=109;QVERIFY(!view.restorePublicView(bad,1));
        bad=g.publicView(1);bad["phase"]=3;QVERIFY(!view.restorePublicView(bad,1));
        bad=g.publicView(1);bad["discardCount"]=0;QVERIFY(!view.restorePublicView(bad,1));
        QVERIFY(g.finishHole());QVERIFY(view.restorePublicView(g.publicView(1),1));
        for(int p=0;p<g.players();++p)QCOMPARE(view.hidden(p),0);
        QCOMPARE(view.state().scores,g.state().scores);
    }
    void mixedHumansAndComputerCompleteSelectedRounds() {
        Game hg(6),ag,bg;NetworkSession h(hg),a(ag),b(bg);
        QVERIFY(h.host({true,false,true},3,"Host",0));
        a.join("127.0.0.1",h.port(),h.roomCode(),"Alice");
        QTRY_COMPARE(a.localSeat(),1);QVERIFY(!h.ready());QVERIFY(!a.ready());QVERIFY(!h.act("open",0));
        b.join("127.0.0.1",h.port(),h.roomCode(),"Bob");
        QTRY_VERIFY(h.ready() && a.ready() && b.ready());QCOMPARE(b.localSeat(),3);
        QVERIFY(h.isComputer(2));QVERIFY(!h.isComputer(1));
        QCOMPARE(hg.name(1),QString("Alice"));QCOMPARE(ag.name(3),QString("Bob"));
        for(int round=1;round<=3;++round) {
            openSeven(h,h);sync(h,a);openSeven(a,h);
            while(hg.state().phase==Phase::Opening && hg.state().current==2)h.computerStep();
            sync(h,b);openSeven(b,h);sync(h,a);sync(h,b);
            QCOMPARE(hg.state().phase,Phase::Draw);
            for(int p=0;p<4;++p)QCOMPARE(hg.hidden(p),1);
            if(round==1) {
                QVERIFY(!a.act("draw"));
                QVERIFY(h.act("draw"));sync(h,a);sync(h,b);
                QCOMPARE(ag.state().held,-2);QCOMPARE(bg.state().held,-2);
                QVERIFY(h.act("replace",0));sync(h,a);
                QVERIFY(a.act("draw"));QTRY_VERIFY(!a.pending());
                QCOMPARE(value(ag.state().held),value(hg.state().held));
                sync(h,b);QCOMPARE(bg.state().held,-2);
                QVERIFY(a.act("replace",7));QTRY_VERIFY(!a.pending());
            } else if(round==2) {
                QVERIFY(a.act("take"));QTRY_VERIFY(!a.pending());
                QVERIFY(a.act("finish"));QTRY_VERIFY(!a.pending());
            } else {
                // A computer is the starting player of round 3; normal host AI drives it.
                h.computerStep();
                if(hg.state().phase==Phase::Place)h.computerStep();
                if(hg.active()) {
                    sync(h,b);QCOMPARE(hg.state().current,3);
                    QVERIFY(b.act("draw"));QTRY_VERIFY(!b.pending());
                    QVERIFY(b.act("finish"));QTRY_VERIFY(!b.pending());
                }
            }
            sync(h,a);sync(h,b);
            QCOMPARE(hg.state().scores.size(),size_t(round));
            QCOMPARE(ag.state().scores,hg.state().scores);QCOMPARE(bg.state().scores,hg.state().scores);
            for(int p=0;p<4;++p) {QCOMPARE(hg.hidden(p),0);QCOMPARE(ag.hidden(p),0);QCOMPARE(bg.hidden(p),0);}
            QVERIFY(!a.act("next"));
            if(round<3){QVERIFY(h.act("next"));sync(h,a);sync(h,b);}
        }
        QCOMPARE(hg.state().phase,Phase::MatchOver);QCOMPARE(ag.state().phase,Phase::MatchOver);
        QVERIFY(!h.act("next"));
    }
    void fourOnlineOpponentsHaveIndependentSeats() {
        Game hg;NetworkSession h(hg);QVERIFY(h.host({true,true,true,true},1,"Host",0));
        std::array<Game,4> games;
        std::vector<std::unique_ptr<NetworkSession>> clients;
        for(int i=0;i<4;++i) {
            clients.emplace_back(std::make_unique<NetworkSession>(games[i]));
            clients.back()->join("127.0.0.1",h.port(),h.roomCode(),QString("Player %1").arg(i+1));
            QTRY_COMPARE(clients.back()->localSeat(),i+1);
        }
        QTRY_VERIFY(h.ready());
        openSeven(h,h);
        for(auto &client:clients){sync(h,*client);openSeven(*client,h);}
        QVERIFY(h.act("draw"));QVERIFY(h.act("finish"));
        for(int i=0;i<4;++i){sync(h,*clients[i]);QCOMPARE(games[i].players(),5);QCOMPARE(games[i].state().phase,Phase::MatchOver);QCOMPARE(games[i].state().scores,hg.state().scores);}
    }
    void onlineHumanFlipsFinalCardWithoutDrawing() {
        Game hg,ag;NetworkSession h(hg),a(ag);QVERIFY(h.host({true,false,false},3,"Host",0));
        a.join("127.0.0.1",h.port(),h.roomCode(),"Alice");QTRY_VERIFY(a.ready());
        openSeven(h,h);sync(h,a);openSeven(a,h);
        while(hg.state().phase==Phase::Opening)h.computerStep();
        QVERIFY(h.act("draw"));QVERIFY(h.act("discard"));sync(h,a);
        const auto deck=hg.state().deck,discards=hg.state().discards;int original=hg.state().hands[1][7].card;
        QVERIFY(a.act("finish"));QTRY_VERIFY(!a.pending());QCOMPARE(hg.state().phase,Phase::HoleOver);
        QCOMPARE(hg.state().deck,deck);QCOMPARE(hg.state().discards,discards);QCOMPARE(hg.state().hands[1][7].card,original);
        QCOMPARE(ag.state().scores,hg.state().scores);for(int p=0;p<4;++p)QCOMPARE(ag.hidden(p),0);
    }
    void recycledDeckRemainsPlayableOnline() {
        Game hg,ag;NetworkSession h(hg),a(ag);QVERIFY(h.host({true},1,"Host",0));
        a.join("127.0.0.1",h.port(),h.roomCode(),"Alice");QTRY_VERIFY(a.ready());
        openSeven(h,h);sync(h,a);openSeven(a,h);
        // Put all but one stock card in the used pile without changing card conservation.
        auto saved=hg.save();auto deck=saved["deck"].toArray(),discards=saved["discards"].toArray();
        while(deck.size()>1)discards.append(deck.takeAt(0));
        saved["deck"]=deck;saved["discards"]=discards;QVERIFY(hg.restore(saved));h.publish();
        QVERIFY(h.act("draw"));QVERIFY(h.act("replace",0));sync(h,a);
        QVERIFY(ag.state().deck.empty());QVERIFY(ag.state().discards.size()>1);
        int top=value(hg.state().discards.back());
        QVERIFY(a.act("draw"));QTRY_VERIFY(!a.pending());
        QVERIFY(!ag.state().deck.empty());QCOMPARE(ag.state().discards.size(),size_t(1));
        QCOMPARE(value(ag.state().discards.back()),top);QCOMPARE(hg.hidden(0),1);QCOMPARE(hg.hidden(1),1);
        QVERIFY(hg.state().scores.empty());QVERIFY(a.act("discard"));QTRY_VERIFY(!a.pending());
        QCOMPARE(ag.state().phase,Phase::Draw);QVERIFY(hg.state().scores.empty());
    }
    void disconnectPausesAndReconnectPreservesDraw() {
        Game hg,ag;NetworkSession h(hg),a(ag);
        QVERIFY(h.host({true},2,"Host",0));a.join("127.0.0.1",h.port(),h.roomCode(),"Alice");
        QTRY_VERIFY(a.ready());openSeven(h,h);sync(h,a);openSeven(a,h);
        QVERIFY(h.act("draw"));QVERIFY(h.act("discard"));sync(h,a);
        QVERIFY(a.act("draw"));QTRY_VERIFY(!a.pending());int held=hg.state().held;auto before=hg.save();auto token=a.m_token;
        a.m_client->abort();QTRY_VERIFY(!h.ready());QVERIFY(!h.act("finish"));QVERIFY(!a.act("finish"));
        Game strangerGame;NetworkSession stranger(strangerGame);stranger.join("127.0.0.1",h.port(),h.roomCode(),"New player");
        QTRY_VERIFY(!stranger.errorString().isEmpty());QVERIFY(!h.ready());QCOMPARE(hg.save(),before);
        a.reconnect();QTRY_VERIFY(a.ready() && h.ready());
        QCOMPARE(a.localSeat(),1);QCOMPARE(a.m_token,token);QCOMPARE(hg.state().held,held);QCOMPARE(hg.save(),before);
        QCOMPARE(value(ag.state().held),value(held));
        QVERIFY(a.act("finish"));QTRY_COMPARE(hg.state().phase,Phase::HoleOver);sync(h,a);
    }
    void onlyHostCanChangeRoundsAndAllGuestsUpdate() {
        Game hg,ag;NetworkSession h(hg),a(ag);QVERIFY(h.host({true},2,"Host",0));
        QVERIFY(h.act("rounds",4));QCOMPARE(hg.state().holes,4);
        a.join("127.0.0.1",h.port(),h.roomCode(),"Alice");QTRY_VERIFY(a.ready());QCOMPARE(ag.state().holes,4);
        QVERIFY(!a.act("cpuTarget",6));QVERIFY(h.act("cpuTarget",-10));sync(h,a);QCOMPARE(ag.state().rules.cpuTarget,-10);
        QVERIFY(!a.act("rounds",99));QVERIFY(h.act("rounds",7));sync(h,a);QCOMPARE(ag.state().holes,7);
        openSeven(h,h);sync(h,a);openSeven(a,h);QVERIFY(h.act("draw"));QVERIFY(h.act("finish"));sync(h,a);
        auto scores=hg.state().scores;QVERIFY(h.act("rounds",1));sync(h,a);QCOMPARE(ag.state().phase,Phase::MatchOver);
        QVERIFY(h.act("rounds",3));sync(h,a);QCOMPARE(ag.state().phase,Phase::HoleOver);QCOMPARE(ag.state().scores,scores);
    }
    void unauthorizedAndMalformedRequestsCannotChangeGame() {
        Game hg,ag;NetworkSession h(hg),a(ag);QVERIFY(h.host({true},2,"Host",0));
        Game wrongGame;NetworkSession wrong(wrongGame);wrong.join("127.0.0.1",h.port(),"WRONGROOM123","Wrong");
        QTRY_VERIFY(!wrong.errorString().isEmpty());QVERIFY(!h.ready());
        a.join("127.0.0.1",h.port(),h.roomCode(),"Alice");QTRY_VERIFY(a.ready());
        auto before=hg.save();QSignalSpy errors(&a,&NetworkSession::error);
        h.send(a.m_client,{{"type","action"},{"revision",h.m_revision},{"action","open"},{"argument",0},{"seat",0}});
        QTRY_COMPARE(errors.count(),1);QCOMPARE(hg.save(),before);
        openSeven(h,h);sync(h,a);before=hg.save();
        h.send(a.m_client,{{"type","action"},{"revision",h.m_revision-1},{"action","open"},{"argument",0}});
        QTRY_COMPARE(errors.count(),2);QCOMPARE(hg.save(),before);
        h.send(a.m_client,{{"type","action"},{"revision",h.m_revision},{"action","open"},{"argument",99}});
        QTRY_COMPARE(errors.count(),3);QCOMPARE(hg.save(),before);
        a.m_client->write("{invalid json}\n");QTRY_VERIFY(!h.ready());QCOMPARE(hg.save(),before);
        QTcpSocket oversized;oversized.connectToHost("127.0.0.1",h.port());QTRY_COMPARE(oversized.state(),QAbstractSocket::ConnectedState);
        oversized.write(QByteArray(65538,'x'));QTRY_COMPARE(oversized.state(),QAbstractSocket::UnconnectedState);QCOMPARE(hg.save(),before);
    }
    void nativeSetupAndGuestHandWorkOverTcp() {
        Window host(false),guest(false);host.show();guest.show();
        QTcpServer reserve;QVERIFY(reserve.listen(QHostAddress::LocalHost,0));int port=reserve.serverPort();reserve.close();
        QTimer::singleShot(50,[&]{
            auto *d=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(d);
            auto *count=d->findChild<QComboBox *>("playerCount");count->setCurrentIndex(count->findData(3));
            auto *seat=d->findChild<QComboBox *>("seatType1");QVERIFY(seat);QCOMPARE(seat->itemText(1),QString("Online human"));seat->setCurrentIndex(1);
            d->findChild<QSpinBox *>("hostPort")->setValue(port);d->findChild<QSpinBox *>("roundCount")->setValue(3);
            d->findChild<QLineEdit *>("hostName")->setText("Host");
            d->grab().save(QDir::currentPath()+"/online-setup.png");
            QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        host.findChild<QAction *>("gameOptions")->trigger();
        QCOMPARE(host.session().mode(),NetworkSession::Mode::Host);QVERIFY(!host.session().ready());
        QTimer::singleShot(50,[&]{
            auto *d=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(d);
            d->findChild<QTabWidget *>("setupTabs")->setCurrentIndex(4);
            d->findChild<QLineEdit *>("joinAddress")->setText("127.0.0.1");
            d->findChild<QSpinBox *>("joinPort")->setValue(host.session().port());
            d->findChild<QLineEdit *>("joinCode")->setText(host.session().roomCode());
            d->findChild<QLineEdit *>("joinName")->setText("Guest");
            QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        guest.findChild<QAction *>("gameOptions")->trigger();
        QTRY_VERIFY(guest.session().ready());QCOMPARE(guest.session().localSeat(),1);
        QVERIFY(guest.findChild<QAction *>("gameOptions"));
        for(int i=0;i<7;++i)QTest::mouseClick(host.findChild<CardButton *>(QString("card%1").arg(i)),Qt::LeftButton);
        sync(host.session(),guest.session());
        for(int i=0;i<7;++i) {
            auto *card=guest.findChild<CardButton *>(QString("card%1").arg(i));QVERIFY(card->isEnabled());QTest::mouseClick(card,Qt::LeftButton);
            QTRY_COMPARE(host.game().hidden(1),7-i);
            sync(host.session(),guest.session());
        }
        QVERIFY(guest.findChild<CardButton *>("opponent0card0"));QVERIFY(!guest.findChild<CardButton *>("opponent1card0"));
        QCOMPARE(guest.game().players(),4);QCOMPARE(guest.findChild<CardButton *>("card0")->orientation(),0);
        QCOMPARE(guest.findChild<CardButton *>("opponent0card0")->orientation(),270);
        QCOMPARE(guest.findChild<CardButton *>("opponent2card0")->orientation(),90);
        QCOMPARE(guest.findChild<CardButton *>("opponent3card0")->orientation(),180);
        QTRY_COMPARE(host.game().state().phase,Phase::Draw);
        QVERIFY(host.session().act("draw"));QVERIFY(host.session().act("discard"));sync(host.session(),guest.session());
        QTest::mouseClick(guest.findChild<CardButton *>("stock"),Qt::LeftButton);QTRY_VERIFY(!guest.session().pending());
        QTRY_COMPARE(host.game().state().phase,Phase::Place);
        guest.capture(QDir::currentPath()+"/online-guest.png");
        QTest::mouseClick(guest.findChild<CardButton *>("card7"),Qt::LeftButton);
        QTRY_COMPARE(host.game().state().phase,Phase::HoleOver);sync(host.session(),guest.session());
        QCOMPARE(guest.game().state().scores,host.game().state().scores);
        QVERIFY(!guest.findChild<QPushButton *>("nextHole")->isEnabled());
        QTest::mouseClick(host.findChild<QPushButton *>("nextHole"),Qt::LeftButton);sync(host.session(),guest.session());
        QCOMPARE(guest.game().state().hole,2);QCOMPARE(guest.game().hidden(1),8);
    }
    void selectedLayoutsDecksAndRulesSynchronize() {
        for(auto shape:{std::pair{3,3},std::pair{4,4}})for(auto matching:{Matching::Columns,Matching::Pairs})for(bool cancelRows:{false,true}) {
            Rules rules{shape.first,shape.second,3,matching,0,cancelRows};
            Game hg,ag;NetworkSession h(hg),a(ag);QVERIFY(h.host({true},2,"Host",0,rules));
            a.join("127.0.0.1",h.port(),h.roomCode(),"Guest");QTRY_VERIFY(a.ready());
            QCOMPARE(ag.state().rules.cardCount(),rules.cardCount());QCOMPARE(ag.state().rules.decks,3);
            QCOMPARE(ag.state().rules.matching,matching);
            QCOMPARE(ag.state().rules.cancelRows,cancelRows);
            for(int i=0;i<rules.cardCount()-1;++i)QVERIFY(h.act("open",i));sync(h,a);
            for(int i=0;i<rules.cardCount()-1;++i){QVERIFY(a.act("open",i));QTRY_VERIFY(!a.pending());}
            QVERIFY(h.act("draw"));QVERIFY(h.act("discard"));sync(h,a);
            QVERIFY(a.act("draw"));QTRY_VERIFY(!a.pending());
            QVERIFY(a.act("replace",rules.cardCount()-1));QTRY_VERIFY(!a.pending());sync(h,a);
            QCOMPARE(ag.state().scores,hg.state().scores);QCOMPARE(hg.state().phase,Phase::HoleOver);
            QVERIFY(h.act("next"));sync(h,a);QCOMPARE(ag.hidden(1),rules.cardCount());
            QCOMPARE(ag.state().rules.cancelRows,cancelRows);
        }
    }

    void guestRowOptionIsReadOnlyAndMatchesHost() {
        Window host(false),guest(false);Rules rules{3,3,2,Matching::Columns,0,true};
        QVERIFY(host.session().host({true},2,"Host",0,rules));
        guest.session().join("127.0.0.1",host.session().port(),host.session().roomCode(),"Guest");
        QTRY_VERIFY(guest.session().ready());const auto before=host.game().save();
        QTimer::singleShot(30,[&]{
            auto *d=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(d);
            auto *rows=d->findChild<QCheckBox *>("cancelRows");QVERIFY(rows);QVERIFY(rows->isChecked());QVERIFY(!rows->isEnabled());
            QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
        });
        guest.findChild<QAction *>("gameOptions")->trigger();
        QCOMPARE(guest.session().mode(),NetworkSession::Mode::Client);
        QCOMPARE(host.game().save(),before);QVERIFY(guest.game().state().rules.cancelRows);
    }

};
}
QTEST_MAIN(Golf::NetworkTest)
#include "network_test.moc"
