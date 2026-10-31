#include "cards.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>

CardButton::CardButton(QWidget *parent):QAbstractButton(parent) {
    setFocusPolicy(Qt::StrongFocus); setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}
void CardButton::setCard(int v,bool paired,const QString &b) {
    number=v; match=paired; badge=b;
    setAccessibleName(v==Golf::Hidden ? "Face-down card" : v==Golf::Hidden+1 ? "Empty card space" : QString("%1 strokes%2").arg(v).arg(paired?", matched":""));
    update();
}
void CardButton::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.translate(width()/2.0,height()/2.0);p.rotate(rotation);
    const bool sideways=rotation==90 || rotation==270;
    p.scale((sideways?height():width())/88.0,(sideways?width():height())/122.0);
    p.translate(-44,-61);
    QRectF card(3,2,82,115);
    p.setPen(Qt::NoPen); p.setBrush(QColor(0,0,0,55)); p.drawRoundedRect(card.translated(0,3),8,8);
    bool back=number==Golf::Hidden, empty=number==Golf::Hidden+1;
    QColor ink=number<0 ? QColor("#206446") : number<=3 ? QColor("#356f91") : number<=7 ? QColor("#946d27") : QColor("#ad5148");
    QLinearGradient g(0,0,88,122);
    g.setColorAt(0,back?QColor("#22554e"):QColor("#fffaf0"));
    g.setColorAt(1,back?QColor("#153d38"):QColor("#eae4d6"));
    p.setBrush(empty?QBrush(QColor("#153b34")):QBrush(g));
    p.setPen(QPen(hasFocus() || (underMouse() && isEnabled()) ? QColor("#f4ce78") : match ? QColor("#d6b968") : back?QColor("#508279"):QColor("#cec9bb"),hasFocus() || match?2.5:1));
    p.drawRoundedRect(card,8,8);
    if(empty) {
        p.setPen(QColor("#71958a")); QFont f=font(); f.setPixelSize(11); p.setFont(f); p.drawText(card,Qt::AlignCenter,"DRAWN\nCARD"); return;
    }
    if(back) {
        p.save(); QPainterPath clip; clip.addRoundedRect(card.adjusted(5,5,-5,-5),4,4); p.setClipPath(clip);
        p.setPen(QPen(QColor(139,184,160,30),0.7));
        for(int x=-120;x<200;x+=12) { p.drawLine(x,0,x+122,122); p.drawLine(x,0,x-122,122); }
        p.restore();
        p.setPen(QPen(QColor("#9bbbac"),1)); p.setBrush(QColor("#19463f")); p.drawEllipse(QPointF(44,60),23,23);
        p.setPen(QColor("#f1ddb1")); QFont f=font(); f.setPixelSize(31); f.setBold(true); p.setFont(f); p.drawText(QRectF(15,33,58,50),Qt::AlignCenter,"T");
        if(!badge.isEmpty()) { f.setPixelSize(10); f.setBold(false); p.setFont(f); p.setPen(QColor("#d2e3d6")); p.drawText(QRectF(4,96,80,16),Qt::AlignCenter,badge); }
        return;
    }
    p.setPen(ink); QFont f=font(); f.setBold(true); f.setPixelSize(17); p.setFont(f);
    QString text=number<0?QString::fromUtf8("−5"):QString::number(number);
    p.drawText(QRectF(10,7,60,24),Qt::AlignLeft|Qt::AlignVCenter,text);
    p.setPen(QPen(QColor(ink.red(),ink.green(),ink.blue(),40),1)); p.setBrush(Qt::NoBrush); p.drawEllipse(QPointF(44,60),27,27);
    p.setPen(ink); f.setPixelSize(number>=10?36:40); p.setFont(f); p.drawText(QRectF(6,32,76,52),Qt::AlignCenter,text);
    f.setPixelSize(8); f.setBold(false); p.setFont(f); p.drawText(QRectF(6,88,76,13),Qt::AlignCenter,number==-5?"HOLE IN ONE":"STROKES");
    if(match) { f.setPixelSize(9); f.setBold(true); p.setFont(f); p.drawText(QRectF(6,101,76,12),Qt::AlignCenter,number==-5?"PAIR  −10":"PAIR  0"); }
}
