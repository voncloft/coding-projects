// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Velvet Casino contributors
#include "window.h"
#include <algorithm>
#include <cmath>
namespace casino {
static double profileBalance() {
    bool ok=false;double amount=QSettings().value("profile/balance",1000.0).toDouble(&ok);
    return ok&&std::isfinite(amount)&&amount>=0&&amount<=10000000?std::floor(amount*2)/2:1000;
}

static QLabel *textLabel(const QString &text,const QString &style={}) {
    auto *label=new QLabel(text);label->setTextFormat(Qt::PlainText);if(!style.isEmpty())label->setStyleSheet(style);return label;
}
Window::Window(bool startup,bool animateDeals):network(&game,this),sounds(this) {
    sounds.setAnimateDeals(animateDeals);
    setWindowTitle(QString("Velvet Casino %1").arg(CASINO_VERSION));setMinimumSize(1040,730);resize(1440,960);
    setStyleSheet(R"(
        QMainWindow, QDialog { background: #101b20; color: #e6ede8; }
        QWidget { color: #e6ede8; font-family: 'Noto Sans'; font-size: 13px; }
        QLabel { background: transparent; }
        QPushButton, QToolButton { background: #24363c; border: 1px solid #3a5055; border-radius: 7px; padding: 10px 16px; color: #e4ece7; }
        QPushButton:hover, QToolButton:hover { background: #34504f; border-color: #c9ad70; }
        QPushButton:disabled { color: #627779; background: #19282d; border-color: #29393c; }
        QPushButton#primary, QPushButton#dealButton { background: #d7bb7c; color: #17282a; font-weight: bold; border-color: #dfc98d; }
        QPushButton#primary:hover, QPushButton#dealButton:hover { background: #ecd6a0; }
        QPushButton:checked { background: #635236; border-color: #d7bb7c; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background: #1c2c32; border: 1px solid #3a5055; border-radius: 5px; padding: 7px; min-height: 20px; }
        QComboBox QAbstractItemView { background: #1c2c32; selection-background-color: #486a61; color: #f0f3eb; }
        QTabWidget::pane { border: 1px solid #34474b; border-radius: 6px; }
        QTabBar::tab { background: #1b2c32; padding: 12px 24px; border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { background: #263c40; border-bottom-color: #d7bb7c; }
        QGroupBox { border: 1px solid #34494c; border-radius: 8px; margin-top: 16px; padding: 16px; }
        QGroupBox::title { subcontrol-origin: margin; left: 14px; color: #d7bb7c; }
        QListWidget, QTextBrowser { background: #15252b; border: 1px solid #2b4145; border-radius: 8px; padding: 8px; }
        QListWidget::item { padding: 8px 2px; border-bottom: 1px solid #253b3e; }
        QCheckBox { spacing: 7px; }
        QCheckBox::indicator { width: 17px; height: 17px; }
        QScrollArea { border: none; background: transparent; }
        QToolTip { color: #eef4ef; background: #213a3e; border: 1px solid #d7bb7c; padding: 6px; }
        QMenu { background: #1c2c32; border: 1px solid #3a5055; padding: 6px; }
        QMenu::item { padding: 9px 22px; }
        QMenu::item:selected { background: #34504f; }
    )");
    auto *root=new QWidget;setCentralWidget(root);auto *outer=new QVBoxLayout(root);outer->setContentsMargins(22,14,22,8);outer->setSpacing(6);
    auto *header=new QHBoxLayout;
    auto *brand=textLabel("♠  VELVET", "font-size: 23px; font-weight: bold; color: #dfc58c;");header->addWidget(brand);header->addSpacing(20);
    gameLabel=textLabel("Your private casino", "font-size: 16px; color: #bccdc5;");header->addWidget(gameLabel);header->addStretch();
    auto *newTable=new QPushButton("New table");newTable->setObjectName("newTable");header->addWidget(newTable);connect(newTable,&QPushButton::clicked,this,&Window::setup);
    auto *settingsButton=new QPushButton("Settings");settingsButton->setObjectName("settingsButton");auto *settingsMenu=new QMenu(settingsButton);settingsMenu->addAction("Player profile…",this,&Window::profileSettings);settingsMenu->addAction("Chip values…",this,&Window::chipSettings);auto *exchangeAction=settingsMenu->addAction("Exchange chips…",this,&Window::exchangeChips);exchangeAction->setObjectName("exchangeChipsAction");auto *decksAction=settingsMenu->addAction("Blackjack decks…",this,&Window::deckSettings);decksAction->setObjectName("blackjackDecksAction");auto *anteAction=settingsMenu->addAction("Poker ante…",this,&Window::anteSettings);anteAction->setObjectName("pokerAnteAction");settingsMenu->addAction("Card and table themes…",this,&Window::appearance);auto *soundAction=settingsMenu->addAction("Table sounds");soundAction->setObjectName("chipSoundsAction");soundAction->setCheckable(true);soundAction->setChecked(QSettings().value("sound/enabled",true).toBool());sounds.setEnabled(soundAction->isChecked());connect(soundAction,&QAction::toggled,this,[this](bool enabled){sounds.setEnabled(enabled);QSettings().setValue("sound/enabled",enabled);});settingsButton->setMenu(settingsMenu);header->addWidget(settingsButton);
    auto *rulesButton=new QPushButton("How to play");header->addWidget(rulesButton);connect(rulesButton,&QPushButton::clicked,this,&Window::rules);
    pauseButton=new QPushButton("Pause");pauseButton->setCheckable(true);header->addWidget(pauseButton);connect(pauseButton,&QPushButton::toggled,this,[this](bool p){paused=p;pauseButton->setText(p?"Resume":"Pause");network.setPaused(p);refresh();});
    outer->addLayout(header);
    connectionLabel=textLabel("Local table","color: #849e9d; font-size: 11px;");connectionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);connectionLabel->setWordWrap(true);outer->addWidget(connectionLabel);
    auto *middle=new QHBoxLayout;middle->setSpacing(16);table=new Table;auto *tableColumn=new QVBoxLayout;tableColumn->setSpacing(6);tableColumn->addWidget(table,1);middle->addLayout(tableColumn,1);
    auto *rail=new QWidget;rail->setObjectName("informationRail");rail->setStyleSheet("QWidget#informationRail { background: #101b20; }");rail->setMinimumHeight(560);auto *railLayout=new QVBoxLayout(rail);railLayout->setContentsMargins(0,12,0,12);
    railLayout->addWidget(textLabel("AT THE TABLE","font-size: 11px; font-weight: bold; letter-spacing: 2px; color: #d7bb7c;"));
    balanceLabel=textLabel("1,000 play chips","font-size: 18px; font-weight: bold;");balanceLabel->setWordWrap(true);railLayout->addWidget(balanceLabel);
    auto *chipLabel=textLabel("Play chips · no cash value","color: #8ba5a0; font-size: 11px;");railLayout->addWidget(chipLabel);railLayout->addSpacing(18);
    railLayout->addWidget(textLabel("CHIP VALUES","font-size: 11px; color: #8ba5a0;"));
    auto *legend=new QGridLayout;legend->setVerticalSpacing(2);
    for(int i=0;i<5;++i){chipIcons[i]=new QLabel;chipIcons[i]->setFixedSize(34,36);chipLabels[i]=textLabel(ChipNames[i]);chipLabels[i]->setObjectName(QString("chipLegend%1").arg(ChipNames[i]));legend->addWidget(chipIcons[i],i,0);legend->addWidget(chipLabels[i],i,1);}
    legend->setColumnStretch(1,1);railLayout->addLayout(legend);railLayout->addSpacing(12);
    railLayout->addWidget(textLabel("HAND HISTORY","font-size: 11px; color: #8ba5a0;"));activity=new QListWidget;activity->setWordWrap(true);activity->setFocusPolicy(Qt::NoFocus);railLayout->addWidget(activity,1);
    fast=new QCheckBox("Fast computer turns");fast->setObjectName("fastTurns");railLayout->addWidget(fast);
    autoDeal=new QCheckBox("Auto-deal next hand");autoDeal->setObjectName("autoDeal");railLayout->addWidget(autoDeal);auto *railScroll=new QScrollArea;railScroll->setWidgetResizable(true);railScroll->setFixedWidth(215);railScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);railScroll->setWidget(rail);middle->addWidget(railScroll);outer->addLayout(middle,1);
    wagerBar=new QWidget;wagerBar->setObjectName("wagerBar");auto *wagers=new QHBoxLayout(wagerBar);wagers->setContentsMargins(0,0,0,0);wagers->addStretch();
    auto betPolicy=wagerBar->sizePolicy();betPolicy.setRetainSizeWhenHidden(true);wagerBar->setSizePolicy(betPolicy);wagerBar->setFixedHeight(58);
    wagerLabel=textLabel("Choose your bet");wagerLabel->setParent(wagerBar);wagerLabel->setObjectName("wagerLabel");wagerLabel->hide();
    handCount=new QSpinBox;handCount->setObjectName("blackjackHandCount");handCount->setRange(1,4);handCount->setPrefix("Hands: ");handCount->setToolTip("Play 1–4 blackjack hands. Choose a separate chip bet for each hand.");wagers->addWidget(handCount);connect(handCount,&QSpinBox::valueChanged,this,[this](int count){for(int h=count;h<4;++h)draftHandCounts[h]={};refresh();});
    bettingHand=new QComboBox;bettingHand->setObjectName("blackjackBettingHand");bettingHand->setMinimumWidth(142);bettingHand->setToolTip("Choose which blackjack hand receives the chips you select.");connect(bettingHand,&QComboBox::currentIndexChanged,this,[this]{refresh();});
    for(int i=0;i<5;++i){auto *b=new QToolButton;wagerButtons[i]=b;b->setObjectName(QString("betChip%1").arg(ChipNames[i]));b->setIconSize(QSize(32,34));b->setFixedHeight(58);b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);b->setStyleSheet("QToolButton { padding: 3px 8px; font-size: 11px; }");b->installEventFilter(this);wagers->addWidget(b);connect(b,&QToolButton::clicked,this,[this,i]{adjustChip(i,1);});}
    exchangeButton=new QPushButton("Exchange");exchangeButton->setObjectName("exchangeChipsButton");exchangeButton->setStyleSheet("padding: 6px 9px; font-size: 11px;");exchangeButton->setToolTip("Choose how your balance is divided into colored chip stacks.");wagers->addWidget(exchangeButton);connect(exchangeButton,&QPushButton::clicked,this,&Window::exchangeChips);
    clearButton=new QPushButton("Clear");clearButton->setObjectName("clearWagerButton");wagers->addWidget(clearButton);connect(clearButton,&QPushButton::clicked,this,[this]{submitWager(true);});
    placeButton=new QPushButton("Place bet");placeButton->setObjectName("placeWagerButton");wagers->addWidget(placeButton);connect(placeButton,&QPushButton::clicked,this,[this]{submitWager();});wagers->addStretch();
    auto *footer=new QHBoxLayout;footer->addStretch();footer->addWidget(bettingHand);turnLabel=textLabel("Ready when you are","font-size: 15px; font-weight: bold;");turnLabel->setParent(root);turnLabel->hide();
    const QStringList keys{"hit","stand","double","split","fold","check","call","raise","allin","draw"};
    const QStringList labels{"Hit  [H]","Stand  [S]","Double down  [D]","Split","Fold  [F]","Check  [C]","Call  [C]","Raise  [R]","All in","Keep all 5"};
    for(int i=0;i<keys.size();++i){auto *button=new QPushButton(labels[i]);button->setObjectName(keys[i]+"Button");actions[keys[i]]=button;footer->addWidget(button);connect(button,&QPushButton::clicked,this,[this,key=keys[i]]{perform(key);});}
    dealButton=new QPushButton("Deal hand");dealButton->setObjectName("dealButton");footer->addWidget(dealButton);connect(dealButton,&QPushButton::clicked,this,[this]{if(game.config().kind!=Kind::Blackjack&&network.ready()&&!paused&&!network.isClient()){game.deal();nextDealTicks=0;network.publish();refresh();}});
    cashoutButton=new QPushButton("Cash out");cashoutButton->setObjectName("cashoutButton");footer->addWidget(cashoutButton);connect(cashoutButton,&QPushButton::clicked,this,[this]{if(network.isClient())network.sendCashout();else{game.requestCashout();network.publish();}refresh();});
    footer->addStretch();tableColumn->addLayout(footer);tableColumn->addWidget(wagerBar);
    statusLabel=textLabel("Choose a game to begin.");statusLabel->setParent(root);statusLabel->setObjectName("roundStatus");statusLabel->hide();

    for(auto pair:std::vector<std::pair<QString,QString>>{{"H","hit"},{"S","stand"},{"D","double"},{"F","fold"},{"R","raise"}}) {
        auto *shortcut=new QShortcut(QKeySequence(pair.first),this);connect(shortcut,&QShortcut::activated,this,[this,key=pair.second]{if(actions[key]->isVisible()&&actions[key]->isEnabled())perform(key);});
    }
    auto *checkCall=new QShortcut(QKeySequence("C"),this);connect(checkCall,&QShortcut::activated,this,[this]{for(auto key:{"check","call"})if(actions[key]->isVisible()&&actions[key]->isEnabled())perform(key);});
    connect(table,&Table::selectionChanged,this,[this](int count){actions["draw"]->setText(count?QString("Draw %1 card%2").arg(count).arg(count==1?"":"s"):"Keep all 5");});
    connect(table,&Table::inspectedHandChanged,this,&Window::refresh);
    connect(table,&Table::betConfirmed,this,[this]{
        if(table->state()["phase"]=="blackjack"){
            if(actions["hit"]->isVisible()&&actions["hit"]->isEnabled())perform("hit");
            return;
        }
        if(table->state()["phase"]=="draw"){
            if(actions["draw"]->isVisible()&&actions["draw"]->isEnabled())perform("draw");
            return;
        }
        if(placeButton->isVisible()&&placeButton->isEnabled()){
            submitWager();
        }
        else if(table->state()["kind"].toInt()!=0)confirmPokerChips();
    });
    connect(table,&Table::standRequested,this,[this]{
        if(actions["stand"]->isVisible()&&actions["stand"]->isEnabled())perform("stand");
    });
    connect(&sounds,&ChipSounds::eventStarted,this,&Window::applyPlaybackEvent);
    connect(&sounds,&ChipSounds::sequenceFinished,this,[this]{displayState=latestState;network.setInputBlocked(false);refresh();});
    connect(&network,&Network::changed,this,&Window::refresh);
    connect(&network,&Network::profileSaved,this,[this](const QString &name,double balance){
        QSettings settings;settings.setValue("profile/name",name);settings.setValue("profile/balance",balance);settings.sync();draftCounts={};draftKey.clear();
    });
    connect(&network,&Network::notice,this,[this](const QString &s){statusLabel->setText(s);connectionLabel->setText(network.description()+" · "+s);});
    QSettings settings;tableTheme=std::clamp(settings.value("tableTheme",0).toInt(),0,19);faceTheme=std::clamp(settings.value("faceTheme",0).toInt(),0,19);backTheme=std::clamp(settings.value("backTheme",0).toInt(),0,19);
    fast->setChecked(settings.value("fast",false).toBool());autoDeal->setChecked(settings.value("autoDeal",false).toBool());table->setThemes(tableTheme,faceTheme,backTheme);
    Config initial;initial.ante=std::clamp(settings.value("poker/ante",5).toInt(),1,1000000);initial.blackjackDecks=std::clamp(settings.value("blackjack/decks",8).toInt(),1,8);for(int i=0;i<5;++i){bool ok=false;int value=settings.value(QString("chipValues/%1").arg(ChipNames[i]),DefaultChipValues[i]).toInt(&ok);initial.chipValues[i]=ok&&value>=1&&value<=1000000?value:DefaultChipValues[i];}
    initial.players[0].name=settings.value("profile/name","You").toString();initial.players[0].startingBalance=profileBalance();game.configure(initial);refresh();timer.setInterval(250);connect(&timer,&QTimer::timeout,this,&Window::tick);timer.start();
    if(startup)QTimer::singleShot(0,this,&Window::setup);
}
void Window::startConfig(const Config &c,quint16 port) {
    sounds.reset();displayState={};latestState={};network.disconnectSession();savedHand=0;savedCashout=false;draftKey.clear();draftCounts={};game.configure(c);pauseButton->setChecked(false);nextDealTicks=0;
    bool remote=std::any_of(c.players.begin(),c.players.end(),[](auto p){return p.control==Control::Remote;});
    if(remote&&!network.host(port))QMessageBox::warning(this,"Could not host","The port could not be opened. Choose another port in New table.");
    refresh();
}
void Window::setup() {
    dialogOpen=true;sounds.startIntro();
    auto introGuard=qScopeGuard([this]{sounds.stopIntro();});
    QDialog dialog(this);dialog.setObjectName("setupDialog");dialog.setWindowTitle("Choose your game — Velvet Casino");dialog.resize(690,730);
    auto *outer=new QVBoxLayout(&dialog);outer->setContentsMargins(24,22,24,22);outer->setSpacing(14);
    outer->addWidget(textLabel("Welcome to Velvet","font-size: 28px; font-weight: bold; color: #dfc58c;"));
    outer->addWidget(textLabel("Choose your game. Set your seats. Make it your table.","color: #99b1a9;"));
    auto *tabs=new QTabWidget;outer->addWidget(tabs,1);auto *create=new QWidget;create->setObjectName("setupCreatePage");create->setStyleSheet("QWidget#setupCreatePage { background: #101b20; }");auto *layout=new QVBoxLayout(create);layout->setContentsMargins(18,18,18,18);layout->setSpacing(12);auto *createScroll=new QScrollArea;createScroll->setWidgetResizable(true);createScroll->setWidget(create);tabs->addTab(createScroll,"Create a table");
    auto *choice=new QHBoxLayout;auto *blackjack=new QPushButton("♠  Blackjack");blackjack->setObjectName("chooseBlackjack");auto *poker=new QPushButton("♦  Poker");poker->setObjectName("choosePoker");blackjack->setCheckable(true);poker->setCheckable(true);blackjack->setMinimumHeight(54);poker->setMinimumHeight(54);auto *group=new QButtonGroup(&dialog);group->addButton(blackjack,0);group->addButton(poker,1);choice->addWidget(blackjack);choice->addWidget(poker);layout->addLayout(choice);
    QSettings settings;int lastKind=std::clamp(settings.value("kind",0).toInt(),0,4);blackjack->setChecked(lastKind==0);poker->setChecked(lastKind!=0);
    auto *form=new QFormLayout;form->setSpacing(10);auto *variant=new QComboBox;variant->setObjectName("pokerVariant");for(int k=1;k<=4;++k)variant->addItem(gameName(Kind(k)),k);variant->setCurrentIndex(std::max(0,lastKind-1));form->addRow("Poker game",variant);
    auto *jacks=new QCheckBox("Jacks or better to open");jacks->setObjectName("jacksOption");jacks->setChecked(settings.value("jacks",false).toBool());jacks->setToolTip("Draw and stud: a pair of jacks or any better poker hand is needed to open. Once opened, everyone may call or raise.");form->addRow("Opening rule",jacks);
    auto *decks=new QSpinBox;decks->setObjectName("setupBlackjackDecks");decks->setRange(1,8);decks->setValue(std::clamp(settings.value("blackjack/decks",8).toInt(),1,8));form->addRow("Blackjack decks",decks);
    auto *ante=new QSpinBox;ante->setObjectName("setupPokerAnte");ante->setRange(1,1000000);ante->setValue(std::clamp(settings.value("poker/ante",5).toInt(),1,1000000));ante->setSuffix(" units");form->addRow("Starting ante",ante);
    auto *count=new QSpinBox;count->setObjectName("playerCount");count->setRange(lastKind==0?1:2,5);count->setValue(settings.value("players",2).toInt());form->addRow("Players (dealer is extra)",count);
    layout->addLayout(form);
    auto *grid=new QGridLayout;grid->addWidget(textLabel("SEAT", "color: #819e97; font-size: 10px;"),0,0);grid->addWidget(textLabel("NAME", "color: #819e97; font-size: 10px;"),0,1);grid->addWidget(textLabel("CONTROL", "color: #819e97; font-size: 10px;"),0,2);
    std::array<QLineEdit *,5> names;std::array<QComboBox *,5> controls;std::array<QLabel *,5> seatLabels;
    QStringList defaults{settings.value("profile/name","You").toString(),"Ruby","Jasper","Ivy","Ace"};
    for(int i=0;i<5;++i){seatLabels[i]=textLabel(QString::number(i+1));names[i]=new QLineEdit(i==0?defaults[0]:settings.value(QString("name%1").arg(i),defaults[i]).toString());names[i]->setMaxLength(24);names[i]->setObjectName(QString("playerName%1").arg(i));controls[i]=new QComboBox;controls[i]->setObjectName(QString("seatControl%1").arg(i));if(i==0)controls[i]->addItem("You on this computer",int(Control::Human));controls[i]->addItem("Computer",int(Control::Computer));controls[i]->addItem("Network player",int(Control::Remote));int index=controls[i]->findData(settings.value(QString("control%1").arg(i),i==0?0:1).toInt());controls[i]->setCurrentIndex(std::max(0,index));grid->addWidget(seatLabels[i],i+1,0);grid->addWidget(names[i],i+1,1);grid->addWidget(controls[i],i+1,2);}
    layout->addLayout(grid);
    auto *chipsForm=new QFormLayout;auto *bankroll=new QSpinBox;bankroll->setRange(100,1000000);bankroll->setSingleStep(100);bankroll->setValue(settings.value("bankroll",1000).toInt());chipsForm->addRow("Computer starting chips",bankroll);
    chipsForm->addRow("Your saved bankroll",textLabel(formatUnits(profileBalance())+" · edit in Settings → Player profile"));
    auto *unit=new QSpinBox;unit->setRange(1,1000);unit->setSingleStep(4);unit->setValue(settings.value("unit",20).toInt());chipsForm->addRow("Computer target wager",unit);
    auto *cashout=new QSpinBox;cashout->setObjectName("cashoutTarget");cashout->setRange(0,10000000);cashout->setSingleStep(100);cashout->setSpecialValueText("No automatic cash-out");cashout->setValue(settings.value("cashoutTarget",0).toInt());chipsForm->addRow("Cash-out balance",cashout);
    auto *port=new QSpinBox;port->setObjectName("hostPort");port->setRange(1024,65535);port->setValue(settings.value("port",45821).toInt());chipsForm->addRow("Network port",port);layout->addLayout(chipsForm);
    auto *note=textLabel("Computers fill any seats you choose—even every seat. Network players join from their own computers using the host address and room code.","font-size: 11px; color: #9db4ae;");note->setWordWrap(true);layout->addWidget(note);layout->addStretch();
    auto update=[&]{bool bj=blackjack->isChecked();form->setRowVisible(variant,!bj);form->setRowVisible(decks,bj);form->setRowVisible(ante,!bj&&variant->currentData().toInt()!=int(Kind::Holdem));count->setMinimum(bj?1:2);jacks->setEnabled(!bj&&variant->currentData().toInt()!=int(Kind::Holdem));form->setRowVisible(jacks,jacks->isEnabled());qobject_cast<QLabel *>(form->labelForField(count))->setText(bj?"Players (dealer is extra)":"Players");qobject_cast<QLabel *>(chipsForm->labelForField(unit))->setText(bj?"Computer target wager":"Poker minimum bet");unit->setMinimum(bj?1:4);unit->setSingleStep(bj?1:4);if(!bj&&unit->value()%4)unit->setValue((unit->value()+3)/4*4);for(int i=0;i<5;++i){bool show=i<count->value();names[i]->setVisible(show);controls[i]->setVisible(show);seatLabels[i]->setVisible(show);}};
    connect(blackjack,&QPushButton::toggled,&dialog,update);connect(variant,&QComboBox::currentIndexChanged,&dialog,update);connect(count,&QSpinBox::valueChanged,&dialog,update);update();
    auto *joinPage=new QWidget;auto *joinLayout=new QVBoxLayout(joinPage);joinLayout->setContentsMargins(24,24,24,24);tabs->addTab(joinPage,"Join a table");
    auto *joinIntro=textLabel("Join a friend’s casino","font-size: 22px; font-weight: bold; color: #dfc58c;");joinLayout->addWidget(joinIntro);auto *joinForm=new QFormLayout;joinForm->setSpacing(16);
    auto *address=new QLineEdit(settings.value("joinAddress","").toString());address->setPlaceholderText("192.168.1.25 or a host name");address->setObjectName("joinAddress");joinForm->addRow("Host address",address);
    auto *joinPort=new QSpinBox;joinPort->setRange(1024,65535);joinPort->setValue(settings.value("joinPort",45821).toInt());joinForm->addRow("Port",joinPort);
    auto *room=new QLineEdit;room->setMaxLength(8);room->setPlaceholderText("8-character room code");room->setObjectName("roomCode");joinForm->addRow("Room code",room);
    auto *joinName=new QLineEdit(settings.value("profile/name","You").toString());joinName->setMaxLength(24);joinForm->addRow("Your name",joinName);joinLayout->addLayout(joinForm);
    auto *joinNote=textLabel("The host needs an open Network player seat for you. Use the same local network or a private VPN. Your hidden cards stay private. A dropped connection reconnects automatically while this app stays open.","color: #9db4ae;");joinNote->setWordWrap(true);joinLayout->addWidget(joinNote);joinLayout->addStretch();
    auto *buttons=new QHBoxLayout;auto *cancel=new QPushButton("Cancel");auto *start=new QPushButton("Open table");start->setObjectName("primary");buttons->addStretch();buttons->addWidget(cancel);buttons->addWidget(start);outer->addLayout(buttons);connect(cancel,&QPushButton::clicked,&dialog,&QDialog::reject);connect(tabs,&QTabWidget::currentChanged,&dialog,[&](int tab){start->setText(tab?"Join table":"Open table");});
    connect(start,&QPushButton::clicked,&dialog,[&]{
        if(tabs->currentIndex()==1){if(address->text().trimmed().isEmpty()||room->text().trimmed().size()!=8||joinName->text().trimmed().isEmpty()){QMessageBox::information(&dialog,"Join details","Enter the host address, 8-character room code, and your name.");return;}settings.setValue("joinAddress",address->text().trimmed());settings.setValue("joinPort",joinPort->value());settings.setValue("joinName",joinName->text().trimmed());sounds.reset();savedHand=0;savedCashout=false;draftKey.clear();draftCounts={};network.join(address->text().trimmed(),quint16(joinPort->value()),room->text(),joinName->text(),profileBalance());dialog.accept();return;}
        if((!blackjack->isChecked()&&unit->value()%4) || bankroll->value()<unit->value()*5){QMessageBox::information(&dialog,"Chip settings","Computer stacks need at least five wagers. The poker minimum bet must be divisible by 4.");return;}
        Config c;c.ante=ante->value();c.blackjackDecks=decks->value();c.chipValues=game.config().chipValues;c.kind=blackjack->isChecked()?Kind::Blackjack:Kind(variant->currentData().toInt());c.jacks=jacks->isChecked()&&jacks->isEnabled();c.unit=unit->value();c.bankroll=bankroll->value();c.cashoutTarget=cashout->value();c.players.clear();
        for(int i=0;i<count->value();++i){QString name=names[i]->text().simplified();if(name.isEmpty())name=QString("Player %1").arg(i+1);auto control=Control(controls[i]->currentData().toInt());c.players.push_back({name,control,control==Control::Human?profileBalance():-1});settings.setValue(QString("name%1").arg(i),name);settings.setValue(QString("control%1").arg(i),controls[i]->currentData());}
        if(game.active()||network.isClient()||network.isHosting()){if(QMessageBox::question(&dialog,"Start a new table?","This ends the current table. Your profile retains the bankroll from the last completed hand.")!=QMessageBox::Yes)return;}
        settings.setValue("poker/ante",c.ante);settings.setValue("blackjack/decks",c.blackjackDecks);settings.setValue("cashoutTarget",c.cashoutTarget);settings.setValue("kind",int(c.kind));settings.setValue("players",count->value());settings.setValue("jacks",c.jacks);settings.setValue("unit",c.unit);settings.setValue("bankroll",c.bankroll);settings.setValue("port",port->value());startConfig(c,quint16(port->value()));dialog.accept();
    });
    dialog.exec();dialogOpen=false;refresh();
}
void Window::perform(const QString &action) {
    if(paused||dialogOpen||!network.ready()||sounds.isPresenting()||network.remoteState()["inputBlocked"].toBool())return;
    auto picks=action=="draw"?table->discards():std::vector<int>{};
    auto state=table->state();auto values=readChipValues(state["chipValues"].toArray());int amount=0;for(int i=0;i<5;++i)amount+=draftCounts[i]*values[i];
    auto people=state["players"].toArray();int me=state["viewer"].toInt(-1);
    int already=me>=0&&me<people.size()?people[me].toObject()["bet"].toInt():0;
    int raiseTo=action=="raise"&&amount?already+amount:0;
    std::array<int,5> chosen{};
    if(me>=0&&me<people.size()&&state["kind"].toInt()!=0){auto seat=people[me].toObject();int due=action=="raise"?raiseTo-already:action=="allin"?int(seat["chips"].toDouble()):action=="call"?std::min(int(seat["chips"].toDouble()),state["currentBet"].toInt()-already):0;if(amount>0&&amount==due)chosen=draftCounts;}
    if(network.isClient())network.sendAction(action,picks,raiseTo,chosen);
    else {int turn=game.current();if(turn>=0&&game.players()[turn].config.control==Control::Human&&game.act(turn,action,picks,raiseTo,chosen))network.publish();}
    draftKey.clear();draftCounts={};refresh();
}
void Window::confirmPokerChips() {
    auto state=table->state();int me=state["viewer"].toInt(-1);auto people=state["players"].toArray();
    if(me<0||me>=people.size()||state["turn"].toInt(-1)!=me)return;
    auto seat=people[me].toObject();auto values=readChipValues(state["chipValues"].toArray());int amount=0;
    for(int i=0;i<5;++i)amount+=draftCounts[i]*values[i];
    if(amount<=0){
        if(actions["call"]->isEnabled())perform("call");
        else if(actions["check"]->isEnabled())perform("check");
        return;
    }
    int stack=int(seat["chips"].toDouble()),due=std::max(0,state["currentBet"].toInt()-seat["bet"].toInt());
    QString action;
    if(amount==stack&&actions["allin"]->isEnabled())action="allin";
    else if(amount==std::min(stack,due)&&actions["call"]->isEnabled())action="call";
    else if(actions["raise"]->isEnabled())action="raise";
    if(!action.isEmpty())perform(action);
}
bool Window::eventFilter(QObject *object,QEvent *event) {
    if(event->type()==QEvent::MouseButtonPress && static_cast<QMouseEvent *>(event)->button()==Qt::RightButton){for(int i=0;i<5;++i)if(object==wagerButtons[i]){adjustChip(i,-1);return true;}}
    return QMainWindow::eventFilter(object,event);
}
void Window::adjustChip(int index,int delta) {
    if(!wagerBar->isVisible()||!wagerButtons[index]->isEnabled())return;
    auto state=table->state();int me=state["viewer"].toInt(-1);auto people=state["players"].toArray();if(me<0||me>=people.size())return;
    if(state["kind"].toInt()==int(Kind::Holdem)&&state["phase"]=="finished"){
        const QString key=state["soundSession"].toString()+"/"+QString::number(state["hand"].toInt());
        if(delta>0&&clearedResultKey!=key){clearedResultKey=key;sounds.playBet();refresh();}
        return;
    }
    auto s=people[me].toObject();auto values=readChipValues(state["chipValues"].toArray());qint64 amount=0;for(int i=0;i<5;++i)amount+=qint64(draftCounts[i])*values[i];
    bool bj=state["kind"].toInt()==0;
    qint64 total=amount;
    if(bj){total=0;for(int h=0;h<handCount->value();++h)for(int i=0;i<5;++i)total+=qint64(draftHandCounts[h][i])*values[i];}
    qint64 budget=int(s["chips"].toDouble());
    if(delta>0&&total+values[index]>budget)return;
    int changed=std::max(0,draftCounts[index]+delta);if(changed==draftCounts[index])return;
    if(state["phase"]=="finished")clearedResultKey=state["soundSession"].toString()+"/"+QString::number(state["hand"].toInt());
    draftCounts[index]=changed;if(bj)draftHandCounts[std::clamp(bettingHand->currentIndex(),0,3)]=draftCounts;sounds.playBet();refresh();
}
void Window::submitWager(bool clear) {
    if(paused||dialogOpen||!network.ready()||sounds.isPresenting()||network.remoteState()["inputBlocked"].toBool())return;
    auto state=table->state();int me=state["viewer"].toInt(-1);if(me<0)return;
    if(!clear&&state["phase"]=="finished")clearedResultKey=state["soundSession"].toString()+"/"+QString::number(state["hand"].toInt());
    if(state["usesAnte"].toBool()&&(state["phase"]=="ready"||state["phase"]=="finished")){
        if(clear){
            if(state["players"].toArray()[me].toObject()["wagerReady"].toBool()){if(network.isClient())network.sendWager({},true);else{game.clearWager(me);network.publish();}}
        }else if(network.isClient())network.sendAnte();else{game.postAnte(me);network.publish();}
    }else if(state["kind"].toInt()==0){
        if(clear&&!state["players"].toArray()[me].toObject()["wagerReady"].toBool()){
            draftHandCounts[std::clamp(bettingHand->currentIndex(),0,3)]={};draftCounts={};refresh();return;
        }
        HandWagers hands(draftHandCounts.begin(),draftHandCounts.begin()+handCount->value());
        if(network.isClient()){if(clear)network.sendWager({},true);else network.sendHandWagers(hands);}
        else {if(clear)game.clearWager(me);else game.placeHandWagers(me,hands);network.publish();}
    }
    draftCounts={};draftHandCounts={};refresh();
}
void Window::saveProfile(const QJsonObject &state) {
    int me=state["viewer"].toInt(-1);auto people=state["players"].toArray();if(me<0||me>=people.size())return;
    bool ended=state["cashedOut"].toBool();int hand=state["hand"].toInt();
    if(!(ended&&!savedCashout) && !(state["phase"]=="finished"&&hand>savedHand))return;
    auto seat=people[me].toObject();QSettings settings;
    // A next-hand wager can already be in escrow when a network snapshot arrives.
    double balance=seat["chips"].toDouble()+seat["committed"].toInt();
    settings.setValue("profile/name",seat["name"].toString());settings.setValue("profile/balance",balance);
    settings.setValue("profile/hands",settings.value("profile/hands",0).toInt()+std::max(0,hand-savedHand));
    if(ended&&!savedCashout){settings.setValue("profile/cashouts",settings.value("profile/cashouts",0).toInt()+1);settings.setValue("profile/lastCashout",balance);}
    savedHand=hand;savedCashout=ended;settings.sync();
}
void Window::refresh() {
    if(refreshing)return;
    QScopedValueRollback<bool> guard(refreshing,true);
    int viewer=-1;for(int i=0;i<int(game.config().players.size());++i)if(game.config().players[i].control==Control::Human)viewer=i;
    auto state=network.isClient()?network.remoteState():game.snapshot(viewer);
    if(network.isClient()&&state.isEmpty())state={{"players",QJsonArray{}},{"turn",-1},{"viewer",-1},{"game","Joining table…"},{"phase","ready"},{"message","Connecting to the host…"}};
    latestState=state;
    if(displayState["soundSession"]!=state["soundSession"])displayState=state;
    sounds.observe(state);network.setInputBlocked(sounds.isPresenting());
    if(sounds.isPresenting()){
        auto displayed=displayState["players"].toArray(),current=state["players"].toArray();
        for(int i=0;i<std::min(displayed.size(),current.size());++i){auto seat=displayed[i].toObject();seat["name"]=current[i].toObject()["name"];displayed[i]=seat;}
        displayState["players"]=displayed;
        state=displayState;state["phase"]="dealing";state["street"]=playbackMessage;state["message"]=playbackMessage;state["actions"]=QJsonArray{};
    }else{displayState=state;saveProfile(state);}
    if(!sounds.isPresenting()&&state["phase"]=="finished"&&!state["cashedOut"].toBool()){
        const QString key=state["soundSession"].toString()+"/"+QString::number(state["hand"].toInt());
        if(clearedResultKey==key){
            state["dealer"]=QJsonArray{};state["board"]=QJsonArray{};state["dealerValue"]=0;
            auto people=state["players"].toArray();
            for(int i=0;i<people.size();++i){auto seat=people[i].toObject();seat["cards"]=QJsonArray{};seat["hands"]=QJsonArray{};seat["outcome"]="";seat["roundNet"]=0;seat["inHand"]=false;seat["status"]=seat["wagerReady"].toBool()?QString("Bet %1 · ready").arg(seat["wager"].toInt()):QString{};people[i]=seat;}
            state["players"]=people;state["street"]=state["kind"].toInt()==0?"Choose chips before the deal":"Ready to deal";state["message"]=state["street"];
        }
    }
    gameLabel->setText(state["game"].toString()+QString(state["kind"].toInt()==0?"  /  Round %1":"  /  Hand %1").arg(state["hand"].toInt()));connectionLabel->setText(network.description());
    auto people=state["players"].toArray();int turn=state["turn"].toInt(-1),me=state["viewer"].toInt(-1);QString phase=state["phase"].toString();
    bool bj=state["kind"].toInt()==0,waiting=phase=="ready"||phase=="finished",ended=state["cashedOut"].toBool(),enabled=network.ready()&&!paused&&!dialogOpen&&!sounds.isPresenting()&&!latestState["inputBlocked"].toBool();
    QJsonObject seat=me>=0&&me<people.size()?people[me].toObject():QJsonObject{};
    auto values=readChipValues(state["chipValues"].toArray());
    QString key=QString("%1/%2/%3/%4/%5/%6/%7").arg(state["kind"].toInt()).arg(state["hand"].toInt()).arg(turn).arg(me).arg(phase).arg(seat["wagerReady"].toBool()).arg(QString::fromUtf8(QJsonDocument(chipValuesJson(values)).toJson(QJsonDocument::Compact)));
    if(key!=draftKey){draftKey=key;draftCounts={};draftHandCounts={};}
    QStringList allowed;for(auto a:state["actions"].toArray())allowed<<a.toString();
    bool ante=state["usesAnte"].toBool()&&waiting;
    bool holdemFinished=state["kind"].toInt()==int(Kind::Holdem)&&phase=="finished";
    bool choosing=me>=0&&!ended&&(holdemFinished||((bj||ante)&&waiting)||(!bj&&(allowed.contains("raise")||allowed.contains("call")||allowed.contains("allin"))));
    bool editing=choosing&&enabled&&(!(bj||ante)||!seat["wagerReady"].toBool());
    {QSignalBlocker block(handCount);handCount->setMaximum(bj?seat["maxWagerHands"].toInt(4):4);if(bj&&seat["wagerReady"].toBool())handCount->setValue(seat["wagerHands"].toInt(1));}
    handCount->setVisible(bj);handCount->setEnabled(editing);
    int totalWager=0;bool everyHandBet=true;std::array<int,5> aggregate{};QStringList handSummary;
    if(bj){
        for(int h=handCount->value();h<4;++h)draftHandCounts[h]={};
        QSignalBlocker block(bettingHand);int selected=std::clamp(bettingHand->currentIndex(),0,handCount->value()-1);bettingHand->clear();
        for(int h=0;h<handCount->value();++h){int bet=0;for(int i=0;i<5;++i){bet+=draftHandCounts[h][i]*values[i];aggregate[i]+=draftHandCounts[h][i];}totalWager+=bet;everyHandBet&=bet>0;handSummary<<QString("H%1: %2").arg(h+1).arg(bet);bettingHand->addItem(QString("Hand %1 · %2").arg(h+1).arg(bet));}
        bettingHand->setCurrentIndex(selected);draftCounts=draftHandCounts[selected];
    }
    bettingHand->setVisible(bj&&choosing&&handCount->value()>1);bettingHand->setEnabled(editing);
    int amount=0;for(int i=0;i<5;++i)amount+=draftCounts[i]*values[i];
    bool affordable=totalWager<=int(seat["chips"].toDouble());
    wagerBar->setVisible(choosing);clearButton->setVisible(!holdemFinished);clearButton->setEnabled(enabled);placeButton->setVisible(bj||ante);placeButton->setText(ante?QString("Ante %1").arg(seat["anteDue"].toInt()):"Place bet");placeButton->setEnabled(editing&&(ante?seat["anteDue"].toInt()>0:everyHandBet&&totalWager>0&&affordable));
    clearButton->setText((bj||ante)&&seat["wagerReady"].toBool()?"Change bet":bj&&handCount->value()>1?"Clear hand":"Clear");
    if(bj)wagerLabel->setText(seat["wagerReady"].toBool()?QString("%1 hands · Total bet: %2\nWaiting for all bets — cards deal automatically").arg(seat["wagerHands"].toInt(1)).arg(seat["wager"].toInt()):handSummary.join(" · ")+QString(" · Total: %1\n%2").arg(totalWager).arg(!affordable?"Not enough chips":!everyHandBet&&handCount->value()>1?"Choose chips for every hand before placing your bets":"Click the table to bet and deal · right-click chips to remove"));
    else if(holdemFinished)wagerLabel->setText(QString("%1\nNext deal posts blinds %2 / %3").arg(clearedResultKey==state["soundSession"].toString()+"/"+QString::number(state["hand"].toInt())?"Table cleared · ready for the next hand":"Hand complete · click a chip to clear the table").arg(state["unit"].toInt()/2).arg(state["unit"].toInt()));
    else if(ante){
        int due=seat["anteDue"].toInt(),carried=state["carriedPot"].toInt();QString prompt;
        if(seat["wagerReady"].toBool())prompt=QString("Ante %1 placed · waiting for everyone").arg(seat["wager"].toInt());
        else if(!due)prompt="Out of chips · edit your profile or cash out";
        else prompt=QString("Ante %1 · click the table to accept\nEveryone still playing antes before the deal").arg(due);
        if(carried)prompt=QString("Unopened pot carried: %1 · ante again\n").arg(carried)+prompt;
        wagerLabel->setText(prompt);
    }else{
        int stack=int(seat["chips"].toDouble()),due=std::max(0,state["currentBet"].toInt()-seat["bet"].toInt()),target=seat["bet"].toInt()+amount;
        QString intent=due?QString("Call %1 · select %2+ to raise").arg(std::min(stack,due)).arg(state["raiseTo"].toInt()-seat["bet"].toInt()):QString("Check · select %1+ to bet").arg(state["raiseTo"].toInt()-seat["bet"].toInt());
        if(!allowed.contains("raise"))intent=due?QString("Call %1").arg(std::min(stack,due)):"Check";
        if(amount){
            if(amount==stack&&allowed.contains("allin"))intent=QString("All in: %1").arg(amount);
            else if(amount==std::min(stack,due)&&allowed.contains("call"))intent=QString("Call: %1").arg(amount);
            else if(allowed.contains("raise")&&target>=state["raiseTo"].toInt())intent=QString("%1 to %2 · placing %3").arg(state["currentBet"].toInt()?"Raise":"Bet").arg(target).arg(amount);
            else intent=QString("Selected %1 · %2").arg(amount).arg(intent);
        }
        wagerLabel->setText(intent+"\nClick the table to play · right-click chips to remove");
    }
    for(int i=0;i<5;++i){chipLabels[i]->setText(QString("%1   %2").arg(ChipNames[i]).arg(formatUnits(values[i])));QPixmap icon(42,44);icon.fill(Qt::transparent);QPainter painter(&icon);paintChip(painter,QRectF(1,1,40,40),i,values[i]);painter.end();chipIcons[i]->setPixmap(icon.scaled(34,36,Qt::KeepAspectRatio,Qt::SmoothTransformation));wagerButtons[i]->setIcon(QIcon(icon));wagerButtons[i]->setText(QString("%1 ×%2").arg(ChipNames[i]).arg(draftCounts[i]));wagerButtons[i]->setEnabled(editing);wagerButtons[i]->setToolTip(holdemFinished?"Clear the finished hand; blinds are posted on the next deal.":QString("%1 units · left-click add, right-click remove").arg(values[i]));}
    if(choosing&&editing&&!ante&&(bj?totalWager:amount)&&(!bj||affordable)){state["draftHands"]=bj?handCount->value():1;state["draftAmount"]=bj?totalWager:amount;state["draftCounts"]=chipValuesJson(bj?aggregate:draftCounts);}
    if(choosing)state["betPrompt"]=wagerLabel->text();
    if(phase=="draw"&&allowed.contains("draw"))state["betPrompt"]="Click cards to select replacements, then click the table.\nSelect none to keep all five.";
    if(bj&&phase=="blackjack"&&turn==me)state["betPrompt"]="Left-click the table to Hit · right-click to Stand";
    table->setState(state);
    turnLabel->setText(ended?"Cashed out · session ended":paused?"Table paused":!network.ready()?"Waiting for network players…":turn>=0&&turn<people.size()?people[turn].toObject()["name"].toString()+"’s turn":bj?state["street"].toString():phase=="finished"?"Hand complete":"Ready to deal");
    if(bj&&phase=="blackjack"&&turn>=0&&turn<people.size()&&enabled){auto hands=people[turn].toObject()["hands"].toArray();int h=state["handIndex"].toInt();if(h>=0&&h<hands.size())turnLabel->setText(QString("%1 · Hand %2/%3 · Total %4").arg(turn==me?"Your turn":people[turn].toObject()["name"].toString()).arg(h+1).arg(hands.size()).arg(hands[h].toObject()["totalText"].toString()));}
    balanceLabel->setText(!seat.isEmpty()?formatUnits(seat["chips"].toDouble())+" units":"Watching the table");
    statusLabel->setText(state["message"].toString()+(state["cashoutTarget"].toInt()?QString(" · Cash-out balance: %1").arg(formatUnits(state["cashoutTarget"].toInt())):""));
    if(phase=="finished"&&!seat.isEmpty()){
        QString result=seat["outcome"].toString();auto hands=seat["hands"].toArray();int chosen=table->displayedHand();
        if(bj&&chosen>=0&&chosen<hands.size())result=QString("Hand %1/%2: %3").arg(chosen+1).arg(hands.size()).arg(hands[chosen].toObject()["result"].toString());
        double net=seat["roundNet"].toDouble();statusLabel->setText((ended?"Cashed out · ":"")+result+QString(" · Round %1%2 · Balance %3").arg(net>0?"+":"").arg(formatUnits(net)).arg(formatUnits(seat["chips"].toDouble()+seat["committed"].toInt())));
    }
    for(auto it=actions.begin();it!=actions.end();++it){bool bjControl=bj&&(it.key()=="hit"||it.key()=="stand"||it.key()=="double"||it.key()=="split");bool pokerControl=!bj&&(waiting||phase=="betting")&&(it.key()=="call"||it.key()=="fold"||it.key()=="raise"||it.key()=="allin");it.value()->setVisible(allowed.contains(it.key())||bjControl||pokerControl);it.value()->setEnabled(enabled&&allowed.contains(it.key()));}
    if(bj){
        actions["hit"]->setToolTip("Take another card on the active hand.");actions["stand"]->setToolTip("Finish the active hand and play the next one.");
        QString doubleTip="Double the active hand’s bet, take exactly one card, then stand.";
        QString splitTip="Split a two-card pair; add a matching bet for the new hand. Play each hand normally, including split aces. Split blackjack pays 3:2. Up to four hands per original hand.";
        auto hands=seat["hands"].toArray();int h=state["handIndex"].toInt();
        if(turn!=me||phase!="blackjack"){doubleTip="Available on your turn, with two cards and enough chips for a matching bet.";splitTip="Available on your turn with a two-card pair and enough chips for another matching bet.";}
        else if(h>=0&&h<hands.size()){
            auto hand=hands[h].toObject();auto cards=hand["cards"].toArray();bool enough=int(seat["chips"].toDouble())>=hand["bet"].toInt();
            if(cards.size()!=2)doubleTip="Double down requires exactly two cards.";else if(!enough)doubleTip="Not enough chips to match this hand’s bet.";
            if(cards.size()!=2||cards[0].toObject()["rank"]!=cards[1].toObject()["rank"])splitTip="Split requires two cards of the same rank.";else if(!enough)splitTip="Not enough chips for the extra split bet.";else if(!allowed.contains("split"))splitTip="Split limit reached for this original hand or the available shoe.";
        }
        actions["double"]->setToolTip(doubleTip);actions["split"]->setToolTip(splitTip);
    }
    if(bj&&turn==me&&phase=="blackjack"&&table->displayedHand()!=state["handIndex"].toInt())for(auto key:{"hit","stand","double","split"}){actions[key]->setEnabled(false);actions[key]->setToolTip(QString("Click the hand or use the arrows on your seat to select Hand %1 and play it.").arg(state["handIndex"].toInt()+1));}
    if(allowed.contains("call")){int due=std::min(int(seat["chips"].toDouble()),state["currentBet"].toInt()-seat["bet"].toInt());actions["call"]->setText(QString("Call %1  [C]").arg(due));}
    if(allowed.contains("raise")){int target=amount?seat["bet"].toInt()+amount:state["raiseTo"].toInt();actions["raise"]->setText(QString("%1 %2  [R]").arg(state["currentBet"].toInt()?"Raise to":"Bet").arg(target));actions["raise"]->setEnabled(enabled&&target>=state["raiseTo"].toInt()&&target<=int(seat["chips"].toDouble())+seat["bet"].toInt());}
    if(allowed.contains("allin"))actions["allin"]->setText(QString("All in %1").arg(int(seat["chips"].toDouble())));
    dealButton->setVisible(!bj&&!state["usesAnte"].toBool()&&!network.isClient()&&waiting&&!ended);dealButton->setEnabled(enabled&&!bj&&!state["usesAnte"].toBool());
    cashoutButton->setEnabled(enabled&&!ended&&!state["cashoutRequested"].toBool());cashoutButton->setText(state["cashoutRequested"].toBool()&&!ended?"Cash out after hand":"Cash out");
    bool canExchange=enabled&&me>=0&&!ended&&int(seat["chips"].toDouble())>0;exchangeButton->setEnabled(canExchange);findChild<QAction *>("exchangeChipsAction")->setEnabled(canExchange);
    findChild<QAction *>("blackjackDecksAction")->setEnabled(bj&&!network.isClient()&&waiting&&!sounds.isPresenting());
    findChild<QAction *>("pokerAnteAction")->setEnabled(state["usesAnte"].toBool()&&!network.isClient()&&waiting&&!sounds.isPresenting()&&!ended);
    pauseButton->setEnabled(!network.isClient());autoDeal->setVisible(!bj&&!state["usesAnte"].toBool());autoDeal->setEnabled(!network.isClient()&&!ended);
    auto log=state["history"].toArray();QStringList history;for(auto line:log)history<<line.toString();QStringList old;for(int i=0;i<activity->count();++i)old<<activity->item(i)->text();if(old!=history){activity->clear();activity->addItems(history);activity->scrollToBottom();}
}
void Window::applyPlaybackEvent(const QJsonObject &event) {
    const QString cue=event["cue"].toString();
    if(cue=="round"){
        displayState=latestState;displayState["dealer"]=QJsonArray{};displayState["board"]=QJsonArray{};
        auto people=displayState["players"].toArray();
        for(int i=0;i<people.size();++i){
            auto person=people[i].toObject();person["cards"]=QJsonArray{};person["status"]="";person["outcome"]="";person["roundNet"]=0;
            auto hands=person["hands"].toArray();for(int h=0;h<hands.size();++h){auto hand=hands[h].toObject();hand["cards"]=QJsonArray{};hand["result"]="";hand["totalText"]="";hand["value"]=0;hands[h]=hand;}
            person["hands"]=hands;people[i]=person;
        }
        displayState["players"]=people;playbackMessage="Preparing the deal…";
    }else if(cue=="split"){
        int seat=event["seat"].toInt(),h=event["handIndex"].toInt();auto people=displayState["players"].toArray();
        if(seat>=0&&seat<people.size()){
            auto person=people[seat].toObject();auto hands=person["hands"].toArray();
            if(h>=0&&h<hands.size()){
                auto first=hands[h].toObject();auto cards=first["cards"].toArray();
                if(cards.size()==2){
                    auto other=cards.takeAt(1);
                    auto singleCardHand=[](QJsonObject hand,const QJsonArray &cards){
                        auto card=cards[0].toObject();Cards known{{card["rank"].toInt(),card["suit"].toInt()}};
                        hand["cards"]=cards;hand["result"]="";hand["done"]=false;hand["value"]=blackjackValue(known);hand["totalText"]=blackjackTotalText(known);return hand;
                    };
                    hands[h]=singleCardHand(first,cards);hands.insert(h+1,singleCardHand(first,QJsonArray{other}));
                    person["hands"]=hands;person["status"]="Split";people[seat]=person;displayState["players"]=people;
                    playbackMessage="Splitting into two hands…";
                }
            }
        }
    }else if(cue=="deal"){
        QString target=event["target"].toString();int index=event["index"].toInt();
        auto put=[&](QJsonArray &cards){if(index==cards.size())cards.append(event["card"]);else if(index>=0&&index<cards.size())cards[index]=event["card"];};
        if(target=="dealer"||target=="board"){auto cards=displayState[target].toArray();put(cards);displayState[target]=cards;}
        else if(target=="player"){
            int seat=event["seat"].toInt(),h=event["handIndex"].toInt(-1);auto people=displayState["players"].toArray();
            if(seat>=0&&seat<people.size()){
                auto person=people[seat].toObject();
                if(h<0){auto cards=person["cards"].toArray();put(cards);person["cards"]=cards;}
                else{
                    auto hands=person["hands"].toArray();if(h<hands.size()){
                        auto hand=hands[h].toObject();auto cards=hand["cards"].toArray();put(cards);hand["cards"]=cards;
                        Cards known;for(const auto &value:cards){auto c=value.toObject();known.push_back({c["rank"].toInt(),c["suit"].toInt()});}
                        hand["value"]=blackjackValue(known);hand["totalText"]=blackjackTotalText(known);hand["result"]=blackjackValue(known)>21?"Bust":"";hands[h]=hand;person["hands"]=hands;
                        displayState["turn"]=seat;displayState["handIndex"]=h;
                    }
                }
                people[seat]=person;displayState["players"]=people;
            }
        }
        playbackMessage="Dealing cards…";
    }else if(cue=="reveal")displayState["dealer"]=event["dealer"];
    else if(cue=="shuffle")playbackMessage="Shuffling the deck…";
    else if(cue=="deck-tap")playbackMessage="Preparing the deck…";
    else if(cue=="payout"){displayState=latestState;playbackMessage="Paying out…";}
    if(event.contains("cardsRemaining"))displayState["cardsRemaining"]=event["cardsRemaining"];
    refresh();
}
void Window::deckSettings() {
    if(network.isClient()||game.config().kind!=Kind::Blackjack||game.active()||sounds.isPresenting())return;
    dialogOpen=true;QDialog dialog(this);dialog.setObjectName("deckSettingsDialog");dialog.setWindowTitle("Blackjack decks");auto *layout=new QVBoxLayout(&dialog);
    auto *note=textLabel("Choose 1–8 decks. The shoe stays in play between rounds. Changing this setting returns pending bets and starts a fresh shoe on the next deal.");note->setWordWrap(true);layout->addWidget(note);
    auto *count=new QSpinBox;count->setObjectName("blackjackDeckCount");count->setRange(1,8);count->setValue(game.config().blackjackDecks);count->setSuffix(" decks");layout->addWidget(count);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted&&game.setBlackjackDecks(count->value())){QSettings().setValue("blackjack/decks",count->value());network.publish();}
    dialogOpen=false;refresh();
}
void Window::tick() {
    if(network.isClient()||paused||dialogOpen||!network.ready()||game.sessionEnded()||sounds.isBusy())return;
    if(game.active()){
        nextDealTicks=0;
        if(game.current()>=0&&game.players()[game.current()].config.control==Control::Computer){static int pace=0;if(++pace>=(fast->isChecked()?1:4)){pace=0;game.computerStep();network.publish();refresh();}}
    }else {
        if(game.fundBustedComputers()){network.publish();refresh();return;}
        if(game.config().kind==Kind::Blackjack||game.usesAnte()){
            bool needsBet=std::any_of(game.players().begin(),game.players().end(),[this](const Seat &s){return s.config.control==Control::Computer&&!s.wagerReady&&s.chips>=(game.usesAnte()?1:*std::min_element(game.config().chipValues.begin(),game.config().chipValues.end()));});
            if(needsBet){game.computerStep();network.publish();refresh();return;}
            nextDealTicks=0;if(game.betsReady()){game.deal();network.publish();refresh();}return;
        }
        if(autoDeal->isChecked()&&++nextDealTicks>=(fast->isChecked()?6:14)){nextDealTicks=0;if(!game.deal())autoDeal->setChecked(false);network.publish();refresh();}
    }
}
void Window::anteSettings() {
    if(network.isClient()||!game.usesAnte()||game.active()||game.sessionEnded())return;
    dialogOpen=true;QDialog dialog(this);dialog.setObjectName("anteSettingsDialog");dialog.setWindowTitle("Settings — Poker ante");auto *layout=new QVBoxLayout(&dialog);
    auto *note=textLabel("Click the empty table felt to accept this ante before each draw or stud deal; chip selection is optional. Everyone still playing antes. If nobody opens with jacks or better, the pot carries over and everyone antes again. Changing the ante returns pending bets; the carried pot stays.");note->setWordWrap(true);layout->addWidget(note);
    auto *amount=new QSpinBox;amount->setObjectName("pokerAnteAmount");amount->setRange(1,1000000);amount->setValue(game.anteValue());amount->setSuffix(" units");layout->addWidget(amount);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted&&game.setAnte(amount->value())){QSettings().setValue("poker/ante",amount->value());network.publish();}
    dialogOpen=false;refresh();
}
void Window::profileSettings() {
    dialogOpen=true;QSettings settings;QDialog dialog(this);dialog.setObjectName("profileDialog");dialog.setWindowTitle("Settings — Player profile");auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,24,24,24);
    int me=-1;for(int i=0;i<int(game.players().size());++i)if(game.players()[i].config.control==Control::Human)me=i;
    auto state=network.isClient()?network.remoteState():game.snapshot(me);
    if(network.isClient())me=state["viewer"].toInt(-1);
    auto people=state["players"].toArray();bool seated=me>=0&&me<people.size();auto seat=seated?people[me].toObject():QJsonObject{};
    bool connected=!network.isClient()||(state["connected"].toBool()&&seated);
    bool betweenHands=state["phase"]=="ready"||state["phase"]=="finished";
    double openingBalance=seated?seat["chips"].toDouble()+seat["committed"].toInt():profileBalance();
    layout->addWidget(textLabel("Your local player profile","font-size: 23px; color: #dfc58c; font-weight: bold;"));
    auto *note=textLabel("Save updates your current seat immediately. You can change your name during play and your bankroll between hands. Changing the bankroll returns any pending bet so you can choose again.");note->setWordWrap(true);layout->addWidget(note);
    auto *form=new QFormLayout;auto *name=new QLineEdit(seated?seat["name"].toString():settings.value("profile/name","You").toString());name->setObjectName("profileName");name->setMaxLength(24);name->setEnabled(connected);form->addRow("Name",name);
    auto *balance=new QDoubleSpinBox;balance->setObjectName("profileBalance");balance->setDecimals(1);balance->setSingleStep(0.5);balance->setRange(0,std::max(10000000.0,openingBalance));balance->setValue(openingBalance);balance->setSuffix(" units");form->addRow("Current bankroll",balance);layout->addLayout(form);
    balance->setEnabled(connected&&betweenHands&&!sounds.isPresenting()&&!state["inputBlocked"].toBool());
    if(!balance->isEnabled())layout->addWidget(textLabel(connected?"Finish the hand and payout before changing the bankroll.":"Reconnect to the host before saving profile changes."));
    layout->addWidget(textLabel(QString("Rounds completed: %1 · Cash-outs: %2\nLast cash-out: %3 units").arg(settings.value("profile/hands",0).toInt()).arg(settings.value("profile/cashouts",0).toInt()).arg(formatUnits(settings.value("profile/lastCashout",0).toDouble()))));
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setEnabled(connected);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted&&connected){
        QString newName=name->text().simplified().left(24);if(newName.isEmpty())newName="You";
        std::optional<double> amount;
        double chosen=std::floor(balance->value()*2)/2;
        if(balance->isEnabled()&&chosen!=openingBalance)amount=chosen;
        if(network.isClient()){
            if(!network.sendProfile(newName,amount))QMessageBox::information(this,"Profile not saved","Reconnect to the host, then save your profile again.");
        }else if(!seated||game.updateProfile(me,newName,amount)){
            settings.setValue("profile/name",newName);settings.setValue("profile/balance",amount.value_or(openingBalance));settings.sync();draftCounts={};draftKey.clear();network.publish();
        }else QMessageBox::information(this,"Profile not saved","Finish the hand before changing your bankroll. Use whole or half units, up to 10,000,000.");
    }
    dialogOpen=false;refresh();
}
void Window::saveAppearance(){QSettings s;s.setValue("tableTheme",tableTheme);s.setValue("faceTheme",faceTheme);s.setValue("backTheme",backTheme);s.setValue("fast",fast->isChecked());s.setValue("autoDeal",autoDeal->isChecked());}
void Window::exchangeChips() {
    if(dialogOpen||paused||!network.ready()||sounds.isPresenting()||network.remoteState()["inputBlocked"].toBool())return;
    const int me=table->state()["viewer"].toInt(-1);
    const auto basis=network.isClient()?network.remoteState():game.snapshot(me);auto people=basis["players"].toArray();
    if(me<0||me>=people.size()||basis["cashedOut"].toBool())return;
    const auto seat=people[me].toObject();const int available=int(seat["chips"].toDouble());if(available<=0)return;
    const auto values=readChipValues(basis["chipValues"].toArray());const auto original=seat["bankChips"].toArray();
    dialogOpen=true;QDialog dialog(this);dialog.setObjectName("exchangeChipsDialog");dialog.setWindowTitle("Exchange chips");dialog.resize(580,520);
    auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,24,24,24);layout->setSpacing(12);
    layout->addWidget(textLabel("Choose your chip stacks","font-size: 23px; font-weight: bold; color: #dfc58c;"));
    auto *note=textLabel(QString("Available balance: %1 units. Choose how many chips of each color you want. Any remainder stays as change. Your balance stays the same.").arg(formatUnits(seat["chips"].toDouble())));note->setWordWrap(true);layout->addWidget(note);
    if(seat["committed"].toInt())layout->addWidget(textLabel(QString("%1 already bet · those chips stay on the table.").arg(formatUnits(seat["committed"].toInt()))));
    auto *grid=new QGridLayout;grid->setVerticalSpacing(10);grid->addWidget(textLabel("Chip"),0,1);grid->addWidget(textLabel("Value"),0,2);grid->addWidget(textLabel("You have"),0,3);grid->addWidget(textLabel("You want"),0,4);
    std::array<QSpinBox *,5> inputs;
    for(int i=0;i<5;++i){
        auto *icon=new QLabel;QPixmap image(42,44);image.fill(Qt::transparent);QPainter painter(&image);paintChip(painter,QRectF(1,1,40,40),i,values[i]);painter.end();icon->setPixmap(image);grid->addWidget(icon,i+1,0);
        grid->addWidget(textLabel(ChipNames[i]),i+1,1);grid->addWidget(textLabel(formatUnits(values[i])),i+1,2);grid->addWidget(textLabel(formatUnits(original[i].toInt())),i+1,3);
        auto *count=new QSpinBox;inputs[i]=count;count->setObjectName(QString("exchangeCount%1").arg(ChipNames[i]));count->setRange(0,available/values[i]);count->setValue(original[i].toInt());count->setMinimumWidth(125);grid->addWidget(count,i+1,4);
    }
    layout->addLayout(grid);auto *totalLabel=textLabel("");totalLabel->setObjectName("exchangeTotal");totalLabel->setWordWrap(true);layout->addWidget(totalLabel);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Cancel);auto *clear=buttons->addButton("Clear counts",QDialogButtonBox::ResetRole);auto *fill=buttons->addButton("Fill remaining",QDialogButtonBox::ActionRole);auto *apply=buttons->addButton("Exchange",QDialogButtonBox::AcceptRole);apply->setObjectName("confirmExchangeButton");layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    bool waiting=false,stale=false;
    auto counts=[&]{std::array<int,5> out{};for(int i=0;i<5;++i)out[i]=inputs[i]->value();return out;};
    auto update=[&]{
        ChipBreakdown wanted;wanted.counts=counts();qint64 total=chipInventoryValue(wanted,values),remaining=qint64(available)-total;bool changed=chipValuesJson(wanted.counts)!=original;
        totalLabel->setStyleSheet(remaining<0?"color: #ff9b99;":"color: #dfc58c;");
        totalLabel->setText(remaining<0?QString("Chips total: %1 · %2 over your available balance").arg(formatUnits(total)).arg(formatUnits(-remaining)):QString("Chips total: %1 · Change: %2\nBalance after exchange: %3").arg(formatUnits(total)).arg(formatUnits(remaining+seat["chips"].toDouble()-available)).arg(formatUnits(seat["chips"].toDouble())));
        apply->setEnabled(changed&&remaining>=0&&!waiting&&!stale);fill->setEnabled(remaining>0&&!waiting&&!stale);
    };
    for(auto *input:inputs)connect(input,&QSpinBox::valueChanged,&dialog,[&]{update();});
    connect(clear,&QPushButton::clicked,&dialog,[&]{for(auto *input:inputs)input->setValue(0);});
    connect(fill,&QPushButton::clicked,&dialog,[&]{ChipBreakdown wanted;wanted.counts=counts();int remaining=available-int(chipInventoryValue(wanted,values));if(remaining<=0)return;auto added=breakDownChips(remaining,values);for(int i=0;i<5;++i)inputs[i]->setValue(inputs[i]->value()+added.counts[i]);});
    auto rejectExchange=[&](const QString &message){waiting=false;stale=true;apply->setEnabled(false);fill->setEnabled(false);clear->setEnabled(false);for(auto *input:inputs)input->setEnabled(false);totalLabel->setStyleSheet("color: #ff9b99;");totalLabel->setText(message);};
    connect(&network,&Network::chipsExchanged,&dialog,[&](bool accepted,const QString &message){if(!waiting)return;if(accepted)dialog.accept();else rejectExchange(message);});
    connect(&network,&Network::changed,&dialog,[&]{if(network.isClient()&&!network.ready())rejectExchange("The connection is unavailable. Close and reopen Exchange chips after reconnecting.");});
    connect(apply,&QPushButton::clicked,&dialog,[&]{
        if(network.isClient()){
            waiting=true;apply->setEnabled(false);fill->setEnabled(false);clear->setEnabled(false);for(auto *input:inputs)input->setEnabled(false);totalLabel->setText("Waiting for the host to exchange your chips…");
            if(!network.sendExchange(counts(),basis))rejectExchange("The table is busy or disconnected. Close and reopen Exchange chips to try again.");
        }else{
            auto now=game.snapshot(me);auto current=now["players"].toArray()[me].toObject();bool same=now["hand"]==basis["hand"]&&now["soundSession"]==basis["soundSession"]&&now["chipValues"]==basis["chipValues"]&&current["bankChips"]==seat["bankChips"]&&current["bankChange"]==seat["bankChange"]&&current["chips"]==seat["chips"];
            if(same&&game.exchangeChips(me,counts())){network.publish();dialog.accept();}else rejectExchange("Your table or chips changed. Close and reopen Exchange chips to use the current balance.");
        }
    });
    update();dialog.exec();dialogOpen=false;refresh();
}
void Window::chipSettings() {
    dialogOpen=true;
    QDialog dialog(this);dialog.setObjectName("chipSettingsDialog");dialog.setWindowTitle("Settings — Chip values");dialog.resize(480,470);
    auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,24,24,24);layout->setSpacing(16);
    layout->addWidget(textLabel("Chip values","font-size: 25px; color: #dfc58c; font-weight: bold;"));
    auto *note=textLabel(network.isClient()?"The host sets these chip values for everyone at this table.":"Choose how many play-chip units each color represents. Values are saved and shared with network players.","color: #9db4ae;");note->setWordWrap(true);layout->addWidget(note);
    auto values=network.isClient()?readChipValues(network.remoteState()["chipValues"].toArray()):game.config().chipValues;
    auto *grid=new QGridLayout;grid->setVerticalSpacing(12);std::array<QSpinBox *,5> inputs;
    for(int i=0;i<5;++i) {
        auto *icon=new QLabel;icon->setFixedSize(42,44);QPixmap pixmap(42,44);pixmap.fill(Qt::transparent);QPainter p(&pixmap);paintChip(p,QRectF(1,1,40,40),i,values[i]);p.end();icon->setPixmap(pixmap);grid->addWidget(icon,i,0);
        grid->addWidget(textLabel(ChipNames[i]),i,1);inputs[i]=new QSpinBox;inputs[i]->setObjectName(QString("chipValue%1").arg(ChipNames[i]));inputs[i]->setRange(1,1000000);inputs[i]->setValue(values[i]);inputs[i]->setSuffix(" units");inputs[i]->setEnabled(!network.isClient());grid->addWidget(inputs[i],i,2);
        connect(inputs[i],&QSpinBox::valueChanged,&dialog,[icon,i](int value){QPixmap image(42,44);image.fill(Qt::transparent);QPainter painter(&image);paintChip(painter,QRectF(1,1,40,40),i,value);painter.end();icon->setPixmap(image);});
    }
    layout->addLayout(grid);
    auto *detail=textLabel("Chips represent the displayed amounts. Any leftover amount is shown as change.","font-size: 11px; color: #9db4ae;");detail->setWordWrap(true);layout->addWidget(detail);
    auto *buttons=new QDialogButtonBox(network.isClient()?QDialogButtonBox::Close:QDialogButtonBox::Save|QDialogButtonBox::Cancel|QDialogButtonBox::RestoreDefaults);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(auto *defaults=buttons->button(QDialogButtonBox::RestoreDefaults))connect(defaults,&QPushButton::clicked,&dialog,[&]{for(int i=0;i<5;++i)inputs[i]->setValue(DefaultChipValues[i]);});
    if(dialog.exec()==QDialog::Accepted && !network.isClient()) {
        for(int i=0;i<5;++i)values[i]=inputs[i]->value();
        if(game.setChipValues(values)){QSettings settings;for(int i=0;i<5;++i)settings.setValue(QString("chipValues/%1").arg(ChipNames[i]),values[i]);network.publish();}
    }
    dialogOpen=false;refresh();
}
void Window::appearance() {
    dialogOpen=true;int oldTable=tableTheme,oldFace=faceTheme,oldBack=backTheme;
    QDialog dialog(this);dialog.setWindowTitle("Make it your table");dialog.resize(720,810);auto *layout=new QVBoxLayout(&dialog);layout->setContentsMargins(24,24,24,24);layout->setSpacing(14);
    layout->addWidget(textLabel("Make it your table","font-size: 25px; color: #dfc58c; font-weight: bold;"));
    auto *note=textLabel("Mix 20 table finishes, 20 card faces, and 20 card backs. Your choices are saved for the next visit.","color: #9db4ae;");note->setWordWrap(true);layout->addWidget(note);
    auto *form=new QFormLayout;std::array<QComboBox *,3> combos;QStringList labels{"Table finish","Card face","Card back"};
    for(int i=0;i<3;++i){combos[i]=new QComboBox;combos[i]->setObjectName(QString("theme%1").arg(i));int index=0;for(const auto &t:themes()){QPixmap swatch(i==0?QSize(20,20):QSize(48,70));if(i==0)swatch.fill(QColor(t.felt));else{swatch.fill(Qt::transparent);QPainter p(&swatch);paintCard(p,QRectF(2,2,44,64),i==1?QJsonObject{{"rank",14},{"suit",3}}:QJsonObject{{"hidden",true}},index,index);}combos[i]->addItem(QIcon(swatch),t.name);++index;}if(i>0){combos[i]->setIconSize(QSize(44,64));combos[i]->setMaxVisibleItems(6);}combos[i]->setCurrentIndex(i==0?tableTheme:i==1?faceTheme:backTheme);form->addRow(labels[i],combos[i]);}layout->addLayout(form);
    auto *cardPreview=new QLabel;cardPreview->setObjectName("cardThemePreview");cardPreview->setAlignment(Qt::AlignCenter);layout->addWidget(cardPreview);
    auto showCards=[&]{QPixmap picture(380,160);picture.fill(Qt::transparent);QPainter p(&picture);paintCard(p,QRectF(60,6,92,130),{{"rank",13},{"suit",2}},faceTheme,backTheme);paintCard(p,QRectF(222,6,92,130),{{"hidden",true}},faceTheme,backTheme);p.setPen(QColor("#dfc58c"));p.drawText(QRectF(60,138,92,22),Qt::AlignCenter,"Front");p.drawText(QRectF(222,138,92,22),Qt::AlignCenter,"Back");p.end();cardPreview->setPixmap(picture);};showCards();
    auto *preview=new Table;preview->setMinimumSize(580,230);auto sample=table->state();preview->setState(sample);preview->setThemes(tableTheme,faceTheme,backTheme);layout->addWidget(preview,1);
    auto change=[&]{tableTheme=combos[0]->currentIndex();faceTheme=combos[1]->currentIndex();backTheme=combos[2]->currentIndex();showCards();table->setThemes(tableTheme,faceTheme,backTheme);preview->setThemes(tableTheme,faceTheme,backTheme);};for(auto *combo:combos)connect(combo,&QComboBox::currentIndexChanged,&dialog,change);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted)saveAppearance();else{tableTheme=oldTable;faceTheme=oldFace;backTheme=oldBack;table->setThemes(tableTheme,faceTheme,backTheme);}dialogOpen=false;refresh();
}
void Window::rules() {
    dialogOpen=true;QDialog dialog(this);dialog.setWindowTitle("How to play — Velvet Casino");dialog.resize(760,690);auto *layout=new QVBoxLayout(&dialog);auto *body=new QTextBrowser;body->setOpenExternalLinks(true);
    body->setHtml(R"(<h1 style='color:#dfc58c'>A seat at Velvet</h1>
<p>All balances are play chips. Choose 1–5 blackjack players plus the dealer, or 2–5 poker players. Each seat can be a computer or a person. The first seat can be you on this computer. Other people join over the network. All-computer tables are supported.</p>
<h2>Blackjack</h2><p>Aces count as 1 or 11, whichever gives the highest total without busting; face cards count as 10. A + 4 displays 5 / 15. Add an 8 and it becomes 13; add a 6 instead and it is 11 / 21. A soft 21 with three or more cards keeps Hit and Stand available. If you stand, it scores 21 and beats a dealer 20. Two-card blackjack, hard 21, a bust, or doubling ends that hand automatically. Get closer to 21 than the house without going over. Choose 1–4 starting hands before placing your bet. Use the Hand selector to choose chips separately for each hand from your shared bankroll, then click the table once to confirm all bets. Each hand needs a positive bet. Clear hand and right-click removal affect only the selected hand. Play each hand in order. During your turn, left-click the empty table felt to Hit or right-click it to Stand. The buttons and keyboard shortcuts also work. Hit, stand, double on any first two cards (one extra matching bet and exactly one more card), or split an equal-rank pair. Each original hand can split into up to four hands, including doubling after a split when eligible. Split and Double down remain visible; hover over an unavailable button to see why. House rules: split aces stay playable like other hands; you may hit, stand, double, or split another eligible pair. A two-card 21 after a split is blackjack and pays 3:2. The dealer checks for blackjack and stands on all 17s. Two-card blackjack pays 3:2; regular wins pay 1:1; ties return the wager. Choose 1–8 decks in Settings → Blackjack decks or during setup. The shoe stays in use between rounds and reshuffles when low or when more reserve is needed for the next round and possible splits. Small shoes limit simultaneous hands and splits so the deck cannot run out. Poker always uses one deck, reshuffled each hand. No insurance or surrender. Before every deal, choose chips and click the empty table felt to confirm; Place bet also works. Left-click a chip to add it; right-click to remove it. Each human and network player confirms a wager; computers choose their own bets. Once all bets are ready, cards deal automatically after the chip sound. No Deal click is needed. Odd wagers keep the exact half-unit blackjack payout.</p>
<h2>Texas hold’em</h2><p>Two private cards each, followed by the three-card flop, one-card turn, and one-card river. Make your best five-card hand using any combination of private and community cards. Small/big blinds are half/full minimum bet. Heads-up: the button posts the small blind and acts first before the flop, last afterward.</p>
<h2>5-card draw</h2><p>The host chooses a fixed ante in setup or Settings → Poker ante. Click the empty felt to accept and post it, without selecting chips, or press Ante. Selecting chips is optional and does not change the fixed ante. All funded players can ante immediately; bankrupt human seats are skipped. Everyone still playing must ante before the five private cards deal automatically. Betting raises happen after the cards are dealt. The host receives the first card of the opening hand; dealing and betting start left of the dealer button and proceed clockwise, rotating the button after each hand. After the opening betting round, click any of your cards to replace them, then click the empty table felt to confirm. With none selected, clicking the felt keeps all five. The Draw / Keep all 5 button also works. You may replace zero through five cards. A second betting round leads to showdown. Discarded cards do not return to the deck.</p>
<h2>7-card stud</h2><p>Start with two down and one up. Deal a fourth, fifth and sixth card face up, then the seventh face down, with betting after every deal. Final layout: two down, four up, one down. The best five of seven wins.</p>
<h2>5-card stud</h2><p>Start with one down and one up. Three more cards are dealt face up, one at a time with betting between them. Final layout: one down, four up.</p>
<p>Stud uses the host’s fixed ante. Everyone posts it before each deal, just like draw. Without the jacks option, the lowest upcard posts a half-small-bet bring-in (suit order: clubs, diamonds, hearts, spades). After that, the strongest exposed hand acts first. Ties in exposed hands act clockwise from the button. Betting is no-limit on every street.</p>
<h2>Jacks or better to open</h2><p>Available in draw and both stud games. You need a pair of jacks or a better hand to make the first voluntary bet. Once someone opens, all remaining players can call or raise. Computers open whenever their hand qualifies and they can legally bet, using a short all-in if necessary. In stud there is no forced bring-in when this option is on; checked rounds advance the deal until someone can open. If nobody opens before the draw or by the end of stud, the pot carries over and everyone posts another ante before the next deal. This repeats until someone opens and the pot is won. The host’s ante remains fixed. On cash-out, an unopened pot is returned to its contributors. No minimum hand is needed to win after the betting opens.</p>
<h2>Betting and showdown</h2><p>In every poker variant, clicking the empty table felt with no chips selected calls the amount owed, or checks when nothing is owed. No-limit betting: choose the actual chips to put in, including the call amount, then click the empty table felt. Matching the call amount calls, a legal higher amount raises, and your whole stack goes all in. The label previews your action. Incomplete calls and undersized raises stay selected for correction. Call, Raise, All in, Check, and Fold buttons act immediately. Betting rounds, later cards, showdown, and payouts advance automatically; folding forfeits the chips already bet, removes that player from all further action and card replacement in this hand, and leaves them at the table to watch. They can ante into the next hand. Each raise returns action to other players, who must call, raise again, fold, or go all in. A full raise must be at least the previous full raise size, initially the table minimum. There is no raise-count cap. Short all-in increases only reopen raising once their cumulative increase reaches a full raise. All in commits every chip left in your stack and ends your betting. You still receive the normal remaining cards, including all seven stud cards, and may replace up to five cards in draw. If all remaining players are all in, cards continue without more betting. Side pots ensure you can win only the current-hand chips you matched; the unopened carried pot joins the main pot. Unmatched chips are returned. Equal hands split the pot; odd chips go clockwise left of the button. Suits never break showdown ties.</p>
<p>Hand strength, high to low: royal flush, straight flush, four of a kind, full house, flush, straight, three of a kind, two pair, one pair, high card. A royal flush is A-K-Q-J-10 of one suit. Three of a kind beats two pair and one pair. Equal ranks use kickers; exactly tied best five-card hands split the pot. A–2–3–4–5 is a five-high straight. Computer players use their own cards and the community cards; they never inspect opponents’ downcards or the next card in the deck.</p>
<h2>Network tables</h2><p>Create a table with Network player seats, then share the address, port, and room code shown at the top. Each person opens this app, chooses Join a table, and enters those details. Your seat stays at the bottom and the house dealer stays at the top, including with four or five players. The host receives the first card of the opening poker deal. The marker labeled BUTTON rotates clockwise between player seats without moving them; the house dealer is not a poker opponent. Blackjack starts when all wagers are ready; draw and stud start when all antes are posted. The host starts hold’em hands. Use a trusted local network or private VPN; this app does not provide an internet lobby or encrypted transport. Only your private cards are sent to your client. A disconnected client automatically reconnects and reclaims its seat while its app stays open. The host pauses play while waiting for required clients. Restarting a client loses its reconnect token; start a new table if necessary.</p>
<h2>Chip exchange</h2><p>Use Exchange in the bottom chip row or Settings → Exchange chips to choose the number of each colored chip for your available balance. The total updates as you type. Clear counts starts from zero; Fill remaining fills the rest using the configured values. Any amount left over remains as change, including half units. Exchange updates the stacks immediately while preserving your balance and existing bets. Cancel keeps your current chips. Network exchanges are confirmed by the host.</p><h2>Chip values</h2><p>Settings → Chip values lets the host assign a value from 1 to 1,000,000 units to red, blue, white, green, and yellow chips. Defaults: red 5, blue 10, white 1, green 25, yellow 100. Values are saved and shared with network players. The sidebar shows the color/value key; each seat shows its bet above its cards and its remaining stack below them. Any remainder is shown as change. Changing denominations preserves an active hand. Between blackjack, draw and stud hands, pending bets are refunded so players can choose the new chips.</p>
<h2>Computer house loans</h2><p>When a computer runs out of whole chips, the house lends it the configured Computer starting chips amount between hands. Owes house beside its name shows accumulated loans; each loan appears in hand history and is shared with network players. Debt is separate from its playable chips and is not automatically deducted from winnings. It lasts for this table session and appears in the cash-out summary. Your balance and other human balances are never refilled this way. All-in hands settle before loans are issued, and cash-out stops new loans. Computer playing strategy is unchanged.</p><h2>Profile and cash-out</h2><p>Your local profile saves your name, bankroll, completed hands, and cash-outs on this computer. Completed-hand balances are saved automatically, including half units. Settings → Player profile applies name edits to your seat immediately, even during play. Bankroll edits apply immediately between hands or after cashing out; they cancel your pending wager so you can choose again. The editor uses your current balance, and network edits are confirmed by the host. Network players bring their own local saved balance. An unfinished hand is not restored after restarting the app. Cash out ends the table after the current hand settles; pending bets are refunded if cards have not been dealt, along with unopened carried antes. A Cash-out balance of zero disables the automatic target; otherwise the session ends after a hand when any player reaches that balance. Final amounts are shown in hand history and saved to each connected player’s profile.</p><h2>Controls and appearance</h2><p>H: hit · S: stand · D: double · C: check/call · R: raise · F: fold. Cards stay on the table, with a large center rank and the correct suit-symbol count on numbered cards. Use the arrows beside your name to inspect multiple blackjack hands; play advances through them in order. Click table cards during the draw to select replacements, then click the empty felt to confirm; select none to keep all five. Results show Bust, Won, Lost, Tied, or Blackjack, in a colored banner directly above the hand, including that hand’s net winnings. Finished cards and results remain visible until you select a chip for the next bet or start the next hand. There is no clearing timer. In hold’em, clicking a chip clears the finished hand; the next deal posts the normal blinds. Your balance and history stay saved. All cards and stacks stay upright. Bankroll piles sit between the cards and the nearest table edge: left of left-side seats, right of right-side seats, and below your bottom seat. They show the actual colors and number of chips you own, in piles of up to twenty. Bets remove chips, winnings add chips, and existing colors stay in your stack; change is made when needed. Blackjack returns winning or tied stakes and adds winnings; poker pays from the pot. The separate chip panel remains the control for betting. A blackjack shoe beside the dealer shows how many cards remain and refills on a shuffle. The compact action row is centered above the bottom chip row; neither results nor hidden controls resize the table. Click the empty felt to confirm selected betting chips. Cards arrive one at a time, and actions unlock after dealing. Settings → Blackjack decks selects 1–8 decks between rounds; poker always uses one deck. Settings → Table sounds toggles the setup intro, chip bets, all-in pushes, collection at hand-end, payouts, card dealing, shuffling, and the deck tap after shuffling; the choice is saved locally. Computer turns wait for the sound sequence to finish. Use Settings → Card and table themes to change themes during a hand. Card choices show front and back pictures; table finishes retain their color swatches. Pause stops computer turns and network actions. Blackjack deals automatically after every funded seat confirms a wager. Before draw or stud, click the empty felt to accept the fixed ante without selecting chips. All funded players can ante immediately; bankrupt human seats are skipped. Draw and stud deal automatically after everyone antes. Poker betting actions always advance play immediately; hold’em offers Auto-deal for subsequent hands. Draw and stud pot chips sit dead center on the felt. Hold’em pot chips sit below the community cards. All hands play chip collection when settled; blackjack wins and ties and poker pot awards then play the payout sound. A new table uses your saved local bankroll and the configured computer stacks.</p>
<p>Rules references: <a href='https://bicyclecards.com/how-to-play/blackjack/'>Bicycle blackjack</a>, <a href='https://www.pokerstars.com/poker/games/stud/'>PokerStars stud</a>, <a href='https://www.pokerstars.com/poker/games/texas-holdem/'>hold’em</a>, <a href='https://www.pokerstars.com/poker/games/draw/'>draw</a>. The ante-draw, jacks-to-open stud, unopened-pot rollover, and table limits above are this app’s stated house rules.</p>)");
    layout->addWidget(body);auto *close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.exec();dialogOpen=false;refresh();
}
void Window::closeEvent(QCloseEvent *event) {saveAppearance();QMainWindow::closeEvent(event);}
}
