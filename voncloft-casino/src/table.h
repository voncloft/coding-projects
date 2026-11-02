// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 voncloft-casino contributors
#pragma once
#include <QtWidgets>
#include "game.h"
namespace casino {
struct Theme { QString name, felt, light, accent, paper, ink, red, back; };
const QVector<Theme> &themes();
QVector<QPointF> cardPips(int rank);
void paintCard(QPainter &p, const QRectF &rect, const QJsonObject &card, int face, int back, bool selected=false);
void paintChip(QPainter &p, const QRectF &rect, int colorIndex, int value);
class Table : public QWidget {
    Q_OBJECT
public:
    explicit Table(QWidget *parent=nullptr);
    void setState(const QJsonObject &state);
    void setThemes(int table, int face, int back);
    std::vector<int> discards() const;
    void clearSelection();
    void toggleDiscard(int index);
    int displayedHand() const { return inspectedHand; }
    void setDisplayedHand(int index);
    QJsonObject state() const { return data; }
signals:
    void selectionChanged(int count);
    void betConfirmed();
    void standRequested();
    void inspectedHandChanged();
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
private:
    QJsonObject data;
    int tableTheme=0, faceTheme=0, backTheme=0;
    QSet<int> selected;
    QVector<QPair<QRectF,int>> hitRects,handRects;
    QVector<QRectF> cardRects;
    int inspectedHand=0;
    qreal scale=1, ox=0, oy=0;
    void paintSeat(QPainter &p,int index,QPointF at);
    void paintBankroll(QPainter &p,int index,const QRectF &area,bool side=false);
};
}
