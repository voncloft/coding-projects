// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Velvet Casino contributors
#include "window.h"
int main(int argc,char **argv) {
    QApplication app(argc,argv);QCoreApplication::setOrganizationName("Voncloft");QCoreApplication::setApplicationName("VelvetCasino");QCoreApplication::setApplicationVersion(CASINO_VERSION);app.setWindowIcon(QIcon(":/velvet-casino.svg"));
    QCommandLineParser parser;parser.setApplicationDescription("Velvet Casino — blackjack and poker for your desktop");parser.addHelpOption();parser.addVersionOption();parser.addOption({"no-setup","Open the table without the startup dialog."});parser.process(app);
    casino::Window window(!parser.isSet("no-setup"));window.show();return app.exec();
}
