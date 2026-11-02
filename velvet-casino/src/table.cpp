// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Velvet Casino contributors
#include "table.h"
#include <cmath>
namespace casino {
const QVector<Theme> &themes() {
    static const QVector<Theme> items{
        {"Emerald club","#073a32","#146453","#d9bd7b","#fffaf0","#193b36","#b13646","#146453"},
        {"Midnight ink","#11182b","#253858","#8eaee0","#eef3ff","#15223d","#bd3e64","#233656"},
        {"Ruby lounge","#401321","#78293f","#e6a3ab","#fff2f2","#3a1626","#bb254e","#752c43"},
        {"Ocean blue","#07314d","#126790","#7ad9ec","#f0fbff","#0e3758","#d04b61","#17638b"},
        {"Royal plum","#301739","#663e73","#d9b6eb","#fcf2ff","#3e2456","#bf4172","#653d77"},
        {"Champagne","#4b402c","#80704d","#f7dc9e","#fff7de","#453b20","#a4452f","#8c7641"},
        {"Copper canyon","#4b2418","#8a4a2d","#e7ae79","#fff1dc","#4a2920","#b84225","#925339"},
        {"Arctic ice","#274857","#547e8b","#c3eff2","#efffff","#254a5c","#c33d63","#478c9a"},
        {"Rose velvet","#482a3e","#81506b","#f2b3ca","#fff0f5","#522c42","#bc3569","#a15377"},
        {"Obsidian gold","#16191b","#333a3b","#e4c46f","#282d31","#f6e5ba","#ff9192","#20262a"},
        {"Forest lodge","#22351f","#476340","#c4cd97","#f2f5e7","#263b28","#ac4939","#496745"},
        {"Sunset boulevard","#4a2132","#995240","#ffd29c","#fff1e1","#5a3043","#cd435d","#b25846"},
        {"Teal oasis","#093f43","#24777a","#8ce2cc","#edfffb","#164747","#bd4b55","#277c78"},
        {"Ivory classic","#55503f","#8d866a","#ece3ba","#fffff4","#272822","#b42f36","#615b43"},
        {"Slate atelier","#28343e","#4b6373","#bbd1e0","#eff2f4","#273b4b","#b85061","#506c80"},
        {"Neon violet","#211134","#48265f","#db97ff","#241e36","#e8ddff","#ff80c4","#5e3287"},
        {"Cherry wood","#361e1c","#6c3831","#d7ab89","#fff0df","#442722","#a83136","#7d4038"},
        {"Lavender dusk","#35334d","#625c88","#cbbfec","#f8f2ff","#454268","#b04779","#72659e"},
        {"Desert sand","#4b3a2a","#89704c","#edcf9d","#fff2d5","#4b3e29","#b95135","#a2824c"},
        {"Sapphire palace","#152748","#305294","#9dbfff","#f3f5ff","#233560","#b73c6b","#355ba2"}
    };
    return items;
}
static void label(QPainter &p,const QRectF &r,const QString &text,int size,const QColor &color,bool bold=false,int align=Qt::AlignCenter) {
    QFont font("Noto Sans",size); font.setBold(bold); p.setFont(font); p.setPen(color); p.drawText(r,align,text);
}
void paintChip(QPainter &p,const QRectF &rect,int colorIndex,int value) {
    static const std::array<QColor,5> colors{QColor("#c8444e"),QColor("#387fce"),QColor("#f1f0e7"),QColor("#238b58"),QColor("#eac54e")};
    const QColor color=colors[std::clamp(colorIndex,0,4)];
    p.save();p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);p.setBrush(QColor(0,0,0,80));p.drawEllipse(rect.translated(0,2));
    p.setPen(QPen(color.darker(155),1.5));p.setBrush(color);p.drawEllipse(rect);
    p.translate(rect.center());
    for(int i=0;i<8;++i){p.save();p.rotate(i*45);p.setPen(QPen(colorIndex==2?QColor("#82959b"):QColor(255,255,255,210),rect.width()*0.07));p.drawLine(QPointF(0,-rect.height()*0.35),QPointF(0,-rect.height()*0.45));p.restore();}
    p.translate(-rect.center());p.setBrush(Qt::NoBrush);p.setPen(QPen(color.darker(145),0.7));p.drawEllipse(rect.adjusted(rect.width()*0.22,rect.height()*0.22,-rect.width()*0.22,-rect.height()*0.22));
    QString text=QString::number(value);QFont font("Noto Sans",10);font.setBold(true);font.setPixelSize(std::max(7,int(rect.height()*0.26)));
    while(QFontMetricsF(font).horizontalAdvance(text)>rect.width()*0.72&&font.pixelSize()>5)font.setPixelSize(font.pixelSize()-1);
    p.setFont(font);p.setPen(colorIndex==2||colorIndex==4?QColor("#1c3035"):QColor("#ffffff"));p.drawText(rect,Qt::AlignCenter,text);p.restore();
}
static void paintChipPile(QPainter &p,QPointF base,int colorIndex,int value,int count) {
    static const std::array<QColor,5> colors{QColor("#c8444e"),QColor("#387fce"),QColor("#f1f0e7"),QColor("#238b58"),QColor("#eac54e")};
    const QColor color=colors[colorIndex],marks=colorIndex==2?QColor("#82959b"):QColor("#fff5dc");
    p.save();p.setPen(Qt::NoPen);p.setBrush(QColor(0,0,0,85));p.drawEllipse(QRectF(base.x()-2,base.y()-6,32,9));
    // Individual chip rims make a real pile, viewed at an angle. The base
    // stays put while each added chip lifts the top of the pile.
    QRectF top;
    for(int layer=0;layer<count;++layer){
        top=QRectF(base.x(),base.y()-11-layer*2.2,27,9);
        QLinearGradient edge(top.left(),0,top.right(),0);edge.setColorAt(0,color.darker(170));edge.setColorAt(0.45,color);edge.setColorAt(1,color.darker(150));
        p.setPen(QPen(color.darker(190),0.6));p.setBrush(edge);p.drawRoundedRect(top.adjusted(0,3,0,2.2),4,4);
        p.setPen(QPen(marks,2));for(qreal x:{5.0,13.5,22.0})p.drawLine(QPointF(top.x()+x,top.y()+7),QPointF(top.x()+x,top.y()+10));
        p.setPen(QPen(color.darker(160),0.6));p.setBrush(color.lighter(112));p.drawEllipse(top);
    }
    p.setPen(QPen(marks,1.7));for(int angle:{15,75,135,195,255,315})p.drawArc(top.adjusted(1,1,-1,-1),angle*16,15*16);
    QFont font("Noto Sans");font.setBold(true);font.setPixelSize(7);QString text=QString::number(value);
    while(QFontMetricsF(font).horizontalAdvance(text)>23&&font.pixelSize()>4)font.setPixelSize(font.pixelSize()-1);
    p.setFont(font);p.setPen(colorIndex==2||colorIndex==4?QColor("#1c3035"):QColor("#ffffff"));p.drawText(top,Qt::AlignCenter,text);p.restore();
}
static void paintInventory(QPainter &p,const QRectF &area,const ChipBreakdown &bank,const ChipValues &values,double half=0) {
    int pileCount=bank.change||half?1:0;for(int count:bank.counts)pileCount+=(count+19)/20;
    if(!pileCount){label(p,area,"—",11,QColor("#748c88"));return;}
    constexpr int cellWidth=32,cellHeight=66;
    int estimate=std::clamp(int(std::round(std::sqrt(qreal(pileCount)*area.width()*cellHeight/(area.height()*cellWidth)))),1,pileCount);
    int columns=estimate; qreal zoom=0;
    for(int candidate=std::max(1,estimate-2);candidate<=std::min(pileCount,estimate+2);++candidate){
        int rows=(pileCount+candidate-1)/candidate;qreal fit=std::min({1.0,area.width()/(candidate*cellWidth),area.height()/(rows*cellHeight)});
        if(fit>zoom){zoom=fit;columns=candidate;}
    }
    const int rows=(pileCount+columns-1)/columns;
    p.save();p.translate(area.center()-QPointF(columns*cellWidth*zoom/2,rows*cellHeight*zoom/2));p.scale(zoom,zoom);
    int position=0;
    auto tile=[&](int color,int count){QPixmap image(cellWidth,cellHeight);image.fill(Qt::transparent);QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);paintChipPile(painter,QPointF(2,54),color,values[color],count);label(painter,QRectF(0,55,cellWidth,11),QString("×%1").arg(count),6,QColor("#dce4d8"));return image;};
    auto run=[&](const QPixmap &image,int count){
        // Tile full piles in at most three rectangles, even for very large
        // balances. Every pile is represented; there is no visual chip cap.
        auto strip=[&](int length,int rowCount){QPointF at((position%columns)*cellWidth,(position/columns)*cellHeight);p.setBrushOrigin(at);p.fillRect(QRectF(at,QSizeF(length*cellWidth,rowCount*cellHeight)),QBrush(image));position+=length*rowCount;};
        if(position%columns&&count){int n=std::min(count,columns-position%columns);strip(n,1);count-=n;}
        if(count>=columns){int n=count/columns;strip(columns,n);count%=columns;}
        if(count)strip(count,1);
    };
    for(int color=0;color<5;++color)if(bank.counts[color]){
        int count=bank.counts[color];if(count>=20)run(tile(color,20),count/20);
        if(count%20)run(tile(color,count%20),1);
    }
    if(bank.change||half)label(p,QRectF((position%columns)*cellWidth,(position/columns)*cellHeight+30,cellWidth,30),"+"+formatUnits(bank.change+half),7,QColor("#dce4d8"));
    p.restore();
}
static void paintStack(QPainter &p,QPointF at,double amount,const ChipValues &values,const QJsonArray &chosen={}) {
    amount=std::max(0.0,amount);auto bank=breakDownChips(int(amount),values);
    if(chosen.size()==5){bank.change=0;for(int i=0;i<5;++i)bank.counts[i]=chosen[i].toInt();bank.change=std::max(0,int(amount)-int(chipInventoryValue(bank,values)));}
    paintInventory(p,QRectF(at,QSizeF(206,47)),bank,values,amount-int(amount));
}
static void paintShoe(QPainter &p,int remaining,int decks,const Theme &theme) {
    remaining=std::clamp(remaining,0,decks*52);
    p.save();p.translate(696,132);
    label(p,QRectF(0,-21,108,19),"SHOE",9,QColor(theme.accent),true);
    const QPointF rear(4,13),across(39,-12),length(59,65),depth(0,18);
    const QPointF front=rear+length;
    QPolygonF base{rear+depth,rear+across+depth,front+across+depth,front+depth};
    p.setPen(Qt::NoPen);p.setBrush(QColor(0,0,0,90));p.drawPolygon(base.translated(2,4));
    QColor acrylic(theme.accent);acrylic.setAlpha(35);p.setBrush(acrylic);p.setPen(QPen(QColor(200,224,224,125),1));p.drawPolygon(base);
    // Cards sit in a long transparent tray. As cards leave the front, the
    // pack and its dark follower move down the tray toward the curved outlet.
    if(remaining){
        qreal fraction=qreal(remaining)/(decks*52);QPointF back=rear+length*(1-fraction)+QPointF(0,11),tip=front+QPointF(0,11);
        p.setBrush(QColor("#eee9d7"));p.setPen(QPen(QColor("#b7c1b7"),0.6));p.drawPolygon(QPolygonF{back,back+across,tip+across,tip});
        p.setPen(QPen(QColor("#9faeaa"),0.55));int edges=std::max(1,int(fraction*38));
        for(int i=1;i<edges;++i){QPointF edge=back+(tip-back)*(qreal(i)/edges);p.drawLine(edge,edge+across);}
        QColor follower(theme.back);p.setBrush(follower.darker(180));p.setPen(QPen(QColor(theme.accent),0.8));
        p.drawPolygon(QPolygonF{back-QPointF(0,12),back+across-QPointF(0,12),back+across,back});
    }
    QColor glass(theme.light);glass.setAlpha(42);p.setBrush(glass);p.setPen(QPen(QColor(222,239,235,150),1.1));
    p.drawPolygon(QPolygonF{rear,rear+depth,front+depth,front});
    p.drawPolygon(QPolygonF{rear+across,rear+across+depth,front+across+depth,front+across});
    p.drawLine(rear,rear+across);p.drawLine(rear,front);p.drawLine(rear+across,front+across);
    // Theme-tinted front housing, with an open U-shaped thumb cutout.
    QPainterPath housing;housing.moveTo(51,65);housing.lineTo(92,51);housing.lineTo(106,88);housing.lineTo(65,103);housing.closeSubpath();
    QPainterPath opening;opening.moveTo(73,102);opening.lineTo(69,88);opening.cubicTo(61,66,89,57,97,81);opening.lineTo(101,92);opening.closeSubpath();
    QLinearGradient metal(51,64,103,94);metal.setColorAt(0,QColor(theme.accent).darker(180));metal.setColorAt(0.4,QColor(theme.accent).lighter(115));metal.setColorAt(0.65,QColor(theme.back));metal.setColorAt(1,QColor(theme.accent).darker(180));
    p.setBrush(metal);p.setPen(QPen(QColor(theme.accent),1.1));p.drawPath(housing.subtracted(opening));
    p.setPen(QPen(QColor("#11191c"),1));p.setBrush(QColor("#11191c"));p.drawPolygon(QPolygonF{{48,57},{88,44},{92,51},{51,65}});
    label(p,QRectF(0,109,110,18),QString("%1 / %2").arg(remaining).arg(decks*52),9,QColor("#dce4d8"),true);
    p.restore();
}
QVector<QPointF> cardPips(int rank) {
    if(rank==14)return {{0.5,0.82}};
    if(rank==2||rank>10)return {{0.5,0.20},{0.5,0.82}};
    if(rank==3)return {{0.5,0.20},{0.23,0.82},{0.77,0.82}};
    QVector<QPointF> points;
    int rows=rank<=5?2:rank<=8?3:rank==9?4:5;
    for(int i=0;i<rows;++i){double y=0.22+0.60*i/(rows-1);points.append({0.14,y});points.append({0.86,y});}
    if(rank==5)points.append({0.5,0.82});
    if(rank==7||rank==8||rank==9)points.append({0.5,0.18});
    if(rank==8)points.append({0.5,0.86});
    return points;
}
static void paintFaceDecoration(QPainter &p,const QRectF &r,int style,const Theme &theme) {
    // All decoration stays behind the rank and outside the pip columns.
    p.save();p.translate(r.topLeft());p.scale(r.width()/100.0,r.height()/140.0);
    QPainterPath clip;clip.addRoundedRect(QRectF(2,2,96,136),8,8);p.setClipPath(clip);
    const QColor accent(style==9||style==15?theme.accent:theme.back);
    QColor tint=accent;tint.setAlpha(style==9||style==15?24:18);
    p.setBrush(Qt::NoBrush);p.setPen(QPen(accent,2));
    auto line=[&](qreal x1,qreal y1,qreal x2,qreal y2){p.drawLine(QPointF(x1,y1),QPointF(x2,y2));};
    auto frame=[&](qreal radius=4){p.drawRoundedRect(QRectF(4,4,92,132),radius,radius);};
    auto diamond=[&](QPointF center,qreal size){p.drawPolygon(QPolygonF{center+QPointF(0,-size),center+QPointF(size,0),center+QPointF(0,size),center+QPointF(-size,0)});};
    switch(style){
    case 0: // Emerald: club-room corner flourishes.
        frame();for(int x:{8,92})for(int y:{8,132})diamond({qreal(x),qreal(y)},4);break;
    case 1: // Midnight: broad ink rails and a softly framed rank.
        p.fillRect(QRectF(2,2,5,136),accent);p.fillRect(QRectF(93,2,5,136),accent);
        p.setBrush(tint);p.drawRoundedRect(QRectF(25,40,50,66),8,8);break;
    case 2: // Ruby: red ribbon ends.
        p.setBrush(accent);p.drawPolygon(QPolygonF{{36,2},{96,2},{96,19},{86,13},{76,19},{66,13},{56,19},{46,13}});
        p.drawPolygon(QPolygonF{{4,121},{14,127},{24,121},{34,127},{44,121},{64,138},{4,138}});break;
    case 3: // Ocean: rolling waves along both edges.
        for(int edge:{4,92}){QPainterPath wave;wave.moveTo(edge,4);for(int y=4;y<136;y+=22)wave.cubicTo(edge+7,y+6,edge-3,y+16,edge,y+22);p.drawPath(wave);}break;
    case 4: // Royal: an arched, colored rank medallion.
        frame();p.setBrush(tint);p.drawRoundedRect(QRectF(24,36,52,74),26,20);
        for(int x:{38,50,62}){line(x,6,x,13);p.drawEllipse(QPointF(x,5),2,2);}break;
    case 5: // Champagne: art-deco rays.
        for(int offset:{0,5,10}){line(34+offset,5,92-offset,5);line(92-offset,5,92-offset,26-offset);line(8+offset,114+offset,8+offset,135);line(8+offset,135,66-offset,135);}break;
    case 6: // Copper: stepped canyon borders.
        for(int edge:{3,92})for(int y=4;y<136;y+=22){line(edge,y,edge+5,y);line(edge+5,y,edge+5,y+11);line(edge+5,y+11,edge,y+11);line(edge,y+11,edge,y+22);}
        break;
    case 7: // Arctic: angular ice corners.
        p.setBrush(tint);p.drawPolygon(QPolygonF{{32,3},{97,3},{97,28},{83,13},{70,22},{59,7}});
        p.drawPolygon(QPolygonF{{3,112},{17,127},{30,118},{41,133},{68,137},{3,137}});break;
    case 8: // Rose: petal border.
        for(int y=20;y<130;y+=17){p.drawEllipse(QRectF(2,y,5,10));p.drawEllipse(QRectF(93,y,5,10));}frame(12);break;
    case 9: // Obsidian: gold double frame on dark paper.
        frame();p.setPen(QPen(accent,1));p.drawRoundedRect(QRectF(7,7,86,126),3,3);p.setBrush(tint);diamond({50,73},30);break;
    case 10: // Forest: small leaf silhouettes.
        for(int y=22;y<124;y+=18){line(4,y-5,4,y+11);line(96,y-5,96,y+11);p.setBrush(accent);p.drawEllipse(QRectF(3,y,5,8));p.drawEllipse(QRectF(92,y,5,8));}break;
    case 11: // Sunset: warm horizon stripes.
        for(int y=0;y<3;++y){QColor color=y==1?QColor(theme.red):accent;color.setAlpha(220-y*35);p.fillRect(QRectF(32,3+y*4,65,3),color);p.fillRect(QRectF(3,126+y*4,65,3),color);}break;
    case 12: // Teal: linked rings and a circular rank field.
        p.setBrush(tint);p.drawEllipse(QRectF(24,41,52,64));p.setBrush(Qt::NoBrush);
        for(int x:{39,50,61}){p.drawEllipse(QPointF(x,7),5,4);p.drawEllipse(QPointF(x,133),5,4);}break;
    case 13: // Ivory: traditional fine double keyline.
        p.setPen(QPen(accent,1));frame();p.drawRect(QRectF(7,7,86,126));break;
    case 14: // Slate: dashed technical border and square rank field.
        p.setPen(QPen(accent,2,Qt::DashLine));frame(0);p.setPen(Qt::NoPen);p.setBrush(tint);p.drawRect(QRectF(25,39,50,68));break;
    case 15: // Neon: luminous violet edge and rank halo.
        for(int width:{7,3,1}){QColor glow=accent;glow.setAlpha(width==7?45:width==3?120:255);p.setPen(QPen(glow,width));frame(8);}p.setBrush(tint);p.setPen(Qt::NoPen);p.drawEllipse(QRectF(23,38,54,70));break;
    case 16: // Cherry wood: carved grain rails.
        for(int x:{3,6,94,97}){QPainterPath grain;grain.moveTo(x,2);grain.cubicTo(x-3,48,x+3,95,x,138);p.drawPath(grain);}break;
    case 17: // Lavender: beaded border and scalloped ends.
        p.setPen(QPen(accent,2,Qt::DotLine));frame(10);p.setPen(QPen(accent,1.5));
        for(int x=34;x<88;x+=12){p.drawArc(QRectF(x,2,12,12),180*16,180*16);p.drawArc(QRectF(88-x,126,12,12),0,180*16);}break;
    case 18: // Desert: sunburst corners.
        for(QPointF center:{QPointF(91,9),QPointF(9,131)}){p.drawEllipse(center,4,4);for(int i=0;i<8;++i){double angle=i*3.141592653589793/4;QPointF ray(std::cos(angle),std::sin(angle));p.drawLine(center+ray*7,center+ray*12);}}break;
    case 19: // Sapphire: faceted blue frame.
        p.drawPolygon(QPolygonF{{12,3},{88,3},{97,12},{97,128},{88,137},{12,137},{3,128},{3,12}});
        for(int x=36;x<91;x+=14){diamond({qreal(x),8},4);diamond({qreal(100-x),132},4);}break;
    }
    p.restore();
}
void paintCard(QPainter &p,const QRectF &r,const QJsonObject &card,int face,int back,bool selected) {
    face=std::clamp(face,0,19);back=std::clamp(back,0,19);
    const auto &f=themes()[std::clamp(face,0,19)], &b=themes()[std::clamp(back,0,19)];
    p.save(); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen); p.setBrush(QColor(0,0,0,65)); p.drawRoundedRect(r.translated(0,3),6,6);
    p.setBrush(card["hidden"].toBool()?QColor(b.back):QColor(f.paper));
    p.setPen(QPen(selected?QColor("#ffda83"):QColor("#d5d0c5"),selected?3:0.8)); p.drawRoundedRect(r,6,6);
    if(card["hidden"].toBool()) {
        auto inside=r.adjusted(5,5,-5,-5); p.setPen(QPen(QColor(b.accent),0.7)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(inside,3,3);
        p.setClipRect(inside.adjusted(2,2,-2,-2)); QColor pattern(b.accent); pattern.setAlpha(95); p.setPen(QPen(pattern,0.8));
        int style=back%5;
        for(qreal x=inside.left()-inside.height();x<inside.right()+inside.height();x+=9) {
            if(style==0 || style==3) {p.drawLine(QPointF(x,inside.top()),QPointF(x+inside.height(),inside.bottom()));p.drawLine(QPointF(x,inside.top()),QPointF(x-inside.height(),inside.bottom()));}
            else if(style==1) {p.drawLine(QPointF(x,inside.top()),QPointF(x,inside.bottom()));}
        }
        if(style==2 || style==4) for(qreal y=inside.top()+6;y<inside.bottom();y+=12) for(qreal x=inside.left()+6;x<inside.right();x+=12) {
            if(style==2) p.drawEllipse(QPointF(x,y),2,2); else p.drawRect(QRectF(x-3,y-3,6,6));
        }
        p.setClipping(false); p.setBrush(QColor(b.back)); p.setPen(QPen(QColor(b.accent),1)); p.drawEllipse(r.center(),r.width()*0.2,r.width()*0.2);
        label(p,QRectF(r.center().x()-15,r.center().y()-17,30,34),QString::fromUtf8("♠"),int(r.width()*0.25),QColor(b.accent));
    } else {
        paintFaceDecoration(p,r,face,f);
        int rank=card["rank"].toInt(14),suit=card["suit"].toInt();
        QString rankText=rank==14?"A":rank==13?"K":rank==12?"Q":rank==11?"J":QString::number(rank);
        QString suitText=QStringList{"♣","♦","♥","♠"}.value(suit);
        QColor ink(suit==1||suit==2?f.red:f.ink);
        // Keep the center clear for the rank; the surrounding pips match 2–10 exactly.
        QFont pipFont("Noto Sans");pipFont.setPixelSize(std::max(6,int(r.width()*0.20)));p.setFont(pipFont);p.setPen(ink);
        for(auto pos:cardPips(rank)){
            QPointF center(r.x()+pos.x()*r.width(),r.y()+pos.y()*r.height());
            p.drawText(QRectF(center.x()-r.width()*0.12,center.y()-r.height()*0.09,r.width()*0.24,r.height()*0.18),Qt::AlignCenter,suitText);
        }
        QFont corner("Noto Sans");corner.setBold(true);corner.setPixelSize(std::max(7,int(r.width()*0.18)));p.setFont(corner);
        p.drawText(QRectF(r.x()+3,r.y()+1,r.width()*0.35,r.height()*0.17),Qt::AlignLeft|Qt::AlignVCenter,rankText);
        QRectF centerRank(r.x()+r.width()*0.21,r.y()+r.height()*0.22,r.width()*0.58,r.height()*0.60);
        static const QStringList rankFonts{"Noto Serif","Noto Sans","Noto Serif","Noto Sans","Noto Serif","Noto Serif","DejaVu Sans Mono","Noto Sans","Noto Serif","Noto Serif","Noto Serif","Noto Sans","Noto Sans","Noto Serif","DejaVu Sans Mono","Noto Sans","Noto Serif","Noto Serif","DejaVu Sans Mono","Noto Serif"};
        QFont large(rankFonts[face]);large.setBold(true);large.setItalic(face==4||face==8||face==17);large.setPixelSize(int(r.width()*0.82));
        while(large.pixelSize()>8&&(QFontMetricsF(large).horizontalAdvance(rankText)>centerRank.width()||QFontMetricsF(large).height()>centerRank.height()))large.setPixelSize(large.pixelSize()-1);
        p.setFont(large);p.drawText(centerRank,Qt::AlignCenter,rankText);
    }
    p.restore();
}
Table::Table(QWidget *parent):QWidget(parent) { setMinimumSize(760,430);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);setObjectName("casinoTable"); }
void Table::setState(const QJsonObject &s) {
    if(data["hand"]!=s["hand"] || data["turn"]!=s["turn"] || data["phase"]!=s["phase"]) clearSelection();
    int me=s["viewer"].toInt(-1);auto people=s["players"].toArray();int count=me>=0&&me<people.size()?people[me].toObject()["hands"].toArray().size():0;
    if(data["hand"]!=s["hand"]||data["viewer"]!=s["viewer"])inspectedHand=0;
    if(me>=0&&s["turn"].toInt(-1)==me&&(data["turn"]!=s["turn"]||data["handIndex"]!=s["handIndex"]||data["phase"]!=s["phase"]))inspectedHand=s["handIndex"].toInt();
    inspectedHand=std::clamp(inspectedHand,0,std::max(0,count-1));data=s; update();
}
void Table::setDisplayedHand(int index){
    int me=data["viewer"].toInt(-1);auto people=data["players"].toArray();if(me<0||me>=people.size())return;
    int count=people[me].toObject()["hands"].toArray().size();if(index<0||index>=count)return;
    inspectedHand=index;emit inspectedHandChanged();update();
}
void Table::setThemes(int t,int f,int b) {tableTheme=std::clamp(t,0,19);faceTheme=std::clamp(f,0,19);backTheme=std::clamp(b,0,19);update();}
std::vector<int> Table::discards() const {std::vector<int> out;for(int i:selected)out.push_back(i);std::sort(out.begin(),out.end());return out;}
void Table::clearSelection(){selected.clear();emit selectionChanged(0);update();}
void Table::toggleDiscard(int index){
    int me=data["viewer"].toInt(-1);auto people=data["players"].toArray();
    if(data["phase"]!="draw"||data["turn"].toInt(-1)!=me||me<0||me>=people.size()||index<0||index>=people[me].toObject()["cards"].toArray().size())return;
    if(selected.contains(index))selected.remove(index);else selected.insert(index);emit selectionChanged(selected.size());update();
}
void Table::mousePressEvent(QMouseEvent *event) {
    if(event->button()!=Qt::LeftButton&&event->button()!=Qt::RightButton)return;
    QPointF pos=(event->position()-QPointF(ox,oy))/scale;
    QPainterPath felt;felt.addRoundedRect(QRectF(60,102,1160,632),220,220);
    if(event->button()==Qt::RightButton){
        if(data["kind"].toInt()==int(Kind::Blackjack)&&data["phase"]=="blackjack"){
            for(const auto &hand:handRects)if(hand.first.contains(pos))return;
            for(const auto &card:cardRects)if(card.contains(pos))return;
            if(felt.contains(pos))emit standRequested();
        }
        return;
    }
    for(auto hit:handRects)if(hit.first.contains(pos)){setDisplayedHand(hit.second);return;}
    for(auto it=hitRects.rbegin();it!=hitRects.rend();++it)if(it->first.contains(pos)){toggleDiscard(it->second);return;}
    for(const auto &card:cardRects)if(card.contains(pos))return;
    if(felt.contains(pos))emit betConfirmed();
}
void Table::paintSeat(QPainter &p,int index,QPointF at) {
    auto person=data["players"].toArray()[index].toObject(); bool active=data["turn"].toInt(-1)==index;
    bool folded=person["folded"].toBool(); const auto &theme=themes()[tableTheme];
    QRectF plate(at.x(),at.y(),304,202);
    p.setPen(QPen(active?QColor(theme.accent):QColor(255,255,255,25),active?2:1));
    p.setBrush(QColor(9,19,22,active?200:160));p.drawRoundedRect(plate,14,14);
    if(active){p.setPen(Qt::NoPen);p.setBrush(QColor(theme.accent));p.drawEllipse(at+QPointF(16,24),3.5,3.5);}
    QString name=person["name"].toString();if(index==data["viewer"].toInt(-1))name+="  ·  YOU";
    QFont nameFont("Noto Sans",12);nameFont.setBold(true);
    int nameWidth=index==data["viewer"].toInt(-1)&&person["hands"].toArray().size()>1?145:250;
    label(p,QRectF(at.x()+27,at.y()+8,nameWidth,27),QFontMetrics(nameFont).elidedText(name,Qt::ElideRight,nameWidth),12,QColor("#f5f0df"),true,Qt::AlignLeft|Qt::AlignVCenter);

    int control=person["control"].toInt(); QString role=control==int(Control::Computer)?"COMPUTER":control==int(Control::Remote)?"NETWORK":"PLAYER";
    if(index==data["button"].toInt() && data["kind"].toInt()!=0)role+="   •   BUTTON";
    QString debt=person["houseDebt"].toInteger()>0?QString("Owes house: %1").arg(formatUnits(person["houseDebt"].toDouble())):QString{};
    QFont roleFont("Noto Sans",8);QFontMetrics roleMetrics(roleFont);
    int debtWidth=debt.isEmpty()?0:std::min(190,roleMetrics.horizontalAdvance(debt)+8);
    label(p,QRectF(at.x()+27,at.y()+31,270-debtWidth,15),roleMetrics.elidedText(role,Qt::ElideRight,270-debtWidth),8,QColor("#91a9a3"),false,Qt::AlignLeft|Qt::AlignVCenter);
    if(!debt.isEmpty())label(p,QRectF(at.x()+297-debtWidth,at.y()+31,debtWidth,15),debt,8,QColor("#f0d78a"),false,Qt::AlignRight|Qt::AlignVCenter);
    auto values=readChipValues(data["chipValues"].toArray());
    int draft=index==data["viewer"].toInt(-1)?data["draftAmount"].toInt():0;
    int bet=person["committed"].toInt()+draft;
    label(p,QRectF(at.x()+12,at.y()+51,70,37),"BET\n"+formatUnits(bet),9,QColor(theme.accent),true);
    QJsonArray chosen=person["committedChips"].toArray();if(draft){auto selected=data["draftCounts"].toArray();if(selected.size()==5&&chosen.size()==5)for(int i=0;i<5;++i)chosen[i]=chosen[i].toInt()+selected[i].toInt();else chosen=selected;}
    paintStack(p,at+QPointF(86,45),bet,values,chosen);
    if(folded)p.setOpacity(0.5);
    auto cards=person["cards"].toArray();auto hands=person["hands"].toArray();
    auto drawStatus=[&](const QRectF &banner,const QString &status,int fontSize){
        QColor color=status.contains("Bust")||status.contains("Lost")?QColor("#ff9b99"):status.contains("Won")||status.contains("Blackjack")?QColor("#99edb2"):status.contains("Tied")?QColor("#f0d78a"):QColor("#c4d3ca");
        bool outcome=status.contains("Bust")||status.contains("Blackjack")||status.contains("Won")||status.contains("Lost")||status.contains("Tied");
        if(outcome){p.setPen(QPen(color,0.8));p.setBrush(QColor("#102327"));p.drawRoundedRect(banner,5,5);}
        QFont font("Noto Sans",fontSize);font.setBold(true);
        label(p,banner.adjusted(4,0,-4,0),QFontMetrics(font).elidedText(status,Qt::ElideRight,int(banner.width())-8),fontSize,color,true);
    };
    if(!hands.isEmpty()) {
        // Show the split itself: two hands side by side, with no extra panel
        // or change to the table size. Navigation pages through larger groups.
        int count=hands.size();bool own=index==data["viewer"].toInt(-1);int visible=own?std::clamp(inspectedHand,0,count-1):active?std::clamp(data["handIndex"].toInt(),0,count-1):0;
        int first=(visible/2)*2,last=std::min(first+2,count);bool paired=count>1;
        if(own&&paired){
            label(p,QRectF(at.x()+202,at.y()+8,64,26),QString("%1 / %2").arg(visible+1).arg(count),9,QColor(theme.accent),true);
            for(int direction:{-1,1}){QRectF arrow(at.x()+(direction<0?177:268),at.y()+6,28,30);label(p,arrow,direction<0?"‹":"›",20,QColor(theme.accent),true);handRects.append({arrow,(visible+direction+count)%count});}
        }
        for(int h=first;h<last;++h){
            auto hand=hands[h].toObject();auto hc=hand["cards"].toArray();
            qreal width=paired?140:284,x=at.x()+10+(h-first)*144;
            bool playing=active&&h==data["handIndex"].toInt();
            if(paired){
                QRectF area(x,at.y()+90,width,112);p.setPen(QPen(playing?QColor(theme.accent):QColor(255,255,255,25),playing?1.5:0.6));p.setBrush(Qt::NoBrush);p.drawRoundedRect(area,6,6);
                if(own)handRects.append({area,h});
            }
            qreal cw=paired?54:58,step=std::min(cw+6,(width-cw-8)/std::max(1,int(hc.size())-1));
            qreal start=x+(width-cw-(hc.size()-1)*step)/2;
            for(int c=0;c<hc.size();++c){QRectF r(start+c*step,at.y()+116,cw,68);cardRects.append(r);paintCard(p,r,hc[c].toObject(),faceTheme,backTheme,playing);}
            QString result=hand["result"].toString();
            if(!result.isEmpty()&&data["phase"]=="finished"){double net=hand["net"].toDouble();result+=QString(" · %1%2").arg(net>0?"+":"").arg(formatUnits(net));}
            if(result.isEmpty()&&data["phase"]=="blackjack")result=hand["done"].toBool()?(paired?"Standing":"Standing · waiting for dealer"):playing?(own?"Your move":"Playing"):"Waiting to play";
            if(result.isEmpty()&&data["phase"]=="dealing")result=QString("Hand %1").arg(h+1);
            drawStatus(QRectF(x,at.y()+92,width,20),result,paired?9:11);
            QString total=(paired?QString("H%1 · ").arg(h+1):QString{})+hand["totalText"].toString()+QString(" · bet %1").arg(hand["bet"].toInt());
            label(p,QRectF(x,at.y()+184,width,17),total,paired?9:10,QColor("#f5f0df"),true);
        }
    } else if(!cards.isEmpty()) {
        qreal cw=cards.size()>5?42:48,step=std::min(cw+5,(276-cw)/std::max(1,int(cards.size())-1));
        qreal span=cw+(cards.size()-1)*step, start=at.x()+(304-span)/2;
        bool canDraw=data["phase"]=="draw" && index==data["viewer"].toInt(-1) && active;
        for(int c=0;c<cards.size();++c) {
            QRectF rect(start+c*step,at.y()+116-(canDraw&&selected.contains(c)?4:0),cw,cw*1.4);
            cardRects.append(rect);paintCard(p,rect,cards[c].toObject(),faceTheme,backTheme,canDraw&&selected.contains(c));
            if((data["kind"].toInt()==int(Kind::Stud5)||data["kind"].toInt()==int(Kind::Stud7)) && cards[c].toObject()["down"].toBool() && !cards[c].toObject()["hidden"].toBool())
                label(p,QRectF(rect.x(),at.y()+184,rect.width(),12),"DOWN",6,QColor("#d9c99c"));
            if(canDraw)hitRects.append({rect,c});
        }
    } else label(p,QRectF(at.x()+10,at.y()+105,284,50),person["inHand"].toBool()?"Ready":"Waiting for the deal",11,QColor("#9aafa9"));
    p.setOpacity(1);
    if(hands.isEmpty()){
        QString status=person["status"].toString();if(status.isEmpty()&&active)status=index==data["viewer"].toInt(-1)?"Your move":"Thinking…";
        // Status is painted above the cards and never consumes layout space.
        drawStatus(QRectF(at.x()+10,at.y()+92,284,20),status,11);
    }
}
void Table::paintBankroll(QPainter &p,int index,const QRectF &area,bool side) {
    const auto person=data["players"].toArray()[index].toObject();bool own=index==data["viewer"].toInt(-1);
    const auto values=readChipValues(data["chipValues"].toArray());double amount=person["chips"].toDouble();
    auto bank=breakDownChips(int(amount),values);auto counts=person["bankChips"].toArray();
    if(counts.size()==5){for(int i=0;i<5;++i)bank.counts[i]=counts[i].toInt();bank.change=int(person["bankChange"].toDouble());}
    int draft=own?data["draftAmount"].toInt():0;
    if(draft>0&&draft<=chipInventoryValue(bank,values)){std::array<int,5> chosen{};auto selected=data["draftCounts"].toArray();if(selected.size()==5)for(int i=0;i<5;++i)chosen[i]=selected[i].toInt();takeChipInventory(bank,draft,values,chosen);amount-=draft;}
    QString name=own?"YOUR CHIPS":person["name"].toString();QFont font("Noto Sans",side?7:8);font.setBold(true);
    int nameWidth=side?int(area.width()):80;name=QFontMetrics(font).elidedText(name,Qt::ElideRight,nameWidth);
    QRectF heading=side?QRectF(area.x(),area.y(),area.width(),32):QRectF(area.x()+4,area.y()+7,80,34);
    label(p,heading,name+"\n"+formatUnits(amount),side?7:8,QColor(themes()[tableTheme].accent),true);
    paintInventory(p,side?area.adjusted(0,34,0,0):area.adjusted(86,0,-12,0),bank,values,amount-int(amount));
}
void Table::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#101b20"));
    scale=std::min(width()/1280.0,height()/820.0);ox=(width()-1280*scale)/2;oy=(height()-820*scale)/2;p.translate(ox,oy);p.scale(scale,scale);hitRects.clear();handRects.clear();cardRects.clear();
    const auto &t=themes()[tableTheme];
    QRadialGradient bg(640,320,800);bg.setColorAt(0,QColor("#23383c"));bg.setColorAt(1,QColor("#101b20"));p.fillRect(QRectF(0,0,1280,820),bg);
    p.setPen(QPen(QColor("#080f12"),22));p.setBrush(QColor("#172426"));p.drawRoundedRect(QRectF(45,88,1190,660),230,230);
    p.setPen(QPen(QColor(t.accent),1.7));
    QRadialGradient felt(640,320,640);felt.setColorAt(0,QColor(t.light));felt.setColorAt(1,QColor(t.felt));p.setBrush(felt);p.drawRoundedRect(QRectF(60,102,1160,632),220,220);
    QPainterPath clipping;clipping.addRoundedRect(QRectF(67,109,1146,618),214,214);p.save();p.setClipPath(clipping);
    p.setPen(QPen(QColor(255,255,255,9),1));
    int style=tableTheme%5;
    for(int x=0;x<1300;x+=style==4?13:24) {
        if(style==0||style==3)p.drawLine(QPointF(x,100),QPointF(x-250,740));
        else if(style==1)p.drawLine(x,100,x,740);
        else if(style==4){QPainterPath path;path.moveTo(x,100);path.cubicTo(x+40,260,x-40,400,x,740);p.drawPath(path);}
        else for(int y=100;y<740;y+=24)p.drawEllipse(QPointF(x,y),1.3,1.3);
    }
    p.restore();p.setPen(QPen(QColor(255,255,255,23),1));p.setBrush(Qt::NoBrush);p.drawRoundedRect(QRectF(82,124,1116,588),200,200);
    label(p,QRectF(400,20,480,26),"V E L V E T   C A S I N O",13,QColor(t.accent),true);
    label(p,QRectF(400,50,480,25),"A seat for every kind of player",10,QColor("#93aba8"));
    auto people=data["players"].toArray();bool bj=data["kind"].toInt()==0;
    if(bj) {
        label(p,QRectF(414,108,340,25),"HOUSE DEALER",10,QColor(t.accent),true);
        auto cards=data["dealer"].toArray();qreal step=std::min(58.0,148.0/std::max(1,int(cards.size())-1)),span=52+std::max(0,int(cards.size())-1)*step;
        for(int i=0;i<cards.size();++i){QRectF r(584-span/2+i*step,143,52,74);cardRects.append(r);paintCard(p,r,cards[i].toObject(),faceTheme,backTheme);}
        paintShoe(p,data["cardsRemaining"].toInt(),std::clamp(data["deckCount"].toInt(8),1,8),t);
        label(p,QRectF(450,224,380,22),data["phase"]=="finished"&&!cards.isEmpty()?QString("Dealer total  %1").arg(data["dealerValue"].toInt()):"BLACKJACK PAYS 3 : 2",10,QColor("#c4d2bf"));
        label(p,QRectF(420,330,440,40),"Velvet Casino",28,QColor(t.accent),true);
        label(p,QRectF(430,376,420,22),"DEALER STANDS ON ALL 17s",9,QColor("#c4d2bf"));
        if(data["pot"].toInt()&&!data.contains("betPrompt"))label(p,QRectF(430,400,420,20),QString("TOTAL WAGERED  %1").arg(data["pot"].toInt()),9,QColor(t.accent));
    } else {
        // The house dealer stays at the top. The poker button moves between
        // player seats; it never moves the local player away from the bottom.
        for(int layer=2;layer>=0;--layer){QRectF deck(500+layer*3,119+layer*2,36,50);cardRects.append(deck);paintCard(p,deck,{{"hidden",true}},faceTheme,backTheme);}
        label(p,QRectF(548,118,260,25),"HOUSE DEALER",11,QColor(t.accent),true);
        label(p,QRectF(548,144,260,22),data["game"].toString().toUpper(),10,QColor("#bfcec7"));
        label(p,QRectF(430,173,420,20),data["jacks"].toBool()?"JACKS OR BETTER TO OPEN":"NO LIMIT  ·  PLAY CHIPS",9,QColor("#bfcec7"));
        auto board=data["board"].toArray();bool holdem=data["kind"].toInt()==int(Kind::Holdem);
        if(holdem)for(int i=0;i<5;++i) {
            QRectF r(478+i*67,284,59,83);
            if(i<board.size()){cardRects.append(r);paintCard(p,r,board[i].toObject(),faceTheme,backTheme);}
            else{p.setPen(QPen(QColor(255,255,255,35),1,Qt::DashLine));p.setBrush(QColor(0,0,0,15));p.drawRoundedRect(r,6,6);}
        }
        label(p,QRectF(420,holdem?238:292,440,40),"Velvet Casino",25,QColor(t.accent),true);
        // Draw/stud chips are centered on the felt at (640, 418). Hold'em
        // keeps the pot immediately below the complete community-card row.
        label(p,QRectF(450,holdem?377:366,380,30),QString("POT  %1").arg(QLocale().toString(data["pot"].toInt())),18,QColor(t.accent),true);
        p.save();p.translate(516.4,holdem?413:401.8);p.scale(1.2,1.2);paintStack(p,QPointF(0,0),data["pot"].toInt(),readChipValues(data["chipValues"].toArray()),data["potChips"].toArray());p.restore();
    }
    int viewer=data["viewer"].toInt(-1);int offset=viewer>=0?viewer:0;
    // Keep every card upright. Bankroll chips follow the nearest rail:
    // left of left seats, right of right seats, and below the local player.
    const std::array<QPointF,5> positions{{{488,474},{164,442},{164,168},{812,168},{812,442}}};
    const std::array<QRectF,5> banks{{{488,682,304,50},{92,450,64,164},{92,206,64,164},{1124,206,64,164},{1124,450,64,164}}};
    for(int n=0;n<people.size();++n){
        int i=(n+offset)%people.size(),pos=people.size()==2?(n==0?0:2):people.size()==3?(n==0?0:n==1?2:3):n;
        paintSeat(p,i,positions[pos]);paintBankroll(p,i,banks[pos],pos!=0);
    }
    if(data.contains("betPrompt")){QRectF prompt=bj?QRectF(415,254,450,74):QRectF(415,198,450,data["kind"].toInt()==int(Kind::Holdem)?40:75);label(p,prompt,data["betPrompt"].toString(),10,QColor("#e0e6d4"),true,Qt::AlignCenter|Qt::TextWordWrap);}
    else label(p,QRectF(430,bj?278:data["kind"].toInt()==int(Kind::Holdem)?198:336,420,28),data["street"].toString(),11,QColor("#e0e6d4"),true);
    label(p,QRectF(15,790,1250,24),"20 TABLE FINISHES   /   20 CARD FACES   /   20 CARD BACKS",8,QColor("#7f9998"));
}
}
