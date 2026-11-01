#include "window.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QMenuBar>
#include <QMessageBox>
#include <QSaveFile>
#include <QScrollArea>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QShortcut>
#include <QStandardPaths>
#include <QStatusBar>
#include <algorithm>
using namespace Golf;

namespace {
QLabel *label(const QString &text, const QString &name={}, QWidget *parent=nullptr) {
    auto *l=new QLabel(text,parent); if(name=="lastMove" || name=="instruction" || name=="computerLabel" || name=="statusTitle" || name=="result")l->setTextFormat(Qt::PlainText); if(!name.isEmpty()) l->setObjectName(name); return l;
}
void clearLayout(QLayout *layout) {
    while(auto *item=layout->takeAt(0)) { if(item->widget()) delete item->widget(); if(item->layout()) clearLayout(item->layout()); delete item; }
}
}
Window::Window(bool persist,QWidget *parent):QMainWindow(parent),net(g,this),persistence(persist) {
    Q_INIT_RESOURCE(resources);
    setObjectName("mainWindow"); setWindowTitle("Turnover Golf");
    setWindowIcon(QIcon(":/turnover-golf.svg")); resize(1250,820); setMinimumSize(1100,730);
    QSettings settings;
    preferredOpponents=std::clamp(settings.value("opponents",1).toInt(),1,3);
    preferredRounds=std::clamp(settings.value("rounds",9).toInt(),1,99);
    preferredRules={settings.value("columns",4).toInt(),settings.value("rows",2).toInt(),
        settings.value("decks",1).toInt(),Matching(settings.value("matching",0).toInt()),settings.value("cpuTarget",0).toInt(),settings.value("cancelRows",false).toBool()};
    if(!preferredRules.valid())preferredRules=Rules{};
    fastTurns=settings.value("fast",false).toBool();
    faceTheme=std::clamp(settings.value("faceTheme",0).toInt(),0,11);backTheme=std::clamp(settings.value("backTheme",0).toInt(),0,11);
    for(int i=0;i<3;++i)preferredRemote.append(settings.value(QString("onlineSeat%1").arg(i+1),false).toBool());
    preferredName=settings.value("onlineName","Host").toString();
    preferredPort=quint16(std::clamp(settings.value("onlinePort",45839).toInt(),1024,65535));
    g.newMatch(preferredOpponents,preferredRounds,preferredRules);
    savePath=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/game.json";
    bool resumed=false;
    if(persistence) {
        QFile file(savePath); if(file.open(QIODevice::ReadOnly) && file.size()<1000000) resumed=g.restore(QJsonDocument::fromJson(file.readAll()).object());
    }
    buildUi(); buildOpponents();
    aiTimer.setSingleShot(true); aiTimer.setInterval(650);
    connect(&aiTimer,&QTimer::timeout,&net,&NetworkSession::computerStep);
    connect(&net,&NetworkSession::updated,this,&Window::changed);
    connect(&net,&NetworkSession::error,this,[this](const QString &message){ statusBar()->showMessage(message,12000); });
    updateUi();
    if(persistence && settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray());
    statusBar()->showMessage(resumed?"Resumed your saved game.":"Leave one card face down. Change players, layout, matching and decks in Settings → Options.",12000);
}
void Window::buildUi() {
    auto *gameMenu=menuBar()->addMenu("&Game");
    auto *newAction=gameMenu->addAction("&New game",QKeySequence::New,this,&Window::newGame);
    newAction->setObjectName("newGameAction");
    newAction->setIcon(QIcon::fromTheme("document-new"));
    gameMenu->addAction("Join &online game…",this,[this]{showGameSettings(true);});
    gameMenu->addAction("&Quit",QKeySequence::Quit,this,&QWidget::close);
    auto *settings=menuBar()->addMenu("&Settings");
    auto *optionsAction=settings->addAction("&Options…",this,[this]{showGameSettings();});optionsAction->setObjectName("gameOptions");
    auto *help=menuBar()->addMenu("&Help");
    help->addAction("&How to play",QKeySequence::HelpContents,this,&Window::showRules);
    help->addAction("&About Turnover Golf",this,[this]{ QMessageBox::about(this,"About Turnover Golf",QString("<h2>Turnover Golf %1</h2><p>A native Qt game for KDE, using Play Nine card values and scoring, with your house rules.</p><p>Original card artwork. Choose 2–4 total players, including you. Other seats can be computers or online humans.</p><p>All but one card start face up. Flip or replace your final card to end the round; everyone reveals together.</p>").arg(GOLF_VERSION)); });
    auto *root=new QWidget; root->setObjectName("surface"); auto *outer=new QVBoxLayout(root); outer->setContentsMargins(24,15,24,16); outer->setSpacing(14);
    auto *top=new QHBoxLayout;
    auto *brand=new QVBoxLayout; brand->setSpacing(0); brand->addWidget(label("TURNOVER GOLF","brand")); brand->addWidget(label("Lowest score wins.","subtitle")); top->addLayout(brand); top->addStretch();
    heading=label("","holeBadge"); top->addWidget(heading); top->addSpacing(12);
    auto *newButton=new QPushButton("New game"); newButton->setObjectName("newGame"); connect(newButton,&QPushButton::clicked,this,&Window::newGame); top->addWidget(newButton);
    auto *rules=new QPushButton("How to play"); connect(rules,&QPushButton::clicked,this,&Window::showRules); top->addWidget(rules); outer->addLayout(top);
    auto *connectionRow=new QHBoxLayout; connectionLabel=label("","connectionStatus"); connectionLabel->setWordWrap(true); connectionLabel->setTextFormat(Qt::PlainText); connectionRow->addWidget(connectionLabel,1); connectionButton=new QPushButton("Connection details"); connectionButton->setObjectName("connectionDetails"); connect(connectionButton,&QPushButton::clicked,this,&Window::showConnection); connectionRow->addWidget(connectionButton); outer->addLayout(connectionRow);
    auto *body=new QHBoxLayout; body->setSpacing(18);
    auto *table=new QWidget;table->setObjectName("table");tableGrid=new QGridLayout(table);tableGrid->setContentsMargins(18,8,18,8);tableGrid->setHorizontalSpacing(12);tableGrid->setVerticalSpacing(6);tableGrid->setColumnStretch(1,1);
    opponentArea=new QWidget;opponentArea->setObjectName("northSeat");opponents=new QHBoxLayout(opponentArea);opponents->setContentsMargins(0,0,0,0);opponents->setSpacing(16);tableGrid->addWidget(opponentArea,0,0,1,3,Qt::AlignHCenter);
    westArea=new QWidget;westArea->setObjectName("westSeat");westOpponents=new QVBoxLayout(westArea);westOpponents->setContentsMargins(0,0,0,0);tableGrid->addWidget(westArea,1,0,Qt::AlignCenter);
    eastArea=new QWidget;eastArea->setObjectName("eastSeat");eastOpponents=new QVBoxLayout(eastArea);eastOpponents->setContentsMargins(0,0,0,0);tableGrid->addWidget(eastArea,1,2,Qt::AlignCenter);
    auto *centerPanel=new QWidget;centerPanel->setObjectName("centerPiles");auto *controlsColumn=new QVBoxLayout(centerPanel);controlsColumn->setContentsMargins(0,0,0,0);controlsColumn->setSpacing(9);
    auto *center=new QHBoxLayout;center->setSpacing(16);center->addStretch();
    auto pile=[&](CardButton *&card,QLabel *&title,const QString &text,const QString &name){
        auto *box=new QVBoxLayout;box->setSpacing(3);title=label(text,"pileLabel");title->setAlignment(Qt::AlignCenter);box->addWidget(title);
        card=new CardButton;card->setObjectName(name);card->setFixedSize(56,78);box->addWidget(card,0,Qt::AlignHCenter);center->addLayout(box);
    };
    QLabel *discardLabel,*heldLabel;
    pile(stock,deckLabel,"DRAW PILE","stock");pile(discard,discardLabel,"DISCARD","discard");pile(held,heldLabel,"YOUR DRAW","held");held->setEnabled(false);
    connect(stock,&QAbstractButton::clicked,this,[this]{net.act("draw");});connect(discard,&QAbstractButton::clicked,this,[this]{net.act("take");});center->addStretch();controlsColumn->addLayout(center);
    auto *actionPanel=new QWidget;actionPanel->setObjectName("actionPanel");auto *actions=new QVBoxLayout(actionPanel);actions->setContentsMargins(10,8,10,8);actions->setSpacing(4);
    statusTitle=label("","statusTitle");statusTitle->setAlignment(Qt::AlignCenter);actions->addWidget(statusTitle);
    instruction=label("","instruction");instruction->setWordWrap(true);instruction->setAlignment(Qt::AlignCenter);instruction->setMinimumHeight(32);actions->addWidget(instruction);
    auto *actionRow=new QHBoxLayout;actionRow->setSpacing(6);actionRow->addStretch();
    discardButton=new QPushButton("Discard draw");discardButton->setObjectName("discardHeld");connect(discardButton,&QPushButton::clicked,this,[this]{net.act("discard");});actionRow->addWidget(discardButton);
    finishButton=new QPushButton("Flip final card");finishButton->setObjectName("finishHole");connect(finishButton,&QPushButton::clicked,this,[this]{net.act("finish");});actionRow->addWidget(finishButton);
    nextButton=new QPushButton("Next round →");nextButton->setObjectName("nextHole");connect(nextButton,&QPushButton::clicked,this,[this]{if(g.state().phase==Phase::MatchOver)newGame();else net.act("next");});actionRow->addWidget(nextButton);actionRow->addStretch();actions->addLayout(actionRow);
    controlsColumn->addWidget(actionPanel);tableGrid->addWidget(centerPanel,1,1,Qt::AlignVCenter);
    auto *south=new QWidget;south->setObjectName("southSeat");auto *playerColumn=new QVBoxLayout(south);playerColumn->setContentsMargins(0,0,0,0);playerColumn->setSpacing(3);
    auto *you=label("YOUR CARDS","playerHeading");you->setAlignment(Qt::AlignCenter);playerColumn->addWidget(you);yourScore=label("","muted");yourScore->setAlignment(Qt::AlignCenter);playerColumn->addWidget(yourScore);
    handGrid=new QGridLayout;handGrid->setHorizontalSpacing(9);handGrid->setVerticalSpacing(3);
    const QString cardKeys="123456789ABCEFGH";
    for(int i=0;i<cardKeys.size();++i){
        auto *shortcut=new QShortcut(QKeySequence(QString(cardKeys[i])),this);
        connect(shortcut,&QShortcut::activated,this,[this,i]{if(i<int(hand.size()))clickCard(i);});
    }
    auto *cardsRow=new QHBoxLayout;cardsRow->addStretch();cardsRow->addLayout(handGrid);cardsRow->addStretch();playerColumn->addLayout(cardsRow);tableGrid->addWidget(south,2,0,1,3,Qt::AlignHCenter);
    auto *scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(table); body->addWidget(scroll,1);
    auto *sidebar=new QWidget; sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(304); auto *side=new QVBoxLayout(sidebar); side->setContentsMargins(17,16,17,16); side->setSpacing(13);
    side->addWidget(label("SCORECARD","sectionTitle")); totals=label("","totals"); totals->setWordWrap(true); side->addWidget(totals);
    scoreTable=new QTableWidget; scoreTable->setObjectName("scores"); scoreTable->setEditTriggers(QAbstractItemView::NoEditTriggers); scoreTable->setSelectionMode(QAbstractItemView::NoSelection); scoreTable->setFocusPolicy(Qt::NoFocus); scoreTable->verticalHeader()->hide(); scoreTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); scoreTable->setShowGrid(false); scoreTable->setMinimumHeight(140); side->addWidget(scoreTable,1);
    result=label("","result"); result->setWordWrap(true); side->addWidget(result);
    side->addWidget(label("LAST PLAY","sectionTitle")); lastMove=label("","lastMove"); lastMove->setWordWrap(true); lastMove->setMinimumHeight(62); side->addWidget(lastMove);
    auto *quick=label("<b>Match top to bottom</b><br>Equal cards in a column = 0.<br>−5 cards always keep their value.<br><br><b>Matching bonuses</b><br>2 / 3 / 4 same-number pairs:<br>−10 / −15 / −20 bonus.<br><br><b>Four −5 cards in two pairs = −30.</b>","quickRules"); quick->setWordWrap(true); side->addWidget(quick);
    auto *keys=label("KEYS   1–4 top row · 5–8 bottom<br>D draw · T take discard · X discard","keyboardHint"); keys->setWordWrap(true); side->addWidget(keys); body->addWidget(sidebar); outer->addLayout(body,1); setCentralWidget(root);
    for(int key:{Qt::Key_D,Qt::Key_T,Qt::Key_X}) { auto *s=new QShortcut(QKeySequence(key),this); connect(s,&QShortcut::activated,this,[this,key]{keyAction(key);}); }
    setStyleSheet(R"(
      QMenuBar { background: #263b32; color: #f1eddf; }
      QMenuBar::item:selected { background: #42654e; }
      QMenu { background: #263b32; color: #f1eddf; border: 1px solid #63836a; }
      QMenu::item:selected { background: #42654e; }
      QWidget#surface { background: #102b27; color: #efeadd; }
      QWidget#table { background: #1a4239; border-radius: 16px; }
      QWidget#sidebar { background: #16352f; border: 1px solid #36554a; border-radius: 13px; }
      QWidget#surface QLabel { color: #ede9dc; background: transparent; border: none; }
      QLabel#brand { font-size: 25px; font-weight: 700; letter-spacing: 2px; color: #f2d9a2; }
      QLabel#subtitle, QLabel#muted, QLabel#keyboardHint { color: #a5bdb0; font-size: 11px; }
      QLabel#holeBadge { background: #264b3e; border: 1px solid #628267; border-radius: 15px; padding: 8px 13px; color: #edd79c; font-weight: 600; }
      QLabel#playerHeading, QLabel#sectionTitle { font-size: 12px; font-weight: 700; letter-spacing: 1px; color: #cfbd8d; }
      QLabel#pileLabel, QLabel#columnLabel { font-size: 10px; color: #bed0c3; }
      QLabel#computerLabel { font-size: 12px; color: #dce7da; }
      QLabel#totals { font-size: 13px; line-height: 1.5; }
      QLabel#statusTitle { font-size: 18px; font-weight: 600; color: #f6e1ae; }
      QLabel#instruction { font-size: 12px; color: #dce7dc; }
      QLabel#lastMove, QLabel#quickRules { font-size: 12px; color: #c8d7ca; }
      QLabel#result { color: #eed69b; font-size: 15px; font-weight: 600; }
      QWidget#actionPanel { background: #12362d; border: 1px solid #40614d; border-radius: 10px; }
      QWidget#surface QPushButton { background: #264f43; color: #efe8d6; border: 1px solid #5e7c62; padding: 8px 14px; border-radius: 7px; font-size: 12px; }
      QWidget#surface QPushButton:hover { background: #38664e; border-color: #d4b979; }
      QWidget#surface QPushButton:focus { border: 2px solid #efd397; }
      QWidget#surface QPushButton:disabled { color: #7f9b8a; background: #233f35; border-color: #355548; }
      QWidget#surface QPushButton#nextHole { background: #d6bc7c; color: #183b30; font-weight: 700; }
      QTableWidget#scores { background: #16352f; alternate-background-color: #1b3e34; color: #f1eddf; border: none; font-size: 11px; }
      QTableWidget#scores QHeaderView::section { background: #24483b; color: #d5c48f; border: none; padding: 5px 0; font-size: 10px; }
      QTableWidget#scores QTableCornerButton::section { background: #24483b; }
      QScrollArea { background: transparent; border: none; }
      QFrame#separator { color: #416251; }
    )");
}
QString Window::playerLabel(int p) const {
    if(p==net.localSeat())return "You";
    return net.online()?g.name(p):QString("CPU %1").arg(p);
}
QString Window::winnerMessage(bool match) const {
    const auto &s=g.state();
    if(s.scores.empty())return {};
    QStringList winners;
    int lowest=100000;
    for(int p=0;p<g.players();++p) {
        int points=match?g.total(p):s.scores.back()[p];
        if(points<lowest){lowest=points;winners.clear();}
        if(points==lowest)winners<<playerLabel(p);
    }
    QString names=winners.size()>1?winners.mid(0,winners.size()-1).join(", ")+" and "+winners.back():winners.front();
    QString outcome=winners.size()>1?"tied for first in":"won";
    return QString("%1 %2 %3 with %4 %5.").arg(names,outcome,match?"the match":QString("round %1").arg(s.hole)).arg(lowest).arg(match?"total points":"points");
}
int Window::seatOrientation(int player) const {
    int me=std::clamp(net.localSeat(),0,g.players()-1);
    int relative=(player-me+g.players())%g.players();
    if(!relative)return 0;
    if(g.players()==2)return 180;
    if(relative==1)return 90;
    if(g.players()>=4 && relative==g.players()-1)return 270;
    return 180;
}
void Window::buildHand() {
    clearLayout(handGrid);hand.clear();columnLabels.clear();
    const auto &rules=g.state().rules;
    const int width=rules.rows==4?44:rules.rows==3?52:64;
    for(int i=0;i<rules.cardCount();++i){
        auto *card=new CardButton;card->setObjectName(QString("card%1").arg(i));
        card->setFixedSize(width,int(width*122.0/88));
        handGrid->addWidget(card,i/rules.columns,i%rules.columns);
        connect(card,&QAbstractButton::clicked,this,[this,i]{clickCard(i);});hand.push_back(card);
    }
    for(int c=0;c<rules.columns;++c){auto *text=label("","columnLabel");text->setAlignment(Qt::AlignCenter);handGrid->addWidget(text,rules.rows,c);columnLabels.push_back(text);}
    builtColumns=rules.columns;builtRows=rules.rows;
}
void Window::buildOpponents() {
    const auto &rules=g.state().rules;
    if(builtColumns!=rules.columns || builtRows!=rules.rows)buildHand();
    clearLayout(opponents);clearLayout(westOpponents);clearLayout(eastOpponents);computerCards.clear();computerLabels.clear();
    computerCards.resize(g.players());computerLabels.resize(g.players(),nullptr);builtPlayers=g.players();builtSeat=net.localSeat();builtRounds=g.state().holes;
    int me=std::clamp(net.localSeat(),0,g.players()-1);
    const int width=rules.rows==4?24:rules.rows==3?32:40,height=int(width*122.0/88);
    opponentArea->setVisible(g.players()>1);westArea->setVisible(g.players()>=3);eastArea->setVisible(g.players()>=4);
    tableGrid->setColumnMinimumWidth(0,g.players()>=3?140:0);tableGrid->setColumnMinimumWidth(2,g.players()>=3?140:0);
    for(int p=0;p<g.players();++p){
        if(p==me)continue;
        int rotation=seatOrientation(p);bool sideways=rotation==90 || rotation==270;
        auto *panel=new QWidget;auto *v=new QVBoxLayout(panel);v->setContentsMargins(0,0,0,0);v->setSpacing(4);
        auto *title=label("","computerLabel");title->setAlignment(Qt::AlignCenter);title->setWordWrap(true);title->setFixedWidth(sideways?rules.rows*height+(rules.rows-1)*3:rules.columns*width+(rules.columns-1)*3);computerLabels[p]=title;v->addWidget(title);
        auto *grid=new QGridLayout;grid->setSpacing(3);std::vector<CardButton *> cards(rules.cardCount());
        for(int i=0;i<rules.cardCount();++i){
            cards[i]=new CardButton;cards[i]->setOrientation(rotation);cards[i]->setFixedSize(sideways?height:width,sideways?width:height);cards[i]->setEnabled(false);cards[i]->setObjectName(QString("opponent%1card%2").arg(p).arg(i));
            int row=i/rules.columns,column=i%rules.columns;
            if(rotation==180){row=rules.rows-1-row;column=rules.columns-1-column;}
            else if(rotation==90){int oldRow=row;row=column;column=rules.rows-1-oldRow;}
            else if(rotation==270){int oldRow=row;row=rules.columns-1-column;column=oldRow;}
            grid->addWidget(cards[i],row,column);
        }
        computerCards[p]=cards;v->addLayout(grid);
        if(rotation==90)westOpponents->addWidget(panel);else if(rotation==270)eastOpponents->addWidget(panel);else opponents->addWidget(panel);
    }
    scoreTable->setColumnCount(g.players()+1);QStringList headers{"Round"};for(int p=0;p<g.players();++p)headers<<playerLabel(p);scoreTable->setHorizontalHeaderLabels(headers);
    scoreTable->setRowCount(g.state().holes);scoreTable->verticalHeader()->setDefaultSectionSize(23);
}
void Window::updateUi() {
    if(builtPlayers!=g.players() || builtSeat!=net.localSeat() || builtRounds!=g.state().holes || builtColumns!=g.state().rules.columns || builtRows!=g.state().rules.rows)buildOpponents();
    for(auto *card:centralWidget()->findChildren<CardButton *>())card->setAppearance(faceTheme,backTheme);
    const int me=std::clamp(net.localSeat(),0,g.players()-1);
    const auto &s=g.state();const auto &rules=s.rules; bool human=net.canAct(), over=s.phase==Phase::HoleOver || s.phase==Phase::MatchOver;
    heading->setText(QString("ROUND %1 / %2").arg(s.hole).arg(s.holes));
    findChild<QLabel *>("subtitle")->setText(QString("%1 × %2 cards · %3 players · %4 %5").arg(rules.columns).arg(rules.rows).arg(g.players()).arg(rules.decks).arg(rules.decks==1?"deck":"decks"));
    for(int p=0;p<g.players();++p) {
        const auto shown=Game::score(s.hands[p],true,rules);
        for(int i=0;i<rules.cardCount();++i) {
            int v=g.visibleValue(p,i);
            bool match=shown.matched[i];
            auto *card=p==me?hand[i]:computerCards[p][i];
            card->setCard(v,match,p==me?QString(QStringLiteral("123456789ABCEFGH")[i]):QString{});
            if(p==me) {
                bool enabled=human && ((s.phase==Phase::Opening && v==Hidden) || (s.phase==Phase::Draw && v==Hidden) || s.phase==Phase::Place);
                card->setEnabled(enabled); card->setCursor(enabled?Qt::PointingHandCursor:Qt::ArrowCursor);
                card->setAccessibleName(QString("Your card %1, %2").arg(i+1).arg(v==Hidden?"face down":QString::number(v)));
            }
        }
        if(p!=me) {
            computerLabels[p]->setText(QString("%1%2\n%3%4 points").arg(s.current==p && !over?"▶ ":"").arg(playerLabel(p)).arg(Game::score(s.hands[p],true,rules).total).arg(g.hidden(p)>0?" + ?":""));
        }
    }
    auto score=Game::score(s.hands[me],true,rules);
    yourScore->setText(QString("%1 points showing%2").arg(score.total).arg(g.hidden(me)>0?QString(" · %1 face down").arg(g.hidden(me)):""));
    for(int c=0;c<rules.columns;++c) {
        bool down=false;for(int r=0;r<rules.rows;++r)down=down || !s.hands[me][r*rules.columns+c].up;
        columnLabels[c]->setText(down?"?":QString("%1 points").arg(score.columns[c]));
    }
    const bool pairs=rules.matching==Matching::Pairs;
    QString quickRules=pairs?
        "<b>Match within a column</b><br>Each equal pair scores 0; leftover cards count.<br>−5 cards keep their values.<br><br><b>Matching bonus</b><br>Two or more pairs of the same number earn −5 per pair.":
        QString("<b>Match the whole column</b><br>All %1 cards must match to score 0.<br>−5 cards keep their values.<br><br><b>Matching bonus</b><br>Two or more columns of the same number earn −5 per column.").arg(rules.rows);
    if(rules.cancelRows)quickRules=QString("<b>Columns and rows</b><br>%1 Complete rows of %2 equal cards also match.<br>−5 cards keep their values.<br><br><b>Matching bonus</b><br>Two or more groups of the same number earn −5 per group. Intersections cancel once.")
        .arg(pairs?"Equal pairs within columns match.":"Complete matching columns cancel.").arg(rules.columns);
    findChild<QLabel *>("quickRules")->setText(quickRules);
    findChild<QLabel *>("keyboardHint")->setText("KEYS   Use the key shown on each card.<br>D draw · T take discard · X discard");
    deckLabel->setText(s.deck.empty()?"DRAW · RESHUFFLE":QString("DRAW PILE · %1").arg(s.deck.size()));
    stock->setCard(s.deck.empty() && s.discards.size()<=1?Hidden+1:Hidden); stock->setAccessibleName("Draw from deck (D)"); stock->setEnabled(human && s.phase==Phase::Draw && (!s.deck.empty() || s.discards.size()>1));
    discard->setCard(s.discards.empty()?Hidden+1:value(s.discards.back())); discard->setEnabled(human && s.phase==Phase::Draw && !s.discards.empty());
    held->setCard(s.phase!=Phase::Place?Hidden+1:s.current==me?value(s.held):Hidden);
    discardButton->setVisible(human && s.phase==Phase::Place && !s.fromDiscard);
    discardButton->setText("Discard draw");
    finishButton->setVisible(human && (s.phase==Phase::Draw || s.phase==Phase::Place));
    finishButton->setText(s.phase==Phase::Draw?"Flip final card · end round":"Replace final card");
    nextButton->setVisible(over); nextButton->setText(s.phase==Phase::MatchOver?"Play again":net.mode()==NetworkSession::Mode::Client?"Waiting for host":"Next round →"); nextButton->setEnabled(s.phase==Phase::MatchOver || (net.mode()!=NetworkSession::Mode::Client && net.ready()));
    if(s.phase==Phase::Opening && human) { statusTitle->setText(QString("%1 up. One face down.").arg(rules.cardCount()-1)); instruction->setText(QString("Turn %1 more of your cards face up. Your final card stays face down.").arg(g.hidden(me)-1)); }
    else if(over) {
        statusTitle->setText(s.phase==Phase::MatchOver?"Match complete":"Round complete");
        const bool match=s.phase==Phase::MatchOver;
        QString yourScoreText=match?QString("Your final total: %1 points.").arg(g.total(me)):
            QString("You scored %1 this round%2.").arg(score.total).arg(score.bonus?QString(" including a %1 matching bonus").arg(score.bonus):"");
        instruction->setText(winnerMessage(match)+"\n"+yourScoreText);
    } else if(!human) { statusTitle->setText(QString("%1 is playing").arg(playerLabel(s.current))); instruction->setText(s.phase==Phase::Opening?"Turning all but one card face up…":s.phase==Phase::Place?"Choosing where to play the drawn card…":"Considering the table…"); }
    else if(s.phase==Phase::Draw) { statusTitle->setText("Your turn"); instruction->setText("Draw or take the discard to keep playing, or flip your final card to end the round."); }
    else if(s.phase==Phase::Place) { statusTitle->setText(QString("You drew %1").arg(value(s.held))); instruction->setText(s.fromDiscard?"Replace any face-up card, or replace your final face-down card to end the round.":"Replace a face-up card, discard and continue, or replace your final face-down card to end the round for everyone."); }
    QStringList lines;
    for(int p=0;p<g.players();++p) lines<<QString("%1 <b>%2</b>").arg(playerLabel(p).toHtmlEscaped()).arg(g.total(p));
    QString totalText; for(int i=0;i<lines.size();++i) { if(i) totalText+=(i%2==0?"<br>":" &nbsp; · &nbsp; "); totalText+=lines[i]; } totals->setText(totalText);
    for(int r=0;r<s.holes;++r) for(int c=0;c<g.players()+1;++c) {
        auto *item=scoreTable->item(r,c); if(!item) { item=new QTableWidgetItem; item->setTextAlignment(Qt::AlignCenter); scoreTable->setItem(r,c,item); }
        item->setText(c==0?QString::number(r+1):r<int(s.scores.size())?QString::number(s.scores[r][c-1]):"—");
        item->setBackground(r==s.hole-1?QColor("#2b5141"):QColor("#16352f"));
    }
    result->setText(s.phase==Phase::MatchOver?winnerMessage(true):QString("%1 rounds · Lowest total wins").arg(s.holes));
    lastMove->setText(s.message);
    connectionLabel->setVisible(net.online());connectionButton->setVisible(net.online());connectionLabel->setText(net.status());
    connectionButton->setText(net.mode()==NetworkSession::Mode::Client && !net.ready()?"Reconnect / details":"Connection details");
    QStringList headers{"Round"};for(int p=0;p<g.players();++p)headers<<playerLabel(p);scoreTable->setHorizontalHeaderLabels(headers);
    if(!net.ready()) {statusTitle->setText("Waiting for players");instruction->setText(net.status());}
    else if(net.pending()){statusTitle->setText("Sending your move…");instruction->setText("Waiting for the host to confirm.");}
    if(!setupOpen && net.mode()!=NetworkSession::Mode::Client && net.ready() && net.isComputer(s.current) && g.active()) { if(!aiTimer.isActive()) { aiTimer.setInterval(fastTurns?180:650); aiTimer.start(); } }
    else aiTimer.stop();
}
void Window::clickCard(int i) {
    if(!net.canAct() || i<0 || i>=g.state().rules.cardCount())return;
    if(g.state().phase==Phase::Opening)net.act("open",i);
    else if(g.state().phase==Phase::Place)net.act("replace",i);
    else if(g.state().phase==Phase::Draw && g.visibleValue(net.localSeat(),i)==Hidden)net.act("finish");
}
void Window::keyAction(int key) { net.act(key==Qt::Key_D?"draw":key==Qt::Key_T?"take":"discard"); }
void Window::changed() { saveGame(); updateUi(); }
void Window::saveGame() {
    if(!persistence || net.online())return;
    QDir().mkpath(QFileInfo(savePath).absolutePath());
    QSaveFile file(savePath);
    if(!file.open(QIODevice::WriteOnly)) { statusBar()->showMessage("Unable to save game: "+file.errorString());return; }
    auto bytes=QJsonDocument(g.save()).toJson();
    if(file.write(bytes)!=bytes.size() || !file.commit()) statusBar()->showMessage("Unable to save game: "+file.errorString());
}
void Window::savePreferences() {
    if(!persistence)return;
    QSettings settings;settings.setValue("opponents",preferredOpponents);settings.setValue("rounds",preferredRounds);
    settings.setValue("columns",preferredRules.columns);settings.setValue("rows",preferredRules.rows);
    settings.setValue("decks",preferredRules.decks);settings.setValue("matching",int(preferredRules.matching));
    settings.setValue("cancelRows",preferredRules.cancelRows);
    for(int i=0;i<preferredRemote.size();++i)settings.setValue(QString("onlineSeat%1").arg(i+1),preferredRemote[i]);
    settings.setValue("onlineName",preferredName);settings.setValue("onlinePort",preferredPort);
    settings.setValue("cpuTarget",preferredRules.cpuTarget);settings.setValue("faceTheme",faceTheme);settings.setValue("backTheme",backTheme);settings.setValue("fast",fastTurns);
}
void Window::newGame() {
    aiTimer.stop();
    QVector<bool> remote;
    for(int i=0;i<preferredOpponents;++i)remote.append(preferredRemote.value(i,false));
    if(remote.contains(true)) {
        if(!net.host(remote,preferredRounds,preferredName,preferredPort,preferredRules)) {
            QMessageBox::warning(this,"Could not host game",net.errorString());updateUi();return;
        }
    } else net.offline(preferredOpponents,preferredRounds,preferredRules);
    changed();
}
void Window::showGameSettings(bool joining) {
    aiTimer.stop();setupOpen=true;
    QDialog dialog(this);dialog.setObjectName("optionsDialog");dialog.setWindowTitle("Options — Turnover Golf");
    dialog.ensurePolished();const auto available=dialog.screen()->availableGeometry();
    const int width=std::min(std::max(850,dialog.fontMetrics().averageCharWidth()*88),available.width()-60);
    const int height=std::min(720,available.height()-80);
    dialog.setMinimumSize(std::min(760,width),std::min(540,height));dialog.resize(width,height);
    auto *outer=new QVBoxLayout(&dialog);outer->setContentsMargins(20,18,20,18);outer->setSpacing(14);
    auto *tabs=new QTabWidget;tabs->setObjectName("setupTabs");outer->addWidget(tabs,1);
    auto addPage=[&](const QString &title){
        auto *page=new QWidget;auto *layout=new QVBoxLayout(page);
        layout->setContentsMargins(20,20,20,20);layout->setSpacing(18);
        auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setWidget(page);
        tabs->addTab(scroll,title);return layout;
    };
    auto addForm=[](QVBoxLayout *layout){auto *form=new QFormLayout;form->setRowWrapPolicy(QFormLayout::WrapLongRows);form->setVerticalSpacing(16);form->setHorizontalSpacing(20);layout->addLayout(form);return form;};
    auto addNote=[](QVBoxLayout *layout,const QString &text){auto *note=new QLabel(text);note->setWordWrap(true);layout->addWidget(note);};
    auto *players=addPage("Players && rounds");auto *form=addForm(players);
    auto *count=new QComboBox;count->setObjectName("playerCount");
    for(int n=2;n<=4;++n)count->addItem(QString("%1 — you + %2 %3").arg(n).arg(n-1).arg(n==2?"player":"players"),n-1);
    count->setCurrentIndex(count->findData(preferredOpponents));form->addRow("Total players",count);
    std::array<QComboBox *,3> seats;std::array<QLabel *,3> seatLabels;
    for(int i=0;i<3;++i){
        seats[i]=new QComboBox;seats[i]->setObjectName(QString("seatType%1").arg(i+1));
        seats[i]->addItem("Computer",false);seats[i]->addItem("Online human",true);
        seats[i]->setCurrentIndex(preferredRemote.value(i,false)?1:0);
        seatLabels[i]=new QLabel(QString("Player %1").arg(i+2));form->addRow(seatLabels[i],seats[i]);
    }
    auto *rounds=new QSpinBox;rounds->setObjectName("roundCount");rounds->setRange(1,99);rounds->setValue(preferredRounds);form->addRow("Rounds",rounds);
    auto *name=new QLineEdit(preferredName);name->setMaxLength(24);name->setObjectName("hostName");auto *nameLabel=new QLabel("Your online name");form->addRow(nameLabel,name);
    auto *port=new QSpinBox;port->setObjectName("hostPort");port->setRange(1024,65535);port->setValue(preferredPort);auto *portLabel=new QLabel("Host port");form->addRow(portLabel,port);
    addNote(players,"You always sit at the bottom. Online humans join using your address, port and room code; a hosted game waits for every selected human seat.");players->addStretch();

    auto *cardsPage=addPage("Cards");auto *cardsForm=addForm(cardsPage);
    auto *layoutChoice=new QComboBox;layoutChoice->setObjectName("cardLayout");
    layoutChoice->addItem("4 × 2 — 8 cards",8);layoutChoice->addItem("3 × 3 — 9 cards",9);layoutChoice->addItem("4 × 4 — 16 cards",16);
    layoutChoice->setCurrentIndex(layoutChoice->findData(preferredRules.cardCount()));cardsForm->addRow("Layout (columns × rows)",layoutChoice);
    auto *matching=new QComboBox;matching->setObjectName("matchingMode");
    matching->addItem("Cancel whole matching columns",int(Matching::Columns));matching->addItem("Cancel matching pairs within each column",int(Matching::Pairs));
    matching->setCurrentIndex(matching->findData(int(preferredRules.matching)));cardsForm->addRow("Matching rule",matching);
    auto *cancelRows=new QCheckBox("Also cancel complete matching rows");cancelRows->setObjectName("cancelRows");
    cancelRows->setChecked(preferredRules.cancelRows);cardsForm->addRow("Rows (3 × 3 / 4 × 4)",cancelRows);
    auto *decks=new QSpinBox;decks->setObjectName("deckCount");decks->setRange(1,MaxDecks);decks->setValue(preferredRules.decks);decks->setSuffix(" × 108 cards");cardsForm->addRow("Decks",decks);
    addNote(cardsPage,"Larger layouts use Turnover Golf house scoring, adapted from the 4 × 2 game.\n\nWhole columns: all 2, 3 or 4 cards in a column must match.\n\nPairs: each equal pair within a column cancels, even when the cards are not adjacent. Leftover cards still count.\n\n−5 cards keep their negative values in both modes. Two or more matching groups of the same value earn an extra −5 per group.");
    addNote(cardsPage,"Optional rows: every card in a horizontal row must match. Each complete row also counts as a bonus group, alongside the selected column groups. Where matches cross, a card only cancels once; −5 cards still keep their values.");
    addNote(cardsPage,"Every layout starts face down. Turn all but one card up before drawing. Flip or replace the final card to end the round for everyone.");cardsPage->addStretch();

    auto *computer=addPage("Computer");auto *computerForm=addForm(computer);
    auto *target=new QComboBox;target->setObjectName("cpuTarget");
    for(int score:{0,-5,-10,-20,3,6})target->addItem(QString("%1 or lower").arg(score),score);
    if(target->findData(preferredRules.cpuTarget)<0)target->addItem(QString("%1 or lower").arg(preferredRules.cpuTarget),preferredRules.cpuTarget);
    target->setCurrentIndex(target->findData(preferredRules.cpuTarget));computerForm->addRow("Finishing score target",target);
    auto *fast=new QCheckBox("Fast computer turns");fast->setObjectName("fastTurns");fast->setChecked(fastTurns);computerForm->addRow("Pacing",fast);
    addNote(computer,"Lower scores are better, including negative scores. The computer builds matches and bonuses, develops its hand for at least six turns, and only chooses to finish at or below this target when it expects to beat the other hands.\n\nA lower target can make rounds longer. Another player can still end the round while the computer has a higher score. Computers never look at face-down cards.");computer->addStretch();

    auto *appearance=addPage("Appearance");auto *appearanceForm=addForm(appearance);
    auto *faces=new QComboBox;faces->setObjectName("faceTheme");faces->addItems(CardButton::themeNames());faces->setCurrentIndex(faceTheme);appearanceForm->addRow("Card fronts",faces);
    auto *backs=new QComboBox;backs->setObjectName("backTheme");backs->addItems(CardButton::themeNames());backs->setCurrentIndex(backTheme);appearanceForm->addRow("Card backs",backs);
    addNote(appearance,"Choose the front and back independently. These are your personal display settings, including in online games.");
    auto *preview=new QHBoxLayout;preview->setSpacing(22);preview->addStretch();std::array<CardButton *,3> previews;
    for(int i=0;i<3;++i){previews[i]=new CardButton;previews[i]->setObjectName(QString("themePreview%1").arg(i));previews[i]->setFixedSize(106,147);previews[i]->setEnabled(false);previews[i]->setCard(i==0?7:i==1?-5:Hidden);preview->addWidget(previews[i]);}
    preview->addStretch();appearance->addLayout(preview);appearance->addStretch();
    auto refreshPreview=[&]{for(auto *card:previews)card->setAppearance(faces->currentIndex(),backs->currentIndex());};
    connect(faces,&QComboBox::currentIndexChanged,&dialog,refreshPreview);connect(backs,&QComboBox::currentIndexChanged,&dialog,refreshPreview);refreshPreview();

    QSettings settings;auto *joinPage=addPage("Join online");auto *joinForm=addForm(joinPage);
    auto *joinName=new QLineEdit(settings.value("onlineName","Player").toString());joinName->setMaxLength(24);joinName->setObjectName("joinName");joinForm->addRow("Your name",joinName);
    auto *address=new QLineEdit(settings.value("lastHost","").toString());address->setPlaceholderText("Host IP address or hostname");address->setMaxLength(253);address->setObjectName("joinAddress");joinForm->addRow("Host address",address);
    auto *joinPort=new QSpinBox;joinPort->setObjectName("joinPort");joinPort->setRange(1,65535);joinPort->setValue(settings.value("lastPort",45839).toInt());joinForm->addRow("Port",joinPort);
    auto *code=new QLineEdit;code->setObjectName("joinCode");code->setMaxLength(12);code->setPlaceholderText("12-character code from the host");joinForm->addRow("Room code",code);
    addNote(joinPage,"The host chooses the players, rounds, layout, matching rule, decks and CPU target. Both people need the same game version.\n\nOn the same network, use the host's local address. Across the internet, use a VPN or a reachable public address with the host port forwarded.");joinPage->addStretch();
    const int joinTab=tabs->count()-1;
    const bool guest=net.mode()==NetworkSession::Mode::Client;
    if(guest){
        const auto &current=g.state();count->setCurrentIndex(count->findData(std::clamp(current.opponents,1,3)));rounds->setValue(current.holes);
        layoutChoice->setCurrentIndex(layoutChoice->findData(current.rules.cardCount()));matching->setCurrentIndex(matching->findData(int(current.rules.matching)));decks->setValue(current.rules.decks);
        cancelRows->setChecked(current.rules.cancelRows);
        if(target->findData(current.rules.cpuTarget)<0)target->addItem(QString("%1 or lower").arg(current.rules.cpuTarget),current.rules.cpuTarget);
        target->setCurrentIndex(target->findData(current.rules.cpuTarget));
        for(auto *control:std::initializer_list<QWidget *>{count,rounds,layoutChoice,matching,decks,target,name,port})control->setEnabled(false);
        for(int i=0;i<3;++i){seats[i]->setCurrentIndex(i<current.opponents && !net.isComputer(i+1)?1:0);seats[i]->setEnabled(false);}
    }
    auto *effect=new QLabel;effect->setWordWrap(true);outer->addWidget(effect);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setMinimumWidth(165);outer->addWidget(buttons);
    auto selectedRules=[&]{Rules r;r.columns=layoutChoice->currentData().toInt()==9?3:4;r.rows=layoutChoice->currentData().toInt()/r.columns;r.decks=decks->value();r.matching=Matching(matching->currentData().toInt());r.cpuTarget=target->currentData().toInt();r.cancelRows=r.rows>=3 && cancelRows->isChecked();return r;};
    auto needsNewGame=[&]{
        if(guest)return false;
        const auto r=selectedRules();const auto &old=g.state().rules;
        if(count->currentData().toInt()!=g.state().opponents || r.columns!=old.columns || r.rows!=old.rows || r.decks!=old.decks || r.matching!=old.matching || r.cancelRows!=old.cancelRows)return true;
        for(int i=0;i<count->currentData().toInt();++i)if(seats[i]->currentData().toBool()!=(net.mode()==NetworkSession::Mode::Host && !net.isComputer(i+1)))return true;
        return net.mode()==NetworkSession::Mode::Host && (name->text()!=preferredName || port->value()!=preferredPort);
    };
    auto refresh=[&]{
        cancelRows->setEnabled(!guest && layoutChoice->currentData().toInt()!=8);
        bool online=false;
        for(int i=0;i<3;++i){bool visible=i<count->currentData().toInt();seats[i]->setVisible(visible);seatLabels[i]->setVisible(visible);online=online || (visible && seats[i]->currentData().toBool());}
        name->setVisible(online);nameLabel->setVisible(online);port->setVisible(online);portLabel->setVisible(online);
        const bool join=tabs->currentIndex()==joinTab,restart=needsNewGame();
        buttons->button(QDialogButtonBox::Ok)->setText(join?"Join game":restart?(online?"Host new game":"Start new game"):"Save options");
        effect->setText(join?"Joining leaves the current game. Appearance and pacing are kept.":guest?"The host controls the game rules. Your appearance and pacing changes keep this online game open.":restart?"These player or card-rule changes start a new match. Your choices will be remembered.":"Save changes without restarting. Completed scores and the current hand are kept.");
    };
    connect(count,&QComboBox::currentIndexChanged,&dialog,refresh);connect(tabs,&QTabWidget::currentChanged,&dialog,refresh);
    connect(layoutChoice,&QComboBox::currentIndexChanged,&dialog,refresh);connect(matching,&QComboBox::currentIndexChanged,&dialog,refresh);connect(decks,&QSpinBox::valueChanged,&dialog,refresh);
    connect(cancelRows,&QCheckBox::toggled,&dialog,refresh);
    connect(name,&QLineEdit::textChanged,&dialog,refresh);connect(port,&QSpinBox::valueChanged,&dialog,refresh);
    for(auto *seat:seats)connect(seat,&QComboBox::currentIndexChanged,&dialog,refresh);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        if(tabs->currentIndex()==joinTab){
            if(address->text().trimmed().isEmpty() || code->text().trimmed().size()!=12){QMessageBox::information(&dialog,"Join online game","Enter the host address and 12-character room code.");return;}
            net.join(address->text(),quint16(joinPort->value()),code->text(),joinName->text());
            preferredName=joinName->text();
            if(persistence){settings.setValue("onlineName",joinName->text());settings.setValue("lastHost",address->text().trimmed());settings.setValue("lastPort",joinPort->value());}
        }else if(!guest){
            const Rules rules=selectedRules();QVector<bool> remote;for(int i=0;i<count->currentData().toInt();++i)remote.append(seats[i]->currentData().toBool());
            if(needsNewGame()){
                if(remote.contains(true)){
                    if(!net.host(remote,rounds->value(),name->text(),quint16(port->value()),rules)){QMessageBox::warning(&dialog,"Could not host game",net.errorString());return;}
                }else net.offline(count->currentData().toInt(),rounds->value(),rules);
            }else{
                if(rounds->value()<g.state().hole){QMessageBox::information(&dialog,"Rounds",QString("You are on round %1. Choose at least that many rounds, or start a new game first.").arg(g.state().hole));return;}
                net.act("rounds",rounds->value());net.act("cpuTarget",rules.cpuTarget);
            }
            preferredOpponents=count->currentData().toInt();preferredRounds=rounds->value();preferredRules=rules;
            preferredRemote.clear();for(auto *seat:seats)preferredRemote.append(seat->currentData().toBool());
            preferredName=name->text();preferredPort=quint16(port->value());
        }
        faceTheme=faces->currentIndex();backTheme=backs->currentIndex();fastTurns=fast->isChecked();savePreferences();dialog.accept();
    });
    tabs->setCurrentIndex(joining?joinTab:0);refresh();dialog.exec();setupOpen=false;buildOpponents();changed();
}
void Window::showConnection() {
    QDialog dialog(this);dialog.setWindowTitle("Online game — Turnover Golf");dialog.resize(540,340);auto *layout=new QVBoxLayout(&dialog);
    auto *status=new QLabel(net.status());status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
    QString details;
    if(net.mode()==NetworkSession::Mode::Host){
        details=QString("Host address: %1\nPort: %2\nRoom code: %3").arg(net.addresses()).arg(net.port()).arg(net.roomCode());
        auto *info=new QLabel(details);info->setTextFormat(Qt::PlainText);info->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(info);
        auto *copy=new QPushButton("Copy connection details");connect(copy,&QPushButton::clicked,&dialog,[details]{QApplication::clipboard()->setText(details);});layout->addWidget(copy);
        auto *help=new QLabel("Share these details with your players. For a friend outside your network, use your VPN address or public address and forward this TCP port to this computer. The app does not change router settings.\n\nIf someone disconnects, play pauses until they reconnect from their existing game window. Keep the host window open for the match.");help->setWordWrap(true);layout->addWidget(help);
    }else{
        auto *retry=new QPushButton("Reconnect to this room");connect(retry,&QPushButton::clicked,&dialog,[this,&dialog]{net.reconnect();dialog.accept();});layout->addWidget(retry);
        auto *help=new QLabel("Reconnect from this window to keep your seat. The host must still have the room open. To join a different room, choose New game → Join online game.");help->setWordWrap(true);layout->addWidget(help);
    }
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);dialog.exec();
}
void Window::showRules() {
    bool resume=aiTimer.isActive(); aiTimer.stop();
    QDialog dialog(this); dialog.setWindowTitle("How to play — Turnover Golf"); dialog.resize(650,620); auto *v=new QVBoxLayout(&dialog); auto *text=new QTextEdit; text->setReadOnly(true);
    text->setHtml(R"(<h1>Turnover Golf</h1>
      <p>Lowest total wins, including negative scores. Open <b>Settings → Options</b> to choose players and rounds, card layout, matching rule, decks, computer strategy, and card appearance. New game and Play again use your last choices without asking again.</p>
      <h2>Choose your game</h2><p>Choose 2–4 players, 1–99 rounds, and a <b>4×2, 3×3, or 4×4</b> hand (columns × rows). Each opponent can be a computer or an online human. You always sit at the bottom; all hands face their seats. Choose 1–8 decks. Each 108-card deck contains eight cards of each value 0–12 and four −5 cards.</p>
      <h2>Start and take a turn</h2><p>All cards are dealt face down. Turn <b>all but one</b> face up before the first draw: 7 of 8, 8 of 9, or 15 of 16. Draw from the stock or take the top discard. Replace a face-up card; the replaced card becomes the top discard. You must use a taken discard. A stock draw can instead be discarded to keep playing.</p>
      <h2>End the round</h2><p>On your turn, flip your final face-down card without drawing, or replace it with a drawn card. Everyone immediately reveals their remaining card and scores, with no extra turns. Once you draw, resolve that draw before ending your turn. When the stock runs out, used discards are shuffled back into it, keeping the top discard available.</p>
      <h2>Two matching rules</h2><ul><li><b>Whole columns:</b> every card in a column must be equal to cancel. That means 2, 3 or 4 equal cards, depending on your layout.</li><li><b>Pairs within columns:</b> each pair of equal cards in one column cancels, including nonadjacent rows. Any unpaired card still scores. Four equal cards make two pairs.</li><li><b>−5 cards always keep their negative values.</b></li><li>Two or more matching groups of the same value earn an extra −5 per group. A group means a complete column or a pair, according to the selected rule. Groups do not need to be in adjacent columns. Side-by-side cards only match when the complete-row option is enabled.</li></ul><p>In the original 4×2 layout, both rules behave the same. Four −5 cards arranged as two vertical pairs contribute −30, before the other columns are added.</p>
      <h2>Optional matching rows</h2><p>For 3×3 and 4×4, enable <b>Also cancel complete matching rows</b> on the Cards tab. Every card in a horizontal row must have the same value. This works with either column mode. Each complete row counts as another bonus group of that value. A card shared by a matching row and column cancels only once; −5 cards retain their value once. Turning this option on or off starts a new match.</p><p>With whole-column matching, two columns of the same nonnegative value contribute −10. Two columns filled with −5 contribute −30 in 4×2, −40 in 3×3, or −50 in 4×4, before other cards or row matches. In 4×4 pair mode, those eight −5 cards form four pairs and contribute −60. One deck contains only four −5 cards.</p>
      <h2>Computer strategy</h2><p>Choose a finishing target of 0, −5, −10, −20, 3, or 6 points or lower. Default: 0 or lower. The computer develops its hand for at least six turns, seeks matches and bonuses, and only chooses to finish at or below its target when it expects to beat the others. Negative scores are better. Stricter targets can lengthen rounds. Computers never look at hidden cards. Another player can still end a round while a computer has a higher score.</p>
      <h2>Settings and appearance</h2><p>The five Options tabs are Players &amp; rounds, Cards, Computer, Appearance, and Join online. There are 12 card-front themes and 12 card-back themes, selected independently with a preview. Appearance and pacing changes keep your current hand. Round-count and CPU-target changes also keep completed scores. Changes to players, layout, matching rule or decks begin a new match; the dialog says when this will happen. Cancel leaves your choices unchanged.</p>
      <h2>Online play</h2><p>Choose Online human seats in Options to host. Share Connection details with the guests. Guests choose <b>Game → Join online game</b> or the Join online tab. The host controls game rules and round advancement; each guest chooses their own card appearance. All players need version 1.0.10 or a compatible newer build.</p><p>Use a local address on the same network. Across the internet, use a VPN or a reachable public address with the host port forwarded. Connections use plain TCP. A disconnect pauses play; Reconnect from the original guest window retains that seat. Closing the host ends the room. Online matches leave offline saves intact.</p>
      <h2>Controls</h2><p>Click a card or press the small key shown on it: 1–9, then A, B, C, E, F, G, H for a 16-card hand. D draws, T takes the discard, X discards a stock draw. Ctrl+N starts a new game with remembered choices. F1 opens these rules. Offline matches save after every action and resume at launch. Tied final scores share the win.</p>
      <p>Play Nine card values with your house rules. Larger layouts and selectable matching modes are custom variants.</p>)");
    v->addWidget(text); auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close); v->addWidget(buttons); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject); dialog.exec(); if(resume) updateUi();
}
void Window::closeEvent(QCloseEvent *event) { saveGame(); if(persistence) QSettings().setValue("geometry",saveGeometry()); QMainWindow::closeEvent(event); }
void Window::capture(const QString &path) { grab().save(path); }
