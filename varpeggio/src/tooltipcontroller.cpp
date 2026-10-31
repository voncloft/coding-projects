#include "tooltipcontroller.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QSettings>
#include <QToolTip>

TooltipController::TooltipController(QObject *parent)
    : QObject(parent), m_disableAction(new QAction(tr("Disable tooltips"), this)) {
    m_disableAction->setObjectName("disableTooltipsAction");
    m_disableAction->setCheckable(true);
    m_disableAction->setChecked(
        QSettings("Voncloft", "VArpeggio").value("ui/tooltipsDisabled", false).toBool());
    connect(m_disableAction, &QAction::toggled, this, [](bool disabled) {
        QSettings("Voncloft", "VArpeggio").setValue("ui/tooltipsDisabled", disabled);
        if (disabled)
            QToolTip::hideText();
    });
    if (m_disableAction->isChecked())
        QToolTip::hideText();
    // Filter tooltip requests rather than deleting their text, so newly
    // created dialogs are covered and re-enabling restores every hint.
    qApp->installEventFilter(this);
}

bool TooltipController::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ToolTip && m_disableAction->isChecked()) {
        event->ignore();
        return true;
    }
    return QObject::eventFilter(watched, event);
}
