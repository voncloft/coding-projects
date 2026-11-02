// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Velvet Casino contributors
#include "network.h"
namespace casino {
static constexpr int ProtocolVersion=10;
Network::Network(Game *g,QObject *parent):QObject(parent),game(g),server(this) {
    connect(&server,&QTcpServer::newConnection,this,&Network::accept);
    connect(&client,&QTcpSocket::readyRead,this,&Network::readClient);
    connect(&client,&QTcpSocket::connected,this,[this]{ send(&client,{{"type","join"},{"protocol",ProtocolVersion},{"code",targetCode},{"name",targetName},{"balance",targetBalance},{"token",token}}); });
    connect(&client,&QTcpSocket::disconnected,this,[this]{ if(clientMode) { latest["connected"]=false; emit changed(); reconnect.start(); } });
    connect(&client,&QTcpSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){ if(clientMode) { emit notice(client.errorString()); if(!reconnect.isActive()) reconnect.start(); } });
    reconnect.setInterval(2000);
    connect(&reconnect,&QTimer::timeout,this,[this]{ if(clientMode && client.state()==QAbstractSocket::UnconnectedState) { clientBuffer.clear(); client.connectToHost(targetAddress,targetPort); } });
}
Network::~Network(){disconnectSession();}
void Network::disconnectSession() {
    clientMode=false; reconnect.stop(); client.abort(); server.close();
    auto sockets=peers.keys(); peers.clear();
    for(auto *socket:sockets) { socket->disconnect(this); socket->abort(); socket->deleteLater(); }
    tokens.clear(); latest={}; clientBuffer.clear(); token.clear(); code.clear(); exchangeRequest.clear(); paused=false;inputBlocked=false;
}
bool Network::host(quint16 p) {
    disconnectSession();
    if(!server.listen(QHostAddress::AnyIPv4,p)) { emit notice(server.errorString()); return false; }
    code=QString::number(QRandomGenerator::system()->generate64(),16).rightJustified(16,'0').left(8).toUpper();
    emit changed(); return true;
}
void Network::join(const QString &address,quint16 p,const QString &room,const QString &name,double balance) {
    disconnectSession(); clientMode=true; targetAddress=address; targetPort=p; targetCode=room.trimmed().toUpper(); targetName=name.left(24);
    targetBalance=balance;client.connectToHost(targetAddress,targetPort); emit changed();
}
void Network::send(QTcpSocket *socket,const QJsonObject &packet) {
    if(socket->state()==QAbstractSocket::ConnectedState) socket->write(QJsonDocument(packet).toJson(QJsonDocument::Compact)+'\n');
}
void Network::accept() {
    while(server.hasPendingConnections()) {
        auto *socket=server.nextPendingConnection();
        if(peers.size()>=12) { socket->disconnectFromHost(); socket->deleteLater(); continue; }
        peers.insert(socket,{});
        connect(socket,&QTcpSocket::readyRead,this,[this,socket]{readPeer(socket);});
        connect(socket,&QTcpSocket::disconnected,this,[this,socket]{ peers.remove(socket); socket->deleteLater(); publish(); emit changed(); });
        QTimer::singleShot(8000,socket,[this,socket]{ if(peers.contains(socket) && peers[socket].seat<0) socket->disconnectFromHost(); });
    }
}
void Network::readPeer(QTcpSocket *socket) {
    if(!peers.contains(socket)) return;
    peers[socket].buffer+=socket->readAll();
    if(peers[socket].buffer.size()>65536) {socket->abort();return;}
    while(peers.contains(socket) && peers[socket].buffer.contains('\n')) {
        int end=peers[socket].buffer.indexOf('\n'); QByteArray line=peers[socket].buffer.left(end); peers[socket].buffer.remove(0,end+1);
        QJsonParseError err; auto doc=QJsonDocument::fromJson(line,&err);
        if(err.error!=QJsonParseError::NoError || !doc.isObject()) {socket->abort();return;}
        handlePeer(socket,doc.object());
    }
}
void Network::handlePeer(QTcpSocket *socket,const QJsonObject &p) {
    auto &peer=peers[socket];
    if(peer.seat<0) {
        if(p["type"]!="join" || p["protocol"].toInt()!=ProtocolVersion || p["code"].toString()!=code) {send(socket,{{"type","error"},{"message","Room code or app version does not match."}});socket->disconnectFromHost();return;}
        int assigned=-1;
        for(int i=0;i<int(game->config().players.size());++i) {
            if(game->config().players[i].control!=Control::Remote) continue;
            bool occupied=false; for(auto it=peers.cbegin();it!=peers.cend();++it) if(it.value().seat==i) occupied=true;
            if(occupied) continue;
            if(tokens.contains(i)) {if(p["token"].toString()==tokens[i]) {assigned=i;break;}}
            else if(p["token"].toString().isEmpty() && assigned<0) assigned=i;
        }
        if(assigned<0) {send(socket,{{"type","error"},{"message","No open network seat. A disconnected seat is reserved for its original player."}});socket->disconnectFromHost();return;}
        peer.seat=assigned;
        if(!tokens.contains(assigned)&&p["balance"].isDouble())game->setStartingBalance(assigned,p["balance"].toDouble(-1));
        game->setPlayerName(assigned,p["name"].toString());
        if(!tokens.contains(assigned)) tokens[assigned]=QUuid::createUuid().toString(QUuid::WithoutBraces);
        send(socket,{{"type","welcome"},{"seat",assigned},{"token",tokens[assigned]}});
        publish(); emit changed(); return;
    }
    if(p["type"]=="profile"){
        std::optional<double> balance;
        bool valid=p["name"].isString();
        if(p.contains("balance")){
            valid=valid&&p["balance"].isDouble()&&!inputBlocked&&p["hand"].toInt(-1)==game->handNumber();
            balance=p["balance"].toDouble(-1);
        }
        if(!valid||!game->updateProfile(peer.seat,p["name"].toString(),balance)){
            send(socket,{{"type","profileResult"},{"accepted",false},{"message","Profile not changed. Bankroll edits require a completed hand and a valid half-unit amount. Reopen your profile to try again."}});return;
        }
        const auto &seat=game->players()[peer.seat];
        send(socket,{{"type","profileResult"},{"accepted",true},{"name",seat.config.name},{"balance",seat.chips+seat.halfUnit*0.5+seat.committed}});
        publish();emit changed();return;
    }
    if(p["type"]=="exchange"){
        std::array<int,5> counts{};auto list=p["counts"].toArray();auto state=game->snapshot(peer.seat);auto seat=state["players"].toArray()[peer.seat].toObject();
        bool valid=list.size()==5;
        for(int i=0;valid&&i<5;++i){valid=list[i].isDouble()&&list[i].toDouble()==list[i].toInt(-1)&&list[i].toInt(-1)>=0;if(valid)counts[i]=list[i].toInt();}
        valid=valid&&ready()&&!inputBlocked&&p["hand"].toInt(-1)==game->handNumber()&&p["soundSession"]==state["soundSession"]&&p["chipValues"]==state["chipValues"]&&p["bankChips"]==seat["bankChips"]&&p["bankChange"]==seat["bankChange"]&&p["balance"]==seat["chips"];
        bool accepted=valid&&game->exchangeChips(peer.seat,counts);
        if(accepted){publish();emit changed();}
        send(socket,{{"type","exchangeResult"},{"request",p["request"]},{"accepted",accepted},{"message",accepted?"Chips exchanged.":"Exchange not completed. Your table or chips changed, or the selected chips cost more than your balance. Close and reopen Exchange chips."}});return;
    }
    if(inputBlocked)return;
    if(p["type"]=="ante"&&ready()&&p["hand"].toInt(-1)==game->handNumber()&&p["ante"].toInt(-1)==game->anteValue()){
        if(game->postAnte(peer.seat)){publish();emit changed();}return;
    }
    if(p["type"]=="cashout" && ready()){game->requestCashout();publish();emit changed();return;}
    if(p["type"]=="action" && ready() && p["hand"].toInt()==game->handNumber()) {
        std::vector<int> discard;
        for(auto v:p["discards"].toArray()) {if(!v.isDouble() || v.toInt(-1)<0 || v.toInt()>4) return;discard.push_back(v.toInt());}
        if(p.contains("raiseTo")&&(!p["raiseTo"].isDouble()||p["raiseTo"].toDouble()!=p["raiseTo"].toInt(-1)))return;
        std::array<int,5> chosen{};
        if(p.contains("counts")){
            auto counts=p["counts"].toArray();if(counts.size()!=5||p["chipValues"].toArray()!=chipValuesJson(game->config().chipValues))return;
            for(int i=0;i<5;++i){if(!counts[i].isDouble()||counts[i].toDouble()!=counts[i].toInt(-1))return;chosen[i]=counts[i].toInt();}
        }
        if(game->act(peer.seat,p["action"].toString(),discard,p["raiseTo"].toInt(),chosen)) {publish();emit changed();}
    }
    if((p["type"]=="wager"||p["type"]=="clearWager") && ready() && p["hand"].toInt(-1)==game->handNumber()) {
        if(p["type"]=="clearWager"){if(game->clearWager(peer.seat)){publish();emit changed();}return;}
        if(p["chipValues"].toArray()!=chipValuesJson(game->config().chipValues))return;
        if(p.contains("hands")){
            const auto list=p["hands"].toArray();if(!p["hands"].isArray()||list.isEmpty()||list.size()>4)return;
            HandWagers hands;
            for(const auto &item:list){
                if(!item.isArray()||item.toArray().size()!=5)return;
                std::array<int,5> counts{};auto array=item.toArray();
                for(int i=0;i<5;++i){if(!array[i].isDouble()||array[i].toDouble()!=array[i].toInt(-1))return;counts[i]=array[i].toInt();}
                hands.push_back(counts);
            }
            if(game->placeHandWagers(peer.seat,hands)){publish();emit changed();}return;
        }
        auto array=p["counts"].toArray();if(array.size()!=5)return;std::array<int,5> counts{};
        for(int i=0;i<5;++i){if(!array[i].isDouble()||array[i].toDouble()!=array[i].toInt(-1))return;counts[i]=array[i].toInt();}
        int handCount=p["handCount"].toInt(-1);
        if(!p["handCount"].isDouble()||p["handCount"].toDouble()!=handCount)return;
        if(game->placeWager(peer.seat,counts,handCount)){publish();emit changed();}
    }
}
void Network::readClient() {
    clientBuffer+=client.readAll(); if(clientBuffer.size()>262144) {client.abort();return;}
    while(clientBuffer.contains('\n')) {
        int end=clientBuffer.indexOf('\n'); auto doc=QJsonDocument::fromJson(clientBuffer.left(end)); clientBuffer.remove(0,end+1); if(!doc.isObject()) continue;
        auto p=doc.object();
        if(p["type"]=="welcome") { token=p["token"].toString(); reconnect.stop(); }
        if(p["type"]=="state") {latest=p["state"].toObject();latest["connected"]=true;emit changed();}
        if(p["type"]=="profileResult"){
            if(p["accepted"].toBool()){
                targetName=p["name"].toString();targetBalance=p["balance"].toDouble();
                emit profileSaved(targetName,targetBalance);
            }else emit notice(p["message"].toString());
        }
        if(p["type"]=="exchangeResult"&&!exchangeRequest.isEmpty()&&p["request"].toString()==exchangeRequest){exchangeRequest.clear();emit chipsExchanged(p["accepted"].toBool(),p["message"].toString());}
        if(p["type"]=="error") {clientMode=false;reconnect.stop();client.abort();latest={};emit notice(p["message"].toString());emit changed();}
    }
}
bool Network::ready() const {
    if(clientMode) return latest["connected"].toBool() && latest["networkReady"].toBool();
    if(paused) return false;
    for(int i=0;i<int(game->config().players.size());++i) if(game->config().players[i].control==Control::Remote) {
        bool connected=false; for(auto it=peers.cbegin();it!=peers.cend();++it) if(it.value().seat==i) connected=true;
        if(!connected) return false;
    }
    return true;
}
QString Network::description() const {
    if(clientMode) return client.state()==QAbstractSocket::ConnectedState ? (ready()?"Connected to table":"Waiting for players") : "Reconnecting to host…";
    if(!isHosting()) return "Local table";
    QStringList addresses;
    for(auto a:QNetworkInterface::allAddresses()) if(a.protocol()==QAbstractSocket::IPv4Protocol && !a.isLoopback()) addresses<<a.toString();
    return QString("Host %1  ·  Port %2  ·  Room %3%4").arg(addresses.join(" / ")).arg(port()).arg(code).arg(ready()?"":"  ·  Waiting for network players");
}
void Network::publish() {
    for(auto it=peers.cbegin();it!=peers.cend();++it) if(it.value().seat>=0) {
        auto state=game->snapshot(it.value().seat); state["networkReady"]=ready();state["inputBlocked"]=inputBlocked;
        send(it.key(),{{"type","state"},{"state",state}});
    }
}
void Network::sendAction(const QString &action,const std::vector<int> &discards,int raiseTo,const std::array<int,5> &chosen) {
    if(!clientMode || !ready()) return;
    QJsonArray indices; for(int i:discards) indices.append(i);
    send(&client,{{"type","action"},{"action",action},{"discards",indices},{"raiseTo",raiseTo},{"counts",chipValuesJson(chosen)},{"chipValues",latest["chipValues"]},{"hand",latest["hand"]}});
}
void Network::sendCashout() {if(clientMode&&ready())send(&client,{{"type","cashout"}});}
bool Network::sendExchange(const std::array<int,5> &counts,const QJsonObject &basis) {
    if(!clientMode||!ready()||latest["inputBlocked"].toBool())return false;
    int me=basis["viewer"].toInt(-1);auto people=basis["players"].toArray();if(me<0||me>=people.size())return false;auto seat=people[me].toObject();
    exchangeRequest=QUuid::createUuid().toString(QUuid::WithoutBraces);
    send(&client,{{"type","exchange"},{"request",exchangeRequest},{"counts",chipValuesJson(counts)},{"hand",basis["hand"]},{"soundSession",basis["soundSession"]},{"chipValues",basis["chipValues"]},{"bankChips",seat["bankChips"]},{"bankChange",seat["bankChange"]},{"balance",seat["chips"]}});return true;
}
bool Network::sendProfile(const QString &name,std::optional<double> balance) {
    if(!clientMode||client.state()!=QAbstractSocket::ConnectedState||token.isEmpty()||!latest["connected"].toBool())return false;
    QJsonObject packet{{"type","profile"},{"name",name},{"hand",latest["hand"]}};
    if(balance)packet["balance"]=*balance;
    send(&client,packet);return true;
}
void Network::sendWager(const std::array<int,5> &counts,bool clear,int handCount) {
    if(!clientMode||!ready())return;
    send(&client,{{"type",clear?"clearWager":"wager"},{"counts",chipValuesJson(counts)},{"handCount",handCount},{"chipValues",latest["chipValues"]},{"hand",latest["hand"]}});
}
void Network::sendHandWagers(const HandWagers &hands) {
    if(!clientMode||!ready())return;
    send(&client,{{"type","wager"},{"hands",handWagersJson(hands)},{"chipValues",latest["chipValues"]},{"hand",latest["hand"]}});
}
void Network::sendAnte() {
    if(clientMode&&ready())send(&client,{{"type","ante"},{"hand",latest["hand"]},{"ante",latest["ante"]}});
}
}
