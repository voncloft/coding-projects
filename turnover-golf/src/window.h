#pragma once
#include "game.h"
#include "cards.h"
#include "network.h"
#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QTextEdit>
class Window : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(bool persist=true, QWidget *parent=nullptr);
    const Golf::Game &game() const { return g; }
    Golf::NetworkSession &session() { return net; }
    void capture(const QString &path);
private:
    Golf::Game g;
    Golf::NetworkSession net;
    bool persistence;
    QString savePath;
    QTimer aiTimer;
    int builtPlayers=0,builtSeat=-1,builtRounds=0;
    bool setupOpen=false;
    QLabel *connectionLabel;
    QPushButton *connectionButton;
    QAction *roundsAction;
    QWidget *opponentArea;
    QWidget *westArea,*eastArea;
    QVBoxLayout *westOpponents,*eastOpponents;
    QGridLayout *tableGrid;
    QHBoxLayout *opponents;
    std::vector<std::array<CardButton *,8>> computerCards;
    std::vector<QLabel *> computerLabels;
    std::array<CardButton *,8> hand;
    std::array<QLabel *,4> columnLabels;
    CardButton *stock, *discard, *held;
    QLabel *heading, *statusTitle, *instruction, *lastMove, *deckLabel, *yourScore, *totals, *result;
    QPushButton *discardButton, *finishButton, *nextButton;
    QTableWidget *scoreTable;
    void buildUi();
    void buildOpponents();
    void updateUi();
    void changed();
    void saveGame();
    void newGame();
    void showRoundSettings();
    void showRules();
    void showConnection();
    QString playerLabel(int player) const;
    QString winnerMessage(bool match) const;
    int seatOrientation(int player) const;
    void clickCard(int i);
    void keyAction(int key);
protected:
    void closeEvent(QCloseEvent *event) override;
};
