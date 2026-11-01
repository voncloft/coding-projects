#pragma once
#include <QAbstractButton>
#include "game.h"
class CardButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit CardButton(QWidget *parent=nullptr);
    void setCard(int value, bool paired=false, const QString &badge={});
    void setAppearance(int face, int back);
    int faceTheme() const { return faceStyle; }
    int backTheme() const { return backStyle; }
    static QStringList themeNames();
    void setOrientation(int degrees) { rotation=degrees;update(); }
    int orientation() const { return rotation; }
    QSize sizeHint() const override { return {88,122}; }
protected:
    void paintEvent(QPaintEvent *) override;
private:
    int number = Golf::Hidden;
    int rotation = 0;
    int faceStyle = 0, backStyle = 0;
    bool match = false;
    QString badge;
};
