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
    setObjectName("mainWindow"); setWindowTitle("Eight-card golf");
    setWindowIcon(QIcon(":/turnover-golf.svg")); resize(1250,820); setMinimumSize(1100,730);
    QSettings settings;
    g.newMatch(settings.value("opponents",1).toInt(),settings.value("rounds",9).toInt());
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
    statusBar()->showMessage(resumed?"Resumed your saved game.":"Turn seven cards face up; leave one face down. New Game: computer or online human opponents.",12000);
}
void Window::buildUi() {
    auto *gameMenu=menuBar()->addMenu("&Game");
    auto *newAction=gameMenu->addAction("&New game…",QKeySequence::New,this,&Window::newGame);
    newAction->setIcon(QIcon::fromTheme("document-new"));
    gameMenu->addAction("&Quit",QKeySequence::Quit,this,&QWidget::close);
    auto *settings=menuBar()->addMenu("&Settings");
    auto *playersAction=settings->addAction("&Players…",this,&Window::newGame);playersAction->setObjectName("playerSettings");
    roundsAction=settings->addAction("&Rounds…",this,&Window::showRoundSettings);roundsAction->setObjectName("roundSettings");
    auto *fast=settings->addAction("Fast computer turns"); fast->setCheckable(true);
    QSettings config; fast->setChecked(config.value("fast",false).toBool());
    connect(fast,&QAction::toggled,this,[this](bool on){ if(persistence) QSettings().setValue("fast",on); aiTimer.setInterval(on?180:650); });
    auto *help=menuBar()->addMenu("&Help");
    help->addAction("&How to play",QKeySequence::HelpContents,this,&Window::showRules);
    help->addAction("&About Turnover Golf",this,[this]{ QMessageBox::about(this,"About Turnover Golf",QString("<h2>Turnover Golf %1</h2><p>A native Qt game for KDE, using Play Nine card values and scoring, with your house rules.</p><p>Original card artwork. Choose 2–4 total players, including you. Other seats can be computers or online humans.</p><p>Seven cards start face up. Flip or replace your final card to end the round; everyone reveals together.</p>").arg(GOLF_VERSION)); });
    auto *root=new QWidget; root->setObjectName("surface"); auto *outer=new QVBoxLayout(root); outer->setContentsMargins(24,15,24,16); outer->setSpacing(14);
    auto *top=new QHBoxLayout;
    auto *brand=new QVBoxLayout; brand->setSpacing(0); brand->addWidget(label("TURNOVER GOLF","brand")); brand->addWidget(label("Eight cards. Lowest score wins.","subtitle")); top->addLayout(brand); top->addStretch();
    heading=label("","holeBadge"); top->addWidget(heading); top->addSpacing(12);
    auto *newButton=new QPushButton("Players / New game"); newButton->setObjectName("newGame"); connect(newButton,&QPushButton::clicked,this,&Window::newGame); top->addWidget(newButton);
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
    auto *cards=new QGridLayout;cards->setHorizontalSpacing(9);cards->setVerticalSpacing(3);
    for(int i=0;i<8;++i){
        hand[i]=new CardButton;hand[i]->setObjectName(QString("card%1").arg(i));hand[i]->setFixedSize(64,89);cards->addWidget(hand[i],i/4,i%4);connect(hand[i],&QAbstractButton::clicked,this,[this,i]{clickCard(i);});
        auto *shortcut=new QShortcut(QKeySequence(QString::number(i+1)),this);connect(shortcut,&QShortcut::activated,this,[this,i]{clickCard(i);});
    }
    for(int c=0;c<4;++c){columnLabels[c]=label("","columnLabel");columnLabels[c]->setAlignment(Qt::AlignCenter);cards->addWidget(columnLabels[c],2,c);}
    auto *cardsRow=new QHBoxLayout;cardsRow->addStretch();cardsRow->addLayout(cards);cardsRow->addStretch();playerColumn->addLayout(cardsRow);tableGrid->addWidget(south,2,0,1,3,Qt::AlignHCenter);
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
void Window::buildOpponents() {
    clearLayout(opponents);clearLayout(westOpponents);clearLayout(eastOpponents);computerCards.clear();computerLabels.clear();
    computerCards.resize(g.players());computerLabels.resize(g.players(),nullptr);builtPlayers=g.players();builtSeat=net.localSeat();builtRounds=g.state().holes;
    int me=std::clamp(net.localSeat(),0,g.players()-1);
    const int width=40,height=int(width*122.0/88);
    opponentArea->setVisible(g.players()>1);westArea->setVisible(g.players()>=3);eastArea->setVisible(g.players()>=4);
    tableGrid->setColumnMinimumWidth(0,g.players()>=3?140:0);tableGrid->setColumnMinimumWidth(2,g.players()>=3?140:0);
    for(int p=0;p<g.players();++p){
        if(p==me)continue;
        int rotation=seatOrientation(p);bool sideways=rotation==90 || rotation==270;
        auto *panel=new QWidget;auto *v=new QVBoxLayout(panel);v->setContentsMargins(0,0,0,0);v->setSpacing(4);
        auto *title=label("","computerLabel");title->setAlignment(Qt::AlignCenter);title->setWordWrap(true);title->setFixedWidth(sideways?2*height+3:4*width+9);computerLabels[p]=title;v->addWidget(title);
        auto *grid=new QGridLayout;grid->setSpacing(3);std::array<CardButton *,8> cards;
        for(int i=0;i<8;++i){
            cards[i]=new CardButton;cards[i]->setOrientation(rotation);cards[i]->setFixedSize(sideways?height:width,sideways?width:height);cards[i]->setEnabled(false);cards[i]->setObjectName(QString("opponent%1card%2").arg(p).arg(i));
            int row=i/4,column=i%4;
            if(rotation==180){row=1-row;column=3-column;}
            else if(rotation==90){int oldRow=row;row=column;column=1-oldRow;}
            else if(rotation==270){int oldRow=row;row=3-column;column=oldRow;}
            grid->addWidget(cards[i],row,column);
        }
        computerCards[p]=cards;v->addLayout(grid);
        if(rotation==90)westOpponents->addWidget(panel);else if(rotation==270)eastOpponents->addWidget(panel);else opponents->addWidget(panel);
    }
    scoreTable->setColumnCount(g.players()+1);QStringList headers{"Round"};for(int p=0;p<g.players();++p)headers<<playerLabel(p);scoreTable->setHorizontalHeaderLabels(headers);
    scoreTable->setRowCount(g.state().holes);scoreTable->verticalHeader()->setDefaultSectionSize(23);
}
void Window::updateUi() {
    if(builtPlayers!=g.players() || builtSeat!=net.localSeat() || builtRounds!=g.state().holes)buildOpponents();
    const int me=std::clamp(net.localSeat(),0,g.players()-1);
    const auto &s=g.state(); bool human=net.canAct(), over=s.phase==Phase::HoleOver || s.phase==Phase::MatchOver;
    heading->setText(QString("ROUND %1 / %2").arg(s.hole).arg(s.holes));
    findChild<QLabel *>("subtitle")->setText(QString("Eight cards · %1 players · Lowest score wins").arg(g.players()));
    for(int p=0;p<g.players();++p) {
        for(int i=0;i<8;++i) {
            int partner=(i+4)%8, v=g.visibleValue(p,i);
            bool match=v!=Hidden && v==g.visibleValue(p,partner);
            auto *card=p==me?hand[i]:computerCards[p][i];
            card->setCard(v,match,p==me?QString::number(i+1):QString{});
            if(p==me) {
                bool enabled=human && ((s.phase==Phase::Opening && v==Hidden) || (s.phase==Phase::Draw && v==Hidden) || s.phase==Phase::Place);
                card->setEnabled(enabled); card->setCursor(enabled?Qt::PointingHandCursor:Qt::ArrowCursor);
                card->setAccessibleName(QString("Your card %1, %2").arg(i+1).arg(v==Hidden?"face down":QString::number(v)));
            }
        }
        if(p!=me) {
            computerLabels[p]->setText(QString("%1%2\n%3%4 points").arg(s.current==p && !over?"▶ ":"").arg(playerLabel(p)).arg(Game::score(s.hands[p],true).total).arg(g.hidden(p)>0?" + ?":""));
        }
    }
    auto score=Game::score(s.hands[me],true);
    yourScore->setText(QString("%1 points showing%2").arg(score.total).arg(g.hidden(me)>0?QString(" · %1 face down").arg(g.hidden(me)):""));
    for(int c=0;c<4;++c) columnLabels[c]->setText((!s.hands[me][c].up || !s.hands[me][c+4].up)?"?":QString("%1 points").arg(score.columns[c]));
    deckLabel->setText(s.deck.empty()?"DRAW · RESHUFFLE":QString("DRAW PILE · %1").arg(s.deck.size()));
    stock->setCard(s.deck.empty() && s.discards.size()<=1?Hidden+1:Hidden); stock->setAccessibleName("Draw from deck (D)"); stock->setEnabled(human && s.phase==Phase::Draw && (!s.deck.empty() || s.discards.size()>1));
    discard->setCard(s.discards.empty()?Hidden+1:value(s.discards.back())); discard->setEnabled(human && s.phase==Phase::Draw && !s.discards.empty());
    held->setCard(s.phase!=Phase::Place?Hidden+1:s.current==me?value(s.held):Hidden);
    discardButton->setVisible(human && s.phase==Phase::Place && !s.fromDiscard);
    discardButton->setText("Discard draw");
    finishButton->setVisible(human && (s.phase==Phase::Draw || s.phase==Phase::Place));
    finishButton->setText(s.phase==Phase::Draw?"Flip final card · end round":"Replace final card");
    nextButton->setVisible(over); nextButton->setText(s.phase==Phase::MatchOver?"Play again…":net.mode()==NetworkSession::Mode::Client?"Waiting for host":"Next round →"); nextButton->setEnabled(s.phase==Phase::MatchOver || (net.mode()!=NetworkSession::Mode::Client && net.ready()));
    if(s.phase==Phase::Opening && human) { statusTitle->setText("Seven up. One face down."); instruction->setText(QString("Turn %1 more of your cards face up. Your eighth card stays face down.").arg(g.hidden(me)-1)); }
    else if(over) {
        statusTitle->setText(s.phase==Phase::MatchOver?"Match complete":"Round complete");
        const bool match=s.phase==Phase::MatchOver;
        QString yourScoreText=match?QString("Your final total: %1 points.").arg(g.total(me)):
            QString("You scored %1 this round%2.").arg(score.total).arg(score.bonus?QString(" including a %1 matching bonus").arg(score.bonus):"");
        instruction->setText(winnerMessage(match)+"\n"+yourScoreText);
    } else if(!human) { statusTitle->setText(QString("%1 is playing").arg(playerLabel(s.current))); instruction->setText(s.phase==Phase::Opening?"Turning seven cards face up…":s.phase==Phase::Place?"Choosing where to play the drawn card…":"Considering the table…"); }
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
    roundsAction->setEnabled(net.mode()!=NetworkSession::Mode::Client);
    connectionButton->setText(net.mode()==NetworkSession::Mode::Client && !net.ready()?"Reconnect / details":"Connection details");
    QStringList headers{"Round"};for(int p=0;p<g.players();++p)headers<<playerLabel(p);scoreTable->setHorizontalHeaderLabels(headers);
    if(!net.ready()) {statusTitle->setText("Waiting for players");instruction->setText(net.status());}
    else if(net.pending()){statusTitle->setText("Sending your move…");instruction->setText("Waiting for the host to confirm.");}
    if(!setupOpen && net.mode()!=NetworkSession::Mode::Client && net.ready() && net.isComputer(s.current) && g.active()) { if(!aiTimer.isActive()) { aiTimer.setInterval(QSettings().value("fast",false).toBool()?180:650); aiTimer.start(); } }
    else aiTimer.stop();
}
void Window::clickCard(int i) {
    if(!net.canAct())return;
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
void Window::showRoundSettings() {
    if(net.mode()==NetworkSession::Mode::Client)return;
    aiTimer.stop();setupOpen=true;
    QDialog dialog(this);dialog.setWindowTitle("Rounds — Turnover Golf");auto *layout=new QVBoxLayout(&dialog);
    layout->setContentsMargins(22,20,22,20);layout->setSpacing(18);
    auto *form=new QFormLayout;auto *rounds=new QSpinBox;rounds->setObjectName("settingsRoundCount");rounds->setRange(g.state().hole,99);rounds->setValue(g.state().holes);form->addRow("Number of rounds",rounds);layout->addLayout(form);
    auto *note=new QLabel(QString("Applies to this match and becomes the default for new games.\n\nCompleted scores are kept. You are on round %1, so the total cannot be lower than %1.").arg(g.state().hole));note->setWordWrap(true);layout->addWidget(note);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.ensurePolished();
    const int width=std::max(480,dialog.fontMetrics().averageCharWidth()*48);
    const int textWidth=width-layout->contentsMargins().left()-layout->contentsMargins().right();
    note->setMinimumHeight(note->heightForWidth(textWidth));
    layout->activate();
    const int height=std::max(layout->totalHeightForWidth(width),layout->minimumSize().height());
    dialog.setMinimumSize(width,height);dialog.resize(width,height);
    if(dialog.exec()==QDialog::Accepted && net.act("rounds",rounds->value()) && persistence)QSettings().setValue("rounds",rounds->value());
    setupOpen=false;updateUi();
}
void Window::newGame() {
    aiTimer.stop();setupOpen=true;
    QDialog dialog(this);dialog.setWindowTitle("Players and rounds — Turnover Golf");
    dialog.ensurePolished();const auto available=dialog.screen()->availableGeometry();
    const int setupWidth=std::min(std::max(620,dialog.fontMetrics().averageCharWidth()*60),available.width()-60);
    dialog.setMinimumWidth(std::min(620,setupWidth));dialog.resize(setupWidth,std::min(800,available.height()-100));
    auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(18,16,18,16);layout->setSpacing(14);auto *tabs=new QTabWidget;tabs->setObjectName("setupTabs");layout->addWidget(tabs,1);
    auto addPage=[&](QWidget *page,const QString &title){auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setWidget(page);tabs->addTab(scroll,title);};
    auto *create=new QWidget;auto *createLayout=new QVBoxLayout(create);createLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);createLayout->setSpacing(14);addPage(create,"Set up game");
    auto *intro=new QLabel("You are player 1. Choose who fills each other seat.");intro->setWordWrap(true);createLayout->addWidget(intro);
    auto *form=new QFormLayout;form->setRowWrapPolicy(QFormLayout::WrapLongRows);form->setVerticalSpacing(12);createLayout->addLayout(form);
    auto *count=new QComboBox;count->setObjectName("playerCount");for(int n=2;n<=4;++n)count->addItem(QString("%1 — you + %2 %3").arg(n).arg(n-1).arg(n==2?"player":"players"),n-1);count->setCurrentIndex(count->findData(std::clamp(g.state().opponents,1,3)));form->addRow("Total players",count);
    auto *rounds=new QSpinBox;rounds->setObjectName("roundCount");rounds->setRange(1,99);rounds->setValue(g.state().holes);form->addRow("Rounds",rounds);
    std::array<QComboBox *,3> seats;std::array<QLabel *,3> seatLabels;
    for(int i=0;i<3;++i){seats[i]=new QComboBox;seats[i]->setObjectName(QString("seatType%1").arg(i+1));seats[i]->addItem("Computer",false);seats[i]->addItem("Online human",true);if(net.online() && i<g.state().opponents && !net.isComputer(i+1))seats[i]->setCurrentIndex(1);seatLabels[i]=new QLabel(QString("Player %1").arg(i+2));form->addRow(seatLabels[i],seats[i]);}
    QSettings settings;
    auto *name=new QLineEdit(settings.value("onlineName","Host").toString());name->setMaxLength(24);name->setObjectName("hostName");auto *nameLabel=new QLabel("Your online name");form->addRow(nameLabel,name);
    auto *port=new QSpinBox;port->setObjectName("hostPort");port->setRange(1024,65535);port->setValue(settings.value("onlinePort",45839).toInt());auto *portLabel=new QLabel("Host port");form->addRow(portLabel,port);
    auto *note=new QLabel("Computer players are ready immediately. Online humans join using your address, port and room code. The match waits for all chosen human seats.");note->setWordWrap(true);createLayout->addWidget(note);createLayout->addStretch();
    auto *joinPage=new QWidget;auto *joinLayout=new QVBoxLayout(joinPage);joinLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);joinLayout->setSpacing(14);addPage(joinPage,"Join online game");
    auto *joinForm=new QFormLayout;joinForm->setRowWrapPolicy(QFormLayout::WrapLongRows);joinForm->setVerticalSpacing(12);joinLayout->addLayout(joinForm);
    auto *joinName=new QLineEdit(settings.value("onlineName","Player").toString());joinName->setMaxLength(24);joinName->setObjectName("joinName");joinForm->addRow("Your name",joinName);
    auto *address=new QLineEdit(settings.value("lastHost","").toString());address->setPlaceholderText("Host IP address or hostname");address->setMaxLength(253);address->setObjectName("joinAddress");joinForm->addRow("Host address",address);
    auto *joinPort=new QSpinBox;joinPort->setObjectName("joinPort");joinPort->setRange(1,65535);joinPort->setValue(settings.value("lastPort",45839).toInt());joinForm->addRow("Port",joinPort);
    auto *code=new QLineEdit;code->setMaxLength(12);code->setPlaceholderText("12-character code from the host");code->setObjectName("joinCode");joinForm->addRow("Room code",code);
    auto *joinNote=new QLabel("Both people run Turnover Golf. Enter the details shown in the host's Connection details.\n\nOn the same network, use the host's local address. Across the internet, use a VPN address or a reachable public address with the host port forwarded.\n\nThe host chooses the players and number of rounds.");joinNote->setWordWrap(true);joinLayout->addWidget(joinNote);joinLayout->addStretch();
    auto *replacement=new QLabel(net.online()?"Starting or joining another game leaves the current online room.":"A new local match replaces the current saved match.");replacement->setWordWrap(true);layout->addWidget(replacement);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setMinimumWidth(170);layout->addWidget(buttons);
    auto refresh=[&]{
        bool online=false;for(int i=0;i<3;++i){bool visible=i<count->currentData().toInt();seats[i]->setVisible(visible);seatLabels[i]->setVisible(visible);online=online || (visible && seats[i]->currentData().toBool());}
        name->setVisible(online);nameLabel->setVisible(online);port->setVisible(online);portLabel->setVisible(online);buttons->button(QDialogButtonBox::Ok)->setText(tabs->currentIndex()==1?"Join game":online?"Host game":"Start game");
    };
    connect(count,&QComboBox::currentIndexChanged,&dialog,refresh);connect(tabs,&QTabWidget::currentChanged,&dialog,refresh);for(auto *seat:seats)connect(seat,&QComboBox::currentIndexChanged,&dialog,refresh);refresh();
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        if(tabs->currentIndex()==1){
            if(address->text().trimmed().isEmpty() || code->text().trimmed().size()!=12){QMessageBox::information(&dialog,"Join online game","Enter the host address and 12-character room code.");return;}
            net.join(address->text(),quint16(joinPort->value()),code->text(),joinName->text());
            if(persistence){settings.setValue("onlineName",joinName->text());settings.setValue("lastHost",address->text().trimmed());settings.setValue("lastPort",joinPort->value());}
        }else{
            QVector<bool> remote;for(int i=0;i<count->currentData().toInt();++i)remote.append(seats[i]->currentData().toBool());
            if(remote.contains(true)){
                if(!net.host(remote,rounds->value(),name->text(),quint16(port->value()))){QMessageBox::warning(&dialog,"Could not host game",net.errorString());return;}
                if(persistence){settings.setValue("onlineName",name->text());settings.setValue("onlinePort",port->value());}
            }else net.offline(count->currentData().toInt(),rounds->value());
            if(persistence){settings.setValue("opponents",count->currentData().toInt());settings.setValue("rounds",rounds->value());}
        }
        dialog.accept();
    });
    dialog.exec();setupOpen=false;buildOpponents();changed();
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
    text->setHtml(R"(<h1>Eight cards. Custom rounds.</h1><p>Finish with the lowest total score. Choose 1–99 rounds and 2–4 total players, including you, in Settings → Players or Players / New game. You can also change the round count in Settings → Rounds; completed scores stay intact. In online games, the host sets the round count. Each other seat can be a computer or an online human. You always sit south; opponents occupy north for two players, north and west for three, and north, west and east for four. Cards and rows face their seat. Each hand has two rows of four cards.</p>
      <h2>Start and take a turn</h2><p>All eight cards are dealt face down. Turn seven face up before the first draw, leaving the eighth card face down in its place. Computers do the same. Draw from the deck or the discard pile. Replace any face-up card with your draw; the old card goes on the discard pile.</p><p>A card taken from the discard pile must be played. A card drawn from the deck can instead be discarded to end your turn and keep playing.</p>
      <h2>The final reveal</h2><p><b>On your turn, you may flip your final face-down card without drawing.</b> Click the card or choose <b>Flip final card · end round</b>. It keeps its original value. Everyone else immediately turns their remaining face-down card face up and scores, with no extra turns.</p><p>You can instead draw from either pile and replace your final card with that draw to end the round. The replaced card goes to the discard pile. Once you draw, resolve that draw by replacing a card or discarding a deck draw. Both ways of ending apply to every human and computer.</p>
      <h2>Scoring</h2><p>Cards have their printed value: 0–12, or −5. There are eight of each 0–12 and four −5 cards (108 total).</p><ul><li>Two equal cards in the same column score 0.</li><li>Two −5 cards keep their values: −10.</li><li>Two, three, or four completed pairs of the <b>same number</b> earn an extra −10, −15, or −20 bonus.</li><li>Four −5 cards arranged as two vertical pairs score −30.</li><li>Columns do not need to be adjacent. Side-by-side cards alone do not cancel.</li></ul>
      <h2>At the table</h2><p>Shown points include only face-up cards and completed bonuses; ? marks a column with a face-down card. Computers cannot see face-down values. They build their hands for at least six turns and aim for low scores, vertical pairs and matching bonuses before ending. Turns begin with a different player each round. Tied final totals share the win.</p><p>When the draw pile runs out, the used discards are shuffled into a new draw pile, keeping the top discard available. Play continues; face-down cards stay down until a player flips or replaces their final card.</p>
      <h2>Online players</h2><p>In Settings → Players, set any other seat to Online human to host a room. Use Connection details to copy the address, port and room code. Other humans select the Join online game tab. A mix of computers and humans can share one match. Your hand always appears at the bottom.</p><p>Across the internet, connect through a VPN or a reachable public address with the host TCP port forwarded. The match pauses if a human disconnects; use Reconnect in the same window. The host advances rounds. Keep the host open for the entire match. Online matches are not restored after the host closes.</p><h2>Controls</h2><p>Click cards or use 1–4 for the top row and 5–8 for the bottom. D draws from the deck, T takes a discard, X discards your draw. Ctrl+N opens a new game. F1 opens this guide.</p><p>Offline matches save after each action and resume on the next launch. Online games leave your offline save intact.</p><p>Uses Play Nine card values and matching scores, with your house rules for setup and the simultaneous final reveal. <a href="https://playnine.com/pages/how-to-play">Publisher’s rules</a></p>)");
    v->addWidget(text); auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close); v->addWidget(buttons); connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject); dialog.exec(); if(resume) updateUi();
}
void Window::closeEvent(QCloseEvent *event) { saveGame(); if(persistence) QSettings().setValue("geometry",saveGeometry()); QMainWindow::closeEvent(event); }
void Window::capture(const QString &path) { grab().save(path); }
