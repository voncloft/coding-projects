#include "cards.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <algorithm>
#include <array>

CardButton::CardButton(QWidget *parent):QAbstractButton(parent) {
    setFocusPolicy(Qt::StrongFocus); setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
}
void CardButton::setCard(int v,bool paired,const QString &b) {
    number=v; match=paired; badge=b;
    setAccessibleName(v==Golf::Hidden ? "Face-down card" : v==Golf::Hidden+1 ? "Empty card space" : QString("%1 strokes%2").arg(v).arg(paired?", matched":""));
    update();
}
QStringList CardButton::themeNames() {
    return {"Classic", "Midnight", "Ocean", "Ruby", "Forest", "Ivory",
        "Slate", "Copper", "Violet", "Sunrise", "Blueprint", "High contrast"};
}
void CardButton::setAppearance(int face,int back) {
    face=std::clamp(face,0,11);back=std::clamp(back,0,11);
    if(faceStyle==face && backStyle==back)return;
    faceStyle=face;backStyle=back;update();
}
void CardButton::paintEvent(QPaintEvent *) {
    struct Palette { const char *paper,*shade,*ink,*back,*accent; };
    static const std::array<Palette,12> palettes{{
        {"#fffaf0","#eae4d6","#244e43","#153d38","#d8bd7c"},
        {"#263044","#111827","#f1e5bf","#12192c","#b7c5ef"},
        {"#effcff","#ceeaf3","#124c70","#064971","#79d7ef"},
        {"#fff4f2","#f4d5d5","#842b3b","#711e32","#efb7ad"},
        {"#f1f7e7","#d9e7c2","#284d25","#25442b","#c4dc98"},
        {"#fffdf2","#f1e7d0","#57442c","#66513a","#f0d8a4"},
        {"#edf1f5","#d5dde6","#273e54","#263749","#b0c8dd"},
        {"#fff0df","#e9cdb3","#763d20","#68351e","#edb677"},
        {"#f8f0ff","#e4d2f3","#673c88","#482863","#dfbaf7"},
        {"#fff9dd","#f8dfaa","#7b4913","#844114","#ffe3a0"},
        {"#123863","#092347","#f1f8ff","#103562","#a8d7ff"},
        {"#ffffff","#ffffff","#000000","#080808","#ffffff"}
    }};
    const bool back=number==Golf::Hidden,empty=number==Golf::Hidden+1;
    const int style=back?backStyle:faceStyle;
    const auto &theme=palettes[style];
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    p.translate(width()/2.0,height()/2.0);p.rotate(rotation);
    const bool sideways=rotation==90 || rotation==270;
    p.scale((sideways?height():width())/88.0,(sideways?width():height())/122.0);
    p.translate(-44,-61);
    QRectF card(3,2,82,115);
    p.setPen(Qt::NoPen);p.setBrush(QColor(0,0,0,55));p.drawRoundedRect(card.translated(0,3),8,8);
    QColor ink(theme.ink),accent(theme.accent),background(theme.back);
    if(faceStyle==0 && !back)ink=number<0?QColor("#206446"):number<=3?QColor("#356f91"):number<=7?QColor("#946d27"):QColor("#ad5148");
    QLinearGradient gradient(0,0,88,122);
    gradient.setColorAt(0,back?background.lighter(125):QColor(theme.paper));
    gradient.setColorAt(1,back?background:QColor(theme.shade));
    p.setBrush(empty?QBrush(QColor("#153b34")):QBrush(gradient));
    p.setPen(QPen(hasFocus() || (underMouse() && isEnabled())?QColor("#f4ce78"):match?QColor("#c6932c"):back?accent:QColor(theme.shade).darker(125),hasFocus() || match?2.5:1));
    p.drawRoundedRect(card,8,8);
    QFont f=font();
    if(empty){p.setPen(QColor("#b5d0c4"));f.setPixelSize(11);p.setFont(f);p.drawText(card,Qt::AlignCenter,"DRAWN\nCARD");return;}
    if(back) {
        p.save();QPainterPath clip;clip.addRoundedRect(card.adjusted(5,5,-5,-5),4,4);p.setClipPath(clip);
        QColor pattern=accent;pattern.setAlpha(style==11?160:90);p.setPen(QPen(pattern,1));p.setBrush(Qt::NoBrush);
        switch(style%6) {
        case 0:
            for(int x=-120;x<200;x+=12){p.drawLine(x,0,x+122,122);p.drawLine(x,0,x-122,122);}
            break;
        case 1:
            for(int y=12;y<120;y+=12)for(int x=10;x<88;x+=12){p.drawLine(x-3,y,x+3,y);p.drawLine(x,y-3,x,y+3);}
            break;
        case 2:
            for(int y=0;y<140;y+=12){QPainterPath wave;wave.moveTo(0,y);wave.cubicTo(22,y-12,66,y+12,88,y);p.drawPath(wave);}
            break;
        case 3:
            for(int y=6;y<122;y+=18)for(int x=8;x<88;x+=18)p.drawPolygon(QPolygonF{{double(x),double(y-5)},{double(x+5),double(y)},{double(x),double(y+5)},{double(x-5),double(y)}});
            break;
        case 4:
            for(int y=12;y<120;y+=12)for(int x=12;x<86;x+=12)p.drawEllipse(QPointF(x,y),2,2);
            break;
        case 5:
            for(int d=0;d<35;d+=7)p.drawRoundedRect(card.adjusted(d+6,d+6,-d-6,-d-6),4,4);
            break;
        }
        p.restore();p.setPen(QPen(accent,1.5));p.setBrush(background);
        if(style%3==0)p.drawEllipse(QPointF(44,60),23,23);
        else if(style%3==1)p.drawRoundedRect(QRectF(21,37,46,46),5,5);
        else p.drawPolygon(QPolygonF{{44,32},{72,60},{44,88},{16,60}});
        p.setPen(accent);f.setPixelSize(31);f.setBold(true);p.setFont(f);p.drawText(QRectF(15,33,58,50),Qt::AlignCenter,"T");
        if(!badge.isEmpty()){f.setPixelSize(11);p.setFont(f);p.drawText(QRectF(4,96,80,16),Qt::AlignCenter,badge);}
        return;
    }
    const QString text=number<0?QString::fromUtf8("−5"):QString::number(number);
    QColor faint=ink;faint.setAlpha(style==11?200:70);p.setPen(QPen(faint,1));p.setBrush(Qt::NoBrush);
    switch(style%6) {
    case 0:p.drawEllipse(QPointF(44,60),27,27);break;
    case 1:p.drawRoundedRect(QRectF(15,33,58,53),9,9);p.drawLine(15,92,73,92);break;
    case 2:p.drawLine(13,32,75,32);p.drawLine(13,86,75,86);break;
    case 3:p.drawPolygon(QPolygonF{{44,28},{76,60},{44,92},{12,60}});
            break;
    case 4:p.drawRoundedRect(card.adjusted(6,6,-6,-6),4,4);
            break;
    case 5:p.drawLine(12,31,76,31);p.drawLine(12,85,76,85);p.drawEllipse(QPointF(44,98),2,2);
            break;
    }
    p.setPen(ink);f.setBold(true);f.setPixelSize(17);p.setFont(f);p.drawText(QRectF(10,7,50,24),Qt::AlignLeft|Qt::AlignVCenter,text);
    f.setPixelSize(number>=10?36:40);p.setFont(f);p.drawText(QRectF(6,32,76,52),Qt::AlignCenter,text);
    f.setPixelSize(8);f.setBold(false);p.setFont(f);p.drawText(QRectF(6,88,76,13),Qt::AlignCenter,number==-5?"HOLE IN ONE":"STROKES");
    if(!badge.isEmpty()){f.setPixelSize(10);f.setBold(true);p.setFont(f);p.drawText(QRectF(62,9,14,19),Qt::AlignRight|Qt::AlignVCenter,badge);}
    if(match){f.setPixelSize(number==-5?7:9);f.setBold(true);p.setFont(f);p.drawText(QRectF(6,102,76,12),Qt::AlignCenter,number==-5?"MATCH · −5 EACH":"MATCH · 0");}
}
