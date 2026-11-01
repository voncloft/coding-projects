#pragma once
#include "game.h"
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QElapsedTimer>
#include <QPointer>

namespace Golf {
class NetworkSession : public QObject {
    Q_OBJECT
public:
    enum class Mode { Offline, Host, Client };
    explicit NetworkSession(Game &game,QObject *parent=nullptr);
    ~NetworkSession() override;
    Mode mode() const { return m_mode; }
    bool online() const { return m_mode!=Mode::Offline; }
    int localSeat() const { return m_mode==Mode::Client?m_seat:0; }
    bool ready() const;
    bool pending() const { return m_pending; }
    bool isComputer(int seat) const;
    bool canAct() const;
    bool act(const QString &action,int argument=-1);
    void computerStep();
    void offline(int opponents,int rounds,const Rules &rules = {});
    bool host(const QVector<bool> &onlineOpponents,int rounds,const QString &name,quint16 port=45839,const Rules &rules = {});
    void join(const QString &address,quint16 port,const QString &code,const QString &name);
    void reconnect();
    void stop();
    quint16 port() const { return m_server.serverPort(); }
    QString roomCode() const { return m_code; }
    QString status() const;
    QString addresses() const;
    QString errorString() const { return m_error; }
signals:
    void updated();
    void error(const QString &message);
private:
    friend class NetworkTest;
    struct Peer { QByteArray buffer; int seat=-1; QElapsedTimer rate; int messages=0; };
    Game &g;
    Mode m_mode=Mode::Offline;
    QTcpServer m_server;
    QHash<QTcpSocket *,Peer> m_peers;
    QPointer<QTcpSocket> m_client;
    QByteArray m_buffer;
    QVector<bool> m_remote,m_connected;
    QStringList m_tokens;
    QString m_code,m_address,m_name,m_token,m_error;
    quint16 m_joinPort=0;
    int m_seat=-1,m_revision=0;
    bool m_started=false,m_ready=false,m_pending=false,m_stopping=false,m_welcomed=false;
    void acceptConnections();
    void readPeer(QTcpSocket *socket);
    void receive(QTcpSocket *socket,const QJsonObject &message);
    void readClient();
    void receiveClient(const QJsonObject &message);
    void send(QTcpSocket *socket,const QJsonObject &message);
    void reject(QTcpSocket *socket,const QString &reason);
    bool apply(int seat,const QString &action,int argument);
    void publish();
    static QString cleanName(const QString &name);
};
}
