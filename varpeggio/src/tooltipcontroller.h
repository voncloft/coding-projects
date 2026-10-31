#pragma once

#include <QObject>

class QAction;

class TooltipController final : public QObject {
public:
    explicit TooltipController(QObject *parent = nullptr);
    QAction *disableAction() const { return m_disableAction; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QAction *m_disableAction;
};
