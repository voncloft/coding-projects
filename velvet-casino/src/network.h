// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Velvet Casino contributors
#pragma once
#include "game.h"
#include <QtNetwork>

namespace casino {
class Network : public QObject {
    Q_OBJECT
public:
    explicit Network(Game *game, QObject *parent = nullptr);
    ~Network() override;
    bool host(quint16 port);
    void join(const QString &address, quint16 port, const QString &code, const QString &name, double balance=-1);
    void disconnectSession();
    void publish();
    void sendAction(const QString &action, const std::vector<int> &discards = {}, int raiseTo=0,const std::array<int,5> &chosen={});
    void sendWager(const std::array<int,5> &counts, bool clear=false, int handCount=1);
    void sendHandWagers(const HandWagers &hands);
    void sendAnte();
    void sendCashout();
    bool sendExchange(const std::array<int,5> &counts, const QJsonObject &basis);
    bool sendProfile(const QString &name, std::optional<double> balance = std::nullopt);
    bool isClient() const { return clientMode; }
    bool isHosting() const { return server.isListening(); }
    bool ready() const;
    QString description() const;
    QString roomCode() const { return code; }
    quint16 port() const { return server.serverPort(); }
    QJsonObject remoteState() const { return latest; }
    void setInputBlocked(bool value){if(inputBlocked!=value){inputBlocked=value;publish();}}
    void setPaused(bool value) { paused=value;publish();emit changed(); }
signals:
    void changed();
    void notice(const QString &message);
    void profileSaved(const QString &name, double balance);
    void chipsExchanged(bool accepted, const QString &message);
private:
    struct Peer { QByteArray buffer; int seat=-1; };
    Game *game;
    QTcpServer server;
    QTcpSocket client;
    QHash<QTcpSocket *, Peer> peers;
    QHash<int, QString> tokens;
    QString code, token, targetAddress, targetName, targetCode, exchangeRequest;
    quint16 targetPort=0;
    double targetBalance=-1;
    QByteArray clientBuffer;
    bool clientMode=false;
    bool paused=false,inputBlocked=false;
    QJsonObject latest;
    QTimer reconnect;
    void accept();
    void readPeer(QTcpSocket *socket);
    void readClient();
    void handlePeer(QTcpSocket *socket, const QJsonObject &packet);
    static void send(QTcpSocket *socket, const QJsonObject &packet);
};
}
