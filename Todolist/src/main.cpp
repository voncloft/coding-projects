#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Todo List"));
    app.setOrganizationName(QStringLiteral("von"));
    app.setDesktopFileName(QStringLiteral("com.von.TodoList"));
    const QIcon appIcon(QStringLiteral(":/todolist-icon.svg"));
    app.setWindowIcon(appIcon.isNull() ? QIcon::fromTheme(QStringLiteral("com.von.TodoList")) : appIcon);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A simple Qt6 todo list window with dynamic checkboxes."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("target"),
                                 QStringLiteral("Optional file or directory to open or use as the default save location."));
    parser.process(app);

    MainWindow window;
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        window.loadTarget(args.constFirst());
    }

    window.show();
    return app.exec();
}
