// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 voncloft-casino contributors
#include "sounds.h"
#include <QMediaDevices>
static void initializeChipResources(){Q_INIT_RESOURCE(sounds);}
namespace casino {
void ChipSounds::load(Clip &clip,const QString &path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return;
    const QByteArray wav=file.readAll();
    if(wav.size()<12||wav.first(4)!="RIFF"||wav.mid(8,4)!="WAVE")return;
    QByteArray pcm;QAudioFormat format;
    for(qsizetype offset=12;offset+8<=wav.size();) {
        const char *header=wav.constData()+offset;
        quint32 size=qFromLittleEndian<quint32>(header+4);offset+=8;
        if(size>quint64(wav.size()-offset))return;
        const char *data=wav.constData()+offset;
        if(QByteArrayView(header,4)=="fmt ") {
            // The embedded effects are uncompressed, 16-bit little-endian PCM.
            if(size<16||qFromLittleEndian<quint16>(data)!=1||qFromLittleEndian<quint16>(data+14)!=16)return;
            format.setChannelCount(qFromLittleEndian<quint16>(data+2));
            format.setSampleRate(qFromLittleEndian<quint32>(data+4));
            format.setSampleFormat(QAudioFormat::Int16);
        } else if(QByteArrayView(header,4)=="data")pcm=wav.mid(offset,size);
        offset+=qsizetype(size)+(size&1);
    }
    if(!format.isValid()||pcm.isEmpty()||pcm.size()%format.bytesPerFrame())return;
    clip.format=format;clip.buffer.setData(pcm);clip.buffer.open(QIODevice::ReadOnly);
}
ChipSounds::ChipSounds(QObject *parent):QObject(parent) {
    initializeChipResources();
    const QStringList files{"chip-bet","chip-payout","chip-allin","card-shuffle","deck-tap","card-deal","card-deal","card-deal","card-deal","chip-allin"};
    for(int i=0;i<int(clips.size());++i)load(clips[i],":/sounds/"+files[i]+".wav");
    load(intro,":/sounds/intro.wav");
    introTimer.setTimerType(Qt::PreciseTimer);
    connect(&introTimer,&QTimer::timeout,this,[this]{if(introWanted&&enabled)play(intro,"intro");});
    sequenceTimer.setSingleShot(true);sequenceTimer.setTimerType(Qt::PreciseTimer);
    connect(&sequenceTimer,&QTimer::timeout,this,&ChipSounds::playNext);
}
bool ChipSounds::samplesReady() const {return std::all_of(clips.begin(),clips.end(),[](const Clip &c){return c.format.isValid();});}
void ChipSounds::stop(Clip &clip){if(clip.output)clip.output->reset();clip.buffer.seek(0);}
void ChipSounds::stopPlayback(){sequenceTimer.stop();pending.clear();presenting=false;for(auto &clip:clips)stop(clip);}
void ChipSounds::setVolume(float value){volume=std::clamp(value,0.0f,1.0f);for(auto &clip:clips)if(clip.output)clip.output->setVolume(volume);if(intro.output)intro.output->setVolume(volume*0.5f);}
void ChipSounds::setAudioDevice(const QAudioDevice &device){stopPlayback();stopIntro();for(auto &clip:clips)clip.output.reset();intro.output.reset();selectedDevice=device;}
void ChipSounds::setEnabled(bool value){
    enabled=value;
    if(!enabled){
        for(auto &clip:clips)stop(clip);
        stop(intro);introTimer.stop();
        for(auto &event:pending)event["audible"]=false;
        if(!animateDeals)stopPlayback();
    }else if(introWanted)startIntro();
}
void ChipSounds::reset(){primed=false;session.clear();lastSequence=lastBet=lastPayout=0;stopPlayback();stopIntro();}
void ChipSounds::playBet(){if(enabled){emit cueQueued("bet");play(clips[0],"bet");}}
void ChipSounds::startIntro(){
    introWanted=true;
    if(enabled&&introReady()){play(intro,"intro");introTimer.start(intro.durationMs()+25);}
}
void ChipSounds::stopIntro(){introWanted=false;introTimer.stop();stop(intro);}
void ChipSounds::enqueue(QJsonObject event){
    static const QStringList audible{"bet","collect","payout","allin","shuffle","deck-tap","deal"};
    const QString name=event["cue"].toString();
    if(!audible.contains(name)&&name!="round"&&name!="split"&&name!="reveal")return;
    event["audible"]=enabled;pending.enqueue(event);
    if(animateDeals&&(name=="round"||name=="split"||name=="reveal"||name=="deal"))presenting=true;
    if(enabled&&audible.contains(name))emit cueQueued(name);
    if(name=="shuffle"){
        pending.enqueue(QJsonObject{{"cue","deck-tap"},{"audible",enabled}});
        if(enabled)emit cueQueued("deck-tap");
    }
}
void ChipSounds::playNext(){
    if(dispatching)return;
    QScopedValueRollback<bool> guard(dispatching,true);
    while(!pending.isEmpty()){
        const auto event=pending.dequeue();const QString name=event["cue"].toString();
        if(animateDeals)emit eventStarted(event);
        if(name=="round"||name=="split"||name=="reveal")continue;
        int index=name=="collect"?9:name=="payout"?1:name=="allin"?2:name=="shuffle"?3:name=="deck-tap"?4:0;
        if(name=="deal")index=5+(dealVoice++%4);
        auto &clip=clips[index];const bool audible=enabled&&event["audible"].toBool();
        if(audible)play(clip,name);
        int delay=audible?clip.durationMs()+25:0;
        if(name=="deal"&&animateDeals)delay=!pending.isEmpty()&&pending.head()["cue"]=="deal"?380:std::max(380,delay);
        else if(name=="deal"&&!pending.isEmpty()&&pending.head()["cue"]=="deal")delay=std::max(180,(clip.durationMs()+3)/4);
        if(delay){sequenceTimer.start(delay);return;}
    }
    presenting=false;emit sequenceFinished();
}
void ChipSounds::play(Clip &clip,const QString &name){
    if(!enabled)return;
    emit cueRequested(name);
    if(!clip.format.isValid()||volume==0)return;
    const QAudioDevice device=selectedDevice.isNull()?QMediaDevices::defaultAudioOutput():selectedDevice;
    if(device.isNull()||!device.isFormatSupported(clip.format)){qWarning()<<"Table audio output unavailable:"<<device.description();return;}
    stop(clip);
    if(!clip.output||clip.device!=device){
        // Ordinary application audio: notification sounds may be muted separately.
        clip.output=std::make_unique<QAudioSink>(device,clip.format,this);clip.device=device;
        clip.output->setObjectName(name+"AudioOutput");clip.output->setBufferSize(clip.format.bytesForDuration(40000));
    }
    clip.output->setVolume(name=="intro"?volume*0.5f:volume);clip.output->start(&clip.buffer);
}
void ChipSounds::observe(const QJsonObject &state){
    if(state.contains("connected")&&!state["connected"].toBool()){reset();return;}
    QString incoming=state["soundSession"].toString();if(incoming.isEmpty())return;
    const int bets=state["betSound"].toInt(),payouts=state["payoutSound"].toInt(),sequence=state["soundSequence"].toInt();
    const bool ordered=state.contains("soundEvents");
    // Joining, reconnecting, or opening a new table never replays past sounds.
    if(!primed||session!=incoming||bets<lastBet||payouts<lastPayout||(ordered&&sequence<lastSequence)){
        reset();session=incoming;primed=true;lastBet=bets;lastPayout=payouts;lastSequence=sequence;return;
    }
    if(enabled||animateDeals){
        // Bound an overloaded client's queue rather than playing minutes of stale audio.
        if(pending.size()>256)stopPlayback();
        if(ordered){
            int seen=lastSequence;
            for(const auto &value:state["soundEvents"].toArray()){
                auto event=value.toObject();int id=event["id"].toInt();
                if(id>seen&&id<=sequence){enqueue(event);seen=id;}
            }
        }else{ // Compatibility with earlier protocol-5 hosts.
            if(bets>lastBet)enqueue({{"cue","bet"}});
            if(payouts>lastPayout)enqueue({{"cue","payout"}});
        }
    }
    lastBet=bets;lastPayout=payouts;lastSequence=sequence;
    if(!dispatching&&!sequenceTimer.isActive()&&!pending.isEmpty())playNext();
}
}
