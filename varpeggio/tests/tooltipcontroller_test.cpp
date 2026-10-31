#include "tooltipcontroller.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QEventLoop>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolTip>
#include <QWidget>
#include <cstdio>

static bool check(bool condition, const char *message) {
    if (!condition)
        std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

class TooltipProbe final : public QWidget {
public:
    using QWidget::QWidget;
    int tooltips = 0;
    int keys = 0;
    int presses = 0;
    int enters = 0;
    int leaves = 0;

protected:
    bool event(QEvent *event) override {
        switch (event->type()) {
        case QEvent::ToolTip: ++tooltips; break;
        case QEvent::KeyPress: ++keys; return true;
        case QEvent::MouseButtonPress: ++presses; return true;
        case QEvent::Enter: ++enters; return true;
        case QEvent::Leave: ++leaves; return true;
        default: break;
        }
        return QWidget::event(event);
    }
};

static void requestTooltip(QWidget &widget) {
    const QPoint position(5, 5);
    QHelpEvent event(QEvent::ToolTip, position, widget.mapToGlobal(position));
    QApplication::sendEvent(&widget, &event);
}

static void allowTooltipToHide() {
    QEventLoop loop;
    QTimer::singleShot(400, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!check(directory.isValid(), "isolated settings directory exists"))
        return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope,
                       directory.filePath("system"));
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, directory.path());
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope,
                       directory.filePath("system"));
    QSettings settings("Voncloft", "VArpeggio");
    if (!check(settings.fileName().startsWith(directory.path() + '/'),
               "tests cannot write real user preferences"))
        return 1;

    bool ok = true;
    TooltipProbe widget;
    widget.setToolTip("Original piano control hint");
    widget.resize(160, 60);
    widget.show();
    app.processEvents();
    {
        TooltipController controller;
        auto *action = controller.disableAction();
        ok &= check(action->isCheckable() && !action->isChecked(),
                    "Disable tooltips is checkable and off by default");
        ok &= check(action->text() == "Disable tooltips", "menu text is clear");
        requestTooltip(widget);
        ok &= check(widget.tooltips == 1 && QToolTip::isVisible(),
                    "tooltips show normally before disabling");

        action->trigger();
        allowTooltipToHide();
        ok &= check(action->isChecked() && !QToolTip::isVisible(),
                    "disabling dismisses an already visible tooltip");
        requestTooltip(widget);
        ok &= check(widget.tooltips == 1 && !QToolTip::isVisible(),
                    "disabled tooltip events cannot reach existing widgets");
        ok &= check(settings.value("ui/tooltipsDisabled").toBool(),
                    "disabling is saved immediately");
        ok &= check(widget.toolTip() == "Original piano control hint",
                    "the original hint text is preserved");

        QDialog dialog;
        TooltipProbe laterWidget(&dialog);
        laterWidget.setToolTip("New dialog hint");
        requestTooltip(laterWidget);
        ok &= check(laterWidget.tooltips == 0,
                    "new dialog controls are also covered while disabled");
        laterWidget.setToolTip("Updated dialog hint");
        requestTooltip(laterWidget);
        ok &= check(laterWidget.tooltips == 0,
                    "dynamically changed tooltip text cannot bypass disabling");

        const int beforeEnters = widget.enters;
        const int beforeLeaves = widget.leaves;
        QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5),
                          QPointF(widget.mapToGlobal(QPoint(5, 5))),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QEvent enter(QEvent::Enter), leave(QEvent::Leave);
        QApplication::sendEvent(&widget, &key);
        QApplication::sendEvent(&widget, &press);
        QApplication::sendEvent(&widget, &enter);
        QApplication::sendEvent(&widget, &leave);
        ok &= check(widget.keys == 1 && widget.presses == 1 &&
                    widget.enters == beforeEnters + 1 && widget.leaves == beforeLeaves + 1,
                    "keyboard, mouse, and sidebar hover events remain unaffected");

        action->trigger();
        requestTooltip(laterWidget);
        ok &= check(!action->isChecked() && laterWidget.tooltips == 1 &&
                    laterWidget.toolTip() == "Updated dialog hint",
                    "unchecking restores current tooltip text without rebuilding controls");
        ok &= check(!settings.value("ui/tooltipsDisabled").toBool(),
                    "re-enabling is saved immediately");
        action->trigger();
    }
    {
        TooltipController restored;
        ok &= check(restored.disableAction()->isChecked(),
                    "a recreated controller restores the disabled preference");
        const int before = widget.tooltips;
        requestTooltip(widget);
        ok &= check(widget.tooltips == before,
                    "restored disabled preference blocks the first tooltip request");
        restored.disableAction()->trigger();
    }
    {
        TooltipController restored;
        ok &= check(!restored.disableAction()->isChecked(),
                    "a recreated controller also restores the enabled preference");
    }
    const int before = widget.tooltips;
    requestTooltip(widget);
    ok &= check(widget.tooltips == before + 1,
                "destroyed controllers leave no stale application filter");
    QToolTip::hideText();
    if (ok)
        std::puts("Tooltip preference checks passed.");
    return ok ? 0 : 1;
}
