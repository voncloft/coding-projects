#include "network.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QTimer>
#include <QUuid>
#include <algorithm>

namespace Golf {
namespace { constexpr int FrameLimit=65536; constexpr int Protocol=1; }
NetworkSession::NetworkSession(Game &game,QObject *parent):QObject(parent),g(game) {
    m_server.setMaxPendingConnections(8);
    connect(&m_server,&QTcpServer::newConnection,this,&NetworkSession::acceptConnections);
}
NetworkSession::~NetworkSession(){ stop(); }
QString NetworkSession::cleanName(const QString &name) {
    QString result;
    for(QChar c:name.trimmed()) if(c.isLetterOrNumber() || c==' ' || c=='-' || c=='_' || c=='.')result+=c;
    return result.left(24).trimmed().isEmpty()?QString("Player"):result.left(24).trimmed();
}
bool NetworkSession::ready() const {
    if(m_mode==Mode::Offline)return true;
    if(m_mode==Mode::Client)return m_ready && m_client && m_client->state()==QAbstractSocket::ConnectedState;
    return !m_connected.isEmpty() && std::all_of(m_connected.begin(),m_connected.end(),[](bool x){return x;});
}
bool NetworkSession::isComputer(int seat) const {
    if(seat<1 || seat>=g.players())return false;
    return m_mode==Mode::Offline || (seat<m_remote.size() && !m_remote[seat]);
}
bool NetworkSession::canAct() const {return ready() && !m_pending && localSeat()==g.state().current;}
void NetworkSession::stop() {
    m_stopping=true;
    m_server.close();
    if(m_client){m_client->disconnect(this);m_client->abort();m_client->deleteLater();m_client=nullptr;}
    const auto sockets=m_peers.keys();
    for(auto *socket:sockets){socket->disconnect(this);socket->abort();socket->deleteLater();}
    m_peers.clear(); m_buffer.clear();m_remote.clear();m_connected.clear();m_tokens.clear();
    if(m_mode==Mode::Client)g.newMatch();
    m_mode=Mode::Offline;m_seat=-1;m_revision=0;m_ready=false;m_pending=false;m_started=false;
    m_stopping=false;
}
void NetworkSession::offline(int opponents,int rounds){stop();g.newMatch(opponents,rounds);emit updated();}
bool NetworkSession::host(const QVector<bool> &onlineOpponents,int rounds,const QString &name,quint16 port) {
    if(onlineOpponents.size()<1 || onlineOpponents.size()>4 || !onlineOpponents.contains(true)){m_error="Choose at least one online human seat.";return false;}
    stop();
    if(!m_server.listen(QHostAddress::Any,port)){m_error=m_server.errorString();emit updated();return false;}
    m_mode=Mode::Host;m_code=QUuid::createUuid().toString(QUuid::Id128).left(12).toUpper();m_error.clear();
    m_remote={false};m_connected={true};m_tokens={QString{}};
    QStringList names{cleanName(name)};
    for(int i=0;i<onlineOpponents.size();++i){
        m_remote.append(onlineOpponents[i]);m_connected.append(!onlineOpponents[i]);m_tokens.append(QString{});
        names.append(onlineOpponents[i]?QString("Online player %1").arg(i+1):QString("Computer %1").arg(i+1));
    }
    g.newMatch(onlineOpponents.size(),rounds);g.setNames(names);publish();return true;
}
void NetworkSession::join(const QString &address,quint16 port,const QString &code,const QString &name) {
    stop();m_mode=Mode::Client;m_address=address.trimmed();m_joinPort=port;m_code=code.trimmed().toUpper();m_name=cleanName(name);m_token.clear();m_error.clear();reconnect();
}
void NetworkSession::reconnect() {
    if(m_mode!=Mode::Client)return;
    if(m_client){m_client->disconnect(this);m_client->abort();m_client->deleteLater();}
    m_client=new QTcpSocket(this);m_client->setReadBufferSize(FrameLimit*2);m_buffer.clear();m_ready=false;m_pending=false;m_welcomed=false;m_error.clear();
    QPointer<QTcpSocket> socket=m_client;
    connect(socket,&QTcpSocket::connected,this,[this,socket]{
        send(socket,{{"type","hello"},{"protocol",Protocol},{"code",m_code},{"name",m_name},{"token",m_token}});
    });
    connect(socket,&QTcpSocket::readyRead,this,&NetworkSession::readClient);
    connect(socket,&QTcpSocket::disconnected,this,[this]{m_ready=false;m_pending=false;if(m_error.isEmpty())m_error="Disconnected from the host. Use Reconnect to rejoin.";emit updated();});
    connect(socket,&QTcpSocket::errorOccurred,this,[this,socket](QAbstractSocket::SocketError){if(socket){m_error=socket->errorString();m_ready=false;m_pending=false;emit error(m_error);emit updated();}});
    socket->connectToHost(m_address,m_joinPort);
    QTimer::singleShot(10000,this,[this,socket]{if(socket && socket==m_client && !m_welcomed){m_error="Connection timed out. Check the address, port and room code.";socket->abort();emit error(m_error);emit updated();}});
    emit updated();
}
void NetworkSession::send(QTcpSocket *socket,const QJsonObject &message) {
    if(!socket || socket->state()!=QAbstractSocket::ConnectedState)return;
    if(socket->bytesToWrite()>FrameLimit*4){socket->abort();return;}
    socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact)+'\n');
}
void NetworkSession::reject(QTcpSocket *socket,const QString &reason){send(socket,{{"type","error"},{"message",reason}});socket->disconnectFromHost();}
void NetworkSession::acceptConnections() {
    while(m_server.hasPendingConnections()) {
        auto *socket=m_server.nextPendingConnection();
        if(m_peers.size()>=8){socket->abort();socket->deleteLater();continue;}
        socket->setReadBufferSize(FrameLimit*2);Peer peer;peer.rate.start();m_peers.insert(socket,peer);
        connect(socket,&QTcpSocket::readyRead,this,[this,socket]{readPeer(socket);});
        connect(socket,&QTcpSocket::disconnected,this,[this,socket]{
            int seat=m_peers.value(socket).seat;m_peers.remove(socket);socket->deleteLater();
            if(!m_stopping && seat>=1 && seat<m_connected.size()){m_connected[seat]=false;publish();}
        });
        QTimer::singleShot(10000,socket,[this,socket]{if(m_peers.contains(socket) && m_peers[socket].seat<0)reject(socket,"Join handshake timed out.");});
    }
}
void NetworkSession::readPeer(QTcpSocket *socket) {
    if(!m_peers.contains(socket))return;
    m_peers[socket].buffer+=socket->readAll();
    while(m_peers.contains(socket)) {
        auto &peer=m_peers[socket];int newline=peer.buffer.indexOf('\n');
        if(newline<0){if(peer.buffer.size()>FrameLimit)reject(socket,"Message too large.");return;}
        if(newline>FrameLimit){reject(socket,"Message too large.");return;}
        QByteArray frame=peer.buffer.left(newline);peer.buffer.remove(0,newline+1);
        if(peer.rate.elapsed()>1000){peer.rate.restart();peer.messages=0;}
        if(++peer.messages>80){reject(socket,"Too many requests.");return;}
        QJsonParseError error;auto document=QJsonDocument::fromJson(frame,&error);
        if(error.error!=QJsonParseError::NoError || !document.isObject()){reject(socket,"Invalid message.");return;}
        receive(socket,document.object());
        if(socket->state()!=QAbstractSocket::ConnectedState)return;
    }
}
void NetworkSession::receive(QTcpSocket *socket,const QJsonObject &message) {
    if(!m_peers.contains(socket))return;
    int seat=m_peers[socket].seat;
    if(seat<0) {
        if(message["type"]!="hello" || message["protocol"].toInt()!=Protocol || message["code"].toString()!=m_code){reject(socket,"Wrong room code or incompatible game version.");return;}
        QString token=message["token"].toString();
        if(!token.isEmpty()) {
            for(int p=1;p<m_tokens.size();++p)if(m_remote[p] && m_tokens[p]==token && !m_connected[p])seat=p;
            if(seat<0){reject(socket,"That reconnect seat is unavailable.");return;}
        } else {
            if(m_started){reject(socket,"This match has started. Reconnect using your original game window.");return;}
            for(int p=1;p<m_remote.size();++p)if(m_remote[p] && !m_connected[p]){seat=p;break;}
            if(seat<0){reject(socket,"All online seats are occupied.");return;}
            m_tokens[seat]=QUuid::createUuid().toString(QUuid::Id128);
        }
        m_peers[socket].seat=seat;m_connected[seat]=true;
        auto names=g.state().names;names[seat]=cleanName(message["name"].toString());g.setNames(names);
        send(socket,{{"type","welcome"},{"protocol",Protocol},{"seat",seat},{"token",m_tokens[seat]}});
        publish();return;
    }
    if(message["type"]!="action" || !message["revision"].isDouble() || message["revision"].toDouble()!=m_revision || !apply(seat,message["action"].toString(),message["argument"].toInt(-1))) {
        send(socket,{{"type","error"},{"message","That move is not available. Check whose turn it is and try again."}});
        send(socket,{{"type","state"},{"revision",m_revision},{"ready",ready()},{"view",g.publicView(seat)},{"remote",[this]{QJsonArray a;for(bool b:m_remote)a.append(b);return a;}()}});
        return;
    }
    publish();
}
bool NetworkSession::apply(int seat,const QString &action,int argument) {
    if(action=="rounds")return seat==0 && g.setRounds(argument);
    if(!ready())return false;
    if(action=="next")return seat==0 && g.nextHole();
    if(seat!=g.state().current || (seat!=0 && isComputer(seat)))return false;
    if(action=="open")return g.open(argument);
    if(action=="draw")return g.draw(false);
    if(action=="take")return g.draw(true);
    if(action=="replace")return g.replace(argument);
    if(action=="discard")return g.discardHeld();
    if(action=="finish")return g.finishHole();
    return false;
}
bool NetworkSession::act(const QString &action,int argument) {
    if(m_mode==Mode::Client) {
        if(!canAct() || action=="next" || action=="rounds")return false;
        m_pending=true;send(m_client,{{"type","action"},{"revision",m_revision},{"action",action},{"argument",argument}});emit updated();return true;
    }
    if(!apply(0,action,argument))return false;
    publish();return true;
}
void NetworkSession::computerStep() {
    if(m_mode==Mode::Client || !ready() || !isComputer(g.state().current) || !g.active())return;
    g.computerStep();publish();
}
void NetworkSession::publish() {
    if(m_mode==Mode::Host) {
        ++m_revision;if(ready())m_started=true;
        QJsonArray remote;for(bool b:m_remote)remote.append(b);
        const auto sockets=m_peers.keys();
        for(auto *socket:sockets){int seat=m_peers.value(socket).seat;if(seat>=1)send(socket,{{"type","state"},{"revision",m_revision},{"ready",ready()},{"view",g.publicView(seat)},{"remote",remote}});}
    }
    emit updated();
}
void NetworkSession::readClient() {
    if(!m_client)return;
    m_buffer+=m_client->readAll();
    while(m_client) {
        int newline=m_buffer.indexOf('\n');
        if(newline<0){if(m_buffer.size()>FrameLimit){m_error="Host sent an oversized message.";m_client->abort();}return;}
        if(newline>FrameLimit){m_error="Host sent an oversized message.";m_client->abort();return;}
        auto frame=m_buffer.left(newline);m_buffer.remove(0,newline+1);
        QJsonParseError error;auto document=QJsonDocument::fromJson(frame,&error);
        if(error.error!=QJsonParseError::NoError || !document.isObject()){m_error="Host sent an invalid message.";m_client->abort();return;}
        receiveClient(document.object());
    }
}
void NetworkSession::receiveClient(const QJsonObject &message) {
    auto type=message["type"].toString();
    if(type=="welcome") {
        int seat=message["seat"].toInt(-1);auto token=message["token"].toString();
        if(seat<1 || seat>4 || token.size()!=32 || message["protocol"].toInt()!=Protocol){m_error="Invalid welcome from host.";m_client->abort();return;}
        m_seat=seat;m_token=token;m_welcomed=true;return;
    }
    if(type=="error"){m_error=message["message"].toString().left(300);m_pending=false;emit error(m_error);emit updated();return;}
    if(type!="state" || !m_welcomed)return;
    auto remote=message["remote"].toArray();int players=message["view"].toObject()["players"].toInt();
    bool validRoles=remote.size()==players;for(auto x:remote)validRoles=validRoles && x.isBool();
    if(!validRoles || !message["ready"].isBool() || !message["revision"].isDouble() || message["revision"].toDouble()!=message["revision"].toInt(-1) || message["revision"].toInt()<m_revision || !g.restorePublicView(message["view"].toObject(),m_seat)){m_error="Invalid game state from host.";m_client->abort();return;}
    m_revision=message["revision"].toInt();m_ready=message["ready"].toBool();m_pending=false;m_error.clear();m_remote.clear();for(auto x:remote)m_remote.append(x.toBool());emit updated();
}
QString NetworkSession::status() const {
    if(m_mode==Mode::Offline)return {};
    if(m_mode==Mode::Host){
        QStringList waiting;for(int p=1;p<m_connected.size();++p)if(!m_connected[p])waiting.append(g.name(p));
        return waiting.isEmpty()?QString("Hosting · All players connected · Port %1").arg(port()):QString("%1: %2").arg(m_started?"Paused — waiting for reconnection":"Waiting for online players").arg(waiting.join(", "));
    }
    if(!m_error.isEmpty())return m_error;
    if(!m_client || m_client->state()!=QAbstractSocket::ConnectedState)return "Connecting to host…";
    if(m_seat<1)return "Joining the room…";
    return ready()?QString("Online · Connected to %1:%2").arg(m_address).arg(m_joinPort):"Waiting for all online players to connect…";
}
QString NetworkSession::addresses() const {
    QStringList list;
    for(const auto &address:QNetworkInterface::allAddresses())if(address.protocol()==QAbstractSocket::IPv4Protocol && !address.isLoopback())list.append(address.toString());
    return list.isEmpty()?"127.0.0.1":list.join(", ");
}
}
