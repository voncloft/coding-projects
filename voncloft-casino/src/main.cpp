// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 voncloft-casino contributors
#include "window.h"
#include "appsettings.h"
int main(int argc,char **argv) {
    QApplication app(argc,argv);QCoreApplication::setOrganizationName("Voncloft");QCoreApplication::setApplicationName("voncloft-casino");QCoreApplication::setApplicationVersion(CASINO_VERSION);QGuiApplication::setApplicationDisplayName("voncloft-casino");QGuiApplication::setDesktopFileName("io.voncloft.VelvetCasino");app.setWindowIcon(QIcon(":/voncloft-casino.svg"));
    QCommandLineParser parser;parser.setApplicationDescription("voncloft-casino — blackjack and poker for your desktop");parser.addHelpOption();parser.addVersionOption();parser.addOption({"no-setup","Open the table without the startup dialog."});parser.process(app);
    QSettings current,legacy("Voncloft","VelvetCasino");
    if(!casino::migrateLegacySettings(current,legacy))qWarning("Unable to save migrated casino settings.");
    casino::Window window(!parser.isSet("no-setup"));window.show();return app.exec();
}
