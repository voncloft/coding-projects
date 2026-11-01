#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    app.setApplicationName("TurnoverGolf"); app.setApplicationDisplayName("Turnover Golf"); app.setOrganizationName("Voncloft"); app.setApplicationVersion(GOLF_VERSION); app.setDesktopFileName("io.voncloft.TurnoverGolf");
    QCommandLineParser parser; parser.setApplicationDescription("Card golf for KDE with selectable layouts, matching rules and decks."); parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"screenshot","Render a clean table to a PNG and exit without saving.","path"}); parser.process(app);
    Window window(!parser.isSet("screenshot")); window.show();
    if(parser.isSet("screenshot")) QTimer::singleShot(250,&app,[&]{window.capture(parser.value("screenshot"));app.quit();});
    return app.exec();
}
