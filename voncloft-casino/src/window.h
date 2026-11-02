// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 voncloft-casino contributors
#pragma once
#include "game.h"
#include "table.h"
#include "network.h"
#include "sounds.h"
#include <QtWidgets>
namespace casino {
class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(bool startup=true,bool animateDeals=true);
    void setup();
    void appearance();
    void chipSettings();
    void exchangeChips();
    void deckSettings();
    void anteSettings();
    void profileSettings();
    void rules();
    void refresh();
    Game &engine() {return game;}
    Table *canvas() {return table;}
    void startConfig(const Config &config,quint16 port=45821);
private:
    Game game;
    Network network;
    ChipSounds sounds;
    Table *table;
    QSpinBox *handCount;
    QComboBox *bettingHand;
    QLabel *gameLabel,*statusLabel,*connectionLabel,*turnLabel,*balanceLabel;
    std::array<QLabel *,5> chipLabels,chipIcons;
    QListWidget *activity;
    QPushButton *dealButton,*pauseButton,*cashoutButton;
    QWidget *wagerBar;
    QLabel *wagerLabel;
    std::array<QToolButton *,5> wagerButtons;
    QPushButton *placeButton,*clearButton,*exchangeButton;
    std::array<int,5> draftCounts{};
    std::array<std::array<int,5>,4> draftHandCounts{};
    QString draftKey;
    int savedHand=0;
    bool savedCashout=false;
    QCheckBox *autoDeal,*fast;
    QMap<QString,QPushButton *> actions;
    QTimer timer;
    int tableTheme=0,faceTheme=0,backTheme=0;
    bool paused=false,dialogOpen=false;
    int nextDealTicks=0;
    bool refreshing=false;
    QJsonObject latestState,displayState;
    QString playbackMessage;
    QString clearedResultKey;
    void applyPlaybackEvent(const QJsonObject &event);
    void perform(const QString &action);
    void confirmPokerChips();
    void tick();
    void saveAppearance();
    void adjustChip(int index,int delta);
    void submitWager(bool clear=false);
    void saveProfile(const QJsonObject &state);
protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *object,QEvent *event) override;
};
}
