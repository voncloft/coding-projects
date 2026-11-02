// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 voncloft-casino contributors
#pragma once
#include <QtCore>
#include <QAudioSink>
#include <QAudioDevice>
#include <array>
#include <memory>
namespace casino {
class ChipSounds : public QObject {
    Q_OBJECT
public:
    explicit ChipSounds(QObject *parent=nullptr);
    void observe(const QJsonObject &state);
    void reset();
    void setEnabled(bool enabled);
    bool isEnabled() const {return enabled;}
    bool isBusy() const {return sequenceTimer.isActive()||!pending.isEmpty();}
    void setVolume(float volume);
    void setAudioDevice(const QAudioDevice &device);
    bool samplesReady() const;
    void playBet();
    void setAnimateDeals(bool value){animateDeals=value;}
    bool isPresenting() const {return presenting;}
    void startIntro();
    void stopIntro();
    bool introReady() const {return intro.format.isValid();}
signals:
    void eventStarted(const QJsonObject &event);
    void sequenceFinished();
    void cueRequested(const QString &cue); // emitted when playback starts
    void cueQueued(const QString &cue);    // accepted once from local/network state
private:
    struct Clip {
        QBuffer buffer;
        QAudioFormat format;
        std::unique_ptr<QAudioSink> output;
        QAudioDevice device;
        int durationMs() const {return format.isValid()?int(format.durationForBytes(buffer.size())/1000):0;}
    };
    // Four dealing voices preserve each recording's tail during a quick deal.
    std::array<Clip,10> clips;
    QAudioDevice selectedDevice;
    Clip intro;
    QTimer sequenceTimer,introTimer;
    QQueue<QJsonObject> pending;
    bool animateDeals=false,presenting=false,introWanted=false,dispatching=false;
    QString session;
    int lastBet=0,lastPayout=0,lastSequence=0,dealVoice=0;
    bool primed=false,enabled=true;
    float volume=0.55f;
    static void load(Clip &clip,const QString &path);
    static void stop(Clip &clip);
    void stopPlayback();
    void enqueue(QJsonObject event);
    void playNext();
    void play(Clip &clip,const QString &name);
};
}
