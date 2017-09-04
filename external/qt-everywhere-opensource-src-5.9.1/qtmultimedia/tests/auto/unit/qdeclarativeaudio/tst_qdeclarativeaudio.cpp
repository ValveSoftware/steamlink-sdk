/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//TESTED_COMPONENT=plugins/declarative/multimedia

#include <QtTest/QtTest>

#include "qdeclarativeaudio_p.h"
#include "qdeclarativemediametadata_p.h"

#include "mockmediaserviceprovider.h"
#include "mockmediaplayerservice.h"

#include <QtMultimedia/qmediametadata.h>
#include <qmediaplayercontrol.h>
#include <qmediaservice.h>
#include <private/qmediaserviceprovider_p.h>
#include <qmetadatareadercontrol.h>

#include <QtGui/qguiapplication.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>

class tst_QDeclarativeAudio : public QObject
{
    Q_OBJECT
public slots:
    void initTestCase();

private slots:
    void nullPlayerControl();
    void nullMetaDataControl();
    void nullService();

    void source();
    void autoLoad();
    void playing();
    void paused();
    void duration();
    void position();
    void volume();
    void muted();
    void bufferProgress();
    void seekable();
    void playbackRate();
    void status();
    void metaData_data();
    void metaData();
    void error();
    void loops();
    void audioRole();
};

Q_DECLARE_METATYPE(QDeclarativeAudio::Error);
Q_DECLARE_METATYPE(QDeclarativeAudio::AudioRole);

class QtTestMediaPlayerControl : public QMediaPlayerControl
{
    Q_OBJECT
public:
    QtTestMediaPlayerControl(QObject *parent = 0)
        : QMediaPlayerControl(parent)
        , m_state(QMediaPlayer::StoppedState)
        , m_mediaStatus(QMediaPlayer::NoMedia)
        , m_duration(0)
        , m_position(0)
        , m_playbackRate(1.0)
        , m_volume(100)
        , m_bufferStatus(0)
        , m_muted(false)
        , m_audioAvailable(false)
        , m_videoAvailable(false)
        , m_seekable(false)
    {
    }

    QMediaPlayer::State state() const { return m_state; }
    void updateState(QMediaPlayer::State state) { emit stateChanged(m_state = state); }

    QMediaPlayer::MediaStatus mediaStatus() const { return m_mediaStatus; }
    void updateMediaStatus(QMediaPlayer::MediaStatus status) {
        emit mediaStatusChanged(m_mediaStatus = status); }
    void updateMediaStatus(QMediaPlayer::MediaStatus status, QMediaPlayer::State state)
    {
        m_mediaStatus = status;
        m_state = state;

        emit mediaStatusChanged(m_mediaStatus);
        emit stateChanged(m_state);
    }

    qint64 duration() const { return m_duration; }
    void setDuration(qint64 duration) { emit durationChanged(m_duration = duration); }

    qint64 position() const { return m_position; }
    void setPosition(qint64 position) { emit positionChanged(m_position = position); }

    int volume() const { return m_volume; }
    void setVolume(int volume) { emit volumeChanged(m_volume = volume); }

    bool isMuted() const { return m_muted; }
    void setMuted(bool muted) { emit mutedChanged(m_muted = muted); }

    int bufferStatus() const { return m_bufferStatus; }
    void setBufferStatus(int status) { emit bufferStatusChanged(m_bufferStatus = status); }

    bool isAudioAvailable() const { return m_audioAvailable; }
    void setAudioAvailable(bool available) {
        emit audioAvailableChanged(m_audioAvailable = available); }
    bool isVideoAvailable() const { return m_videoAvailable; }
    void setVideoAvailable(bool available) {
        emit videoAvailableChanged(m_videoAvailable = available); }

    bool isSeekable() const { return m_seekable; }
    void setSeekable(bool seekable) { emit seekableChanged(m_seekable = seekable); }

    QMediaTimeRange availablePlaybackRanges() const { return QMediaTimeRange(); }

    qreal playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(qreal rate) { emit playbackRateChanged(m_playbackRate = rate); }

    QMediaContent media() const { return m_media; }
    const QIODevice *mediaStream() const { return 0; }
    void setMedia(const QMediaContent &media, QIODevice *)
    {
        m_media = media;

        m_mediaStatus = m_media.isNull()
                ? QMediaPlayer::NoMedia
                : QMediaPlayer::LoadingMedia;

        emit mediaChanged(m_media);
        emit mediaStatusChanged(m_mediaStatus);
    }

    void play()
    {
        m_state = QMediaPlayer::PlayingState;
        if (m_mediaStatus == QMediaPlayer::EndOfMedia)
            updateMediaStatus(QMediaPlayer::LoadedMedia);
        emit stateChanged(m_state);
    }
    void pause() { emit stateChanged(m_state = QMediaPlayer::PausedState); }
    void stop() { emit stateChanged(m_state = QMediaPlayer::StoppedState); }

    void emitError(QMediaPlayer::Error err, const QString &errorString) {
        emit error(err, errorString); }

private:
    QMediaPlayer::State m_state;
    QMediaPlayer::MediaStatus m_mediaStatus;
    qint64 m_duration;
    qint64 m_position;
    qreal m_playbackRate;
    int m_volume;
    int m_bufferStatus;
    bool m_muted;
    bool m_audioAvailable;
    bool m_videoAvailable;
    bool m_seekable;
    QMediaContent m_media;
};

class QtTestMetaDataControl : public QMetaDataReaderControl
{
    Q_OBJECT
public:
    QtTestMetaDataControl(QObject *parent = 0)
        : QMetaDataReaderControl(parent)
    {
    }

    bool isMetaDataAvailable() const { return true; }

    QVariant metaData(const QString &key) const { return m_metaData.value(key); }
    void setMetaData(const QString &key, const QVariant &value) {
        m_metaData.insert(key, value); emit metaDataChanged(); }

    QStringList availableMetaData() const { return m_metaData.keys(); }

private:
    QMap<QString, QVariant> m_metaData;
};

class QtTestMediaService : public QMediaService
{
    Q_OBJECT
public:
    QtTestMediaService(
            QtTestMediaPlayerControl *playerControl,
            QtTestMetaDataControl *metaDataControl,
            QObject *parent)
        : QMediaService(parent)
        , playerControl(playerControl)
        , metaDataControl(metaDataControl)
    {
    }

    QMediaControl *requestControl(const char *name)
    {
        if (qstrcmp(name, QMediaPlayerControl_iid) == 0)
            return playerControl;
        else if (qstrcmp(name, QMetaDataReaderControl_iid) == 0)
            return metaDataControl;
        else
            return 0;
    }

    void releaseControl(QMediaControl *) {}

    QtTestMediaPlayerControl *playerControl;
    QtTestMetaDataControl *metaDataControl;
};

class QtTestMediaServiceProvider : public QMediaServiceProvider
{
    Q_OBJECT
public:
    QtTestMediaServiceProvider()
        : service(new QtTestMediaService(
                new QtTestMediaPlayerControl(this), new QtTestMetaDataControl(this), this))
    {
        setDefaultServiceProvider(this);
    }

    QtTestMediaServiceProvider(QtTestMediaService *service)
        : service(service)
    {
        setDefaultServiceProvider(this);
    }

    QtTestMediaServiceProvider(
            QtTestMediaPlayerControl *playerControl, QtTestMetaDataControl *metaDataControl)
        : service(new QtTestMediaService(playerControl, metaDataControl, this))
    {
        setDefaultServiceProvider(this);
    }

    ~QtTestMediaServiceProvider()
    {
        setDefaultServiceProvider(0);
    }

    QMediaService *requestService(
            const QByteArray &type,
            const QMediaServiceProviderHint & = QMediaServiceProviderHint())
    {
        requestedService = type;

        return service;
    }

    void releaseService(QMediaService *) {}

    inline QtTestMediaPlayerControl *playerControl() { return service->playerControl; }
    inline QtTestMetaDataControl *metaDataControl() { return service->metaDataControl; }

    QtTestMediaService *service;
    QByteArray requestedService;
};

void tst_QDeclarativeAudio::initTestCase()
{
    qRegisterMetaType<QDeclarativeAudio::Error>();
    qRegisterMetaType<QDeclarativeAudio::AudioRole>();
}

void tst_QDeclarativeAudio::nullPlayerControl()
{
    QtTestMetaDataControl metaDataControl;
    QtTestMediaServiceProvider provider(0, &metaDataControl);

    QDeclarativeAudio audio;
    audio.classBegin();

    QCOMPARE(audio.source(), QUrl());
    audio.setSource(QUrl("http://example.com"));
    QCOMPARE(audio.source(), QUrl("http://example.com"));

    QCOMPARE(audio.playbackState(), audio.StoppedState);
    audio.play();
    QCOMPARE(audio.playbackState(), audio.StoppedState);
    audio.pause();
    QCOMPARE(audio.playbackState(), audio.StoppedState);

    QCOMPARE(audio.duration(), 0);

    QCOMPARE(audio.position(), 0);
    audio.seek(10000);
    QCOMPARE(audio.position(), 10000);

    QCOMPARE(audio.volume(), qreal(1.0));
    audio.setVolume(0.5);
    QCOMPARE(audio.volume(), qreal(0.5));

    QCOMPARE(audio.isMuted(), false);
    audio.setMuted(true);
    QCOMPARE(audio.isMuted(), true);

    QCOMPARE(audio.bufferProgress(), qreal(0));

    QCOMPARE(audio.isSeekable(), false);

    QCOMPARE(audio.playbackRate(), qreal(1.0));

    QCOMPARE(audio.status(), QDeclarativeAudio::NoMedia);

    QCOMPARE(audio.error(), QDeclarativeAudio::ServiceMissing);
}

void tst_QDeclarativeAudio::nullMetaDataControl()
{
    QtTestMediaPlayerControl playerControl;
    QtTestMediaServiceProvider provider(&playerControl, 0);

    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QVERIFY(audio.metaData());
}

void tst_QDeclarativeAudio::nullService()
{
    QtTestMediaServiceProvider provider(0);

    QDeclarativeAudio audio;
    audio.classBegin();

    QCOMPARE(audio.source(), QUrl());
    audio.setSource(QUrl("http://example.com"));
    QCOMPARE(audio.source(), QUrl("http://example.com"));

    QCOMPARE(audio.playbackState(), audio.StoppedState);
    audio.play();
    QCOMPARE(audio.playbackState(), audio.StoppedState);
    audio.pause();
    QCOMPARE(audio.playbackState(), audio.StoppedState);

    QCOMPARE(audio.duration(), 0);

    QCOMPARE(audio.position(), 0);
    audio.seek(10000);
    QCOMPARE(audio.position(), 10000);

    QCOMPARE(audio.volume(), qreal(1.0));
    audio.setVolume(0.5);
    QCOMPARE(audio.volume(), qreal(0.5));

    QCOMPARE(audio.isMuted(), false);
    audio.setMuted(true);
    QCOMPARE(audio.isMuted(), true);

    QCOMPARE(audio.bufferProgress(), qreal(0));

    QCOMPARE(audio.isSeekable(), false);

    QCOMPARE(audio.playbackRate(), qreal(1.0));

    QCOMPARE(audio.status(), QDeclarativeAudio::NoMedia);

    QCOMPARE(audio.error(), QDeclarativeAudio::ServiceMissing);

    QVERIFY(audio.metaData());
}

void tst_QDeclarativeAudio::source()
{
    const QUrl url1("http://example.com");
    const QUrl url2("file:///local/path");
    const QUrl url3;

    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(sourceChanged()));

    audio.setSource(url1);
    QCOMPARE(audio.source(), url1);
    QCOMPARE(provider.playerControl()->media().canonicalUrl(), url1);
    QCOMPARE(spy.count(), 1);

    audio.setSource(url2);
    QCOMPARE(audio.source(), url2);
    QCOMPARE(provider.playerControl()->media().canonicalUrl(), url2);
    QCOMPARE(spy.count(), 2);

    audio.setSource(url3);
    QCOMPARE(audio.source(), url3);
    QCOMPARE(provider.playerControl()->media().canonicalUrl(), url3);
    QCOMPARE(spy.count(), 3);
}

void tst_QDeclarativeAudio::autoLoad()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(autoLoadChanged()));

    QCOMPARE(audio.isAutoLoad(), true);

    audio.setAutoLoad(false);
    QCOMPARE(audio.isAutoLoad(), false);
    QCOMPARE(spy.count(), 1);

    audio.setSource(QUrl("http://example.com"));
    QCOMPARE(audio.source(), QUrl("http://example.com"));
    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    audio.stop();

    audio.setAutoLoad(true);
    audio.setSource(QUrl("http://example.com"));
    audio.pause();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(audio.playbackState(), audio.PausedState);
}

void tst_QDeclarativeAudio::playing()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();

    QSignalSpy stateChangedSpy(&audio, SIGNAL(playbackStateChanged()));
    QSignalSpy playingSpy(&audio, SIGNAL(playing()));
    QSignalSpy stoppedSpy(&audio, SIGNAL(stopped()));

    int stateChanged = 0;
    int playing = 0;
    int stopped = 0;

    audio.componentComplete();
    audio.setSource(QUrl("http://example.com"));

    QCOMPARE(audio.playbackState(), audio.StoppedState);

    // play() when stopped.
    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(playingSpy.count(),        ++playing);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // stop() when playing.
    audio.stop();
    QCOMPARE(audio.playbackState(), audio.StoppedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(playingSpy.count(),          playing);
    QCOMPARE(stoppedSpy.count(),        ++stopped);

    // stop() when stopped.
    audio.stop();
    QCOMPARE(audio.playbackState(), audio.StoppedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateChangedSpy.count(),   stateChanged);
    QCOMPARE(playingSpy.count(),          playing);
    QCOMPARE(stoppedSpy.count(),          stopped);

    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(playingSpy.count(),        ++playing);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // play() when playing.
    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(),   stateChanged);
    QCOMPARE(playingSpy.count(),          playing);
    QCOMPARE(stoppedSpy.count(),          stopped);
}

void tst_QDeclarativeAudio::paused()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();

    QSignalSpy stateChangedSpy(&audio, SIGNAL(playbackStateChanged()));
    QSignalSpy pausedSpy(&audio, SIGNAL(paused()));
    int stateChanged = 0;
    int pausedCount = 0;

    audio.componentComplete();
    audio.setSource(QUrl("http://example.com"));

    QCOMPARE(audio.playbackState(), audio.StoppedState);

    // play() when stopped.
    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(pausedSpy.count(), pausedCount);

    // pause() when playing.
    audio.pause();
    QCOMPARE(audio.playbackState(), audio.PausedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(pausedSpy.count(), ++pausedCount);

    // pause() when paused.
    audio.pause();
    QCOMPARE(audio.playbackState(), audio.PausedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(stateChangedSpy.count(),   stateChanged);
    QCOMPARE(pausedSpy.count(), pausedCount);

    // stop() when paused.
    audio.stop();
    QCOMPARE(audio.playbackState(), audio.StoppedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(pausedSpy.count(), pausedCount);

    // pause() when stopped.
    audio.pause();
    QCOMPARE(audio.playbackState(), audio.PausedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(pausedSpy.count(), ++pausedCount);

    // play() when paused.
    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(pausedSpy.count(), pausedCount);
}

void tst_QDeclarativeAudio::duration()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(durationChanged()));

    QCOMPARE(audio.duration(), 0);

    provider.playerControl()->setDuration(4040);
    QCOMPARE(audio.duration(), 4040);
    QCOMPARE(spy.count(), 1);

    provider.playerControl()->setDuration(-129);
    QCOMPARE(audio.duration(), -129);
    QCOMPARE(spy.count(), 2);

    provider.playerControl()->setDuration(0);
    QCOMPARE(audio.duration(), 0);
    QCOMPARE(spy.count(), 3);

    // Unnecessary duration changed signals aren't filtered.
    provider.playerControl()->setDuration(0);
    QCOMPARE(audio.duration(), 0);
    QCOMPARE(spy.count(), 4);
}

void tst_QDeclarativeAudio::position()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(positionChanged()));

    QCOMPARE(audio.position(), 0);

    // QDeclarativeAudio won't bound set positions to the duration.  A media service may though.
    QCOMPARE(audio.duration(), 0);

    audio.seek(450);
    QCOMPARE(audio.position(), 450);
    QCOMPARE(provider.playerControl()->position(), qint64(450));
    QCOMPARE(spy.count(), 1);

    audio.seek(-5403);
    QCOMPARE(audio.position(), 0);
    QCOMPARE(provider.playerControl()->position(), qint64(0));
    QCOMPARE(spy.count(), 2);

    audio.seek(-5403);
    QCOMPARE(audio.position(), 0);
    QCOMPARE(provider.playerControl()->position(), qint64(0));
    QCOMPARE(spy.count(), 2);

    // Check the signal change signal is emitted if the change originates from the media service.
    provider.playerControl()->setPosition(450);
    QCOMPARE(audio.position(), 450);
    QCOMPARE(spy.count(), 3);

    connect(&audio, SIGNAL(positionChanged()), &QTestEventLoop::instance(), SLOT(exitLoop()));

    provider.playerControl()->updateState(QMediaPlayer::PlayingState);
    QTestEventLoop::instance().enterLoop(2);
    QVERIFY(spy.count() > 3 && spy.count() < 6); // 4 or 5

    provider.playerControl()->updateState(QMediaPlayer::PausedState);
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(spy.count() < 6);
}

void tst_QDeclarativeAudio::volume()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(volumeChanged()));

    QCOMPARE(audio.volume(), qreal(1.0));

    audio.setVolume(0.7);
    QCOMPARE(audio.volume(), qreal(0.7));
    QCOMPARE(provider.playerControl()->volume(), 70);
    QCOMPARE(spy.count(), 1);

    audio.setVolume(0.7);
    QCOMPARE(audio.volume(), qreal(0.7));
    QCOMPARE(provider.playerControl()->volume(), 70);
    QCOMPARE(spy.count(), 1);

    provider.playerControl()->setVolume(30);
    QCOMPARE(audio.volume(), qreal(0.3));
    QCOMPARE(spy.count(), 2);
}

void tst_QDeclarativeAudio::muted()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(mutedChanged()));

    QCOMPARE(audio.isMuted(), false);

    audio.setMuted(true);
    QCOMPARE(audio.isMuted(), true);
    QCOMPARE(provider.playerControl()->isMuted(), true);
    QCOMPARE(spy.count(), 1);

    provider.playerControl()->setMuted(false);
    QCOMPARE(audio.isMuted(), false);
    QCOMPARE(spy.count(), 2);

    audio.setMuted(false);
    QCOMPARE(audio.isMuted(), false);
    QCOMPARE(provider.playerControl()->isMuted(), false);
    QCOMPARE(spy.count(), 2);
}

void tst_QDeclarativeAudio::bufferProgress()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(bufferProgressChanged()));

    QCOMPARE(audio.bufferProgress(), qreal(0.0));

    provider.playerControl()->setBufferStatus(20);
    QCOMPARE(audio.bufferProgress(), qreal(0.2));
    QCOMPARE(spy.count(), 1);

    provider.playerControl()->setBufferStatus(20);
    QCOMPARE(audio.bufferProgress(), qreal(0.2));
    QCOMPARE(spy.count(), 2);

    provider.playerControl()->setBufferStatus(40);
    QCOMPARE(audio.bufferProgress(), qreal(0.4));
    QCOMPARE(spy.count(), 3);

    connect(&audio, SIGNAL(positionChanged()), &QTestEventLoop::instance(), SLOT(exitLoop()));

    provider.playerControl()->updateMediaStatus(
            QMediaPlayer::BufferingMedia, QMediaPlayer::PlayingState);
    QTestEventLoop::instance().enterLoop(2);
    QVERIFY(spy.count() > 3 && spy.count() < 6); // 4 or 5

    provider.playerControl()->updateMediaStatus(QMediaPlayer::BufferedMedia);
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(spy.count() < 6);
}

void tst_QDeclarativeAudio::seekable()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(seekableChanged()));

    QCOMPARE(audio.isSeekable(), false);

    provider.playerControl()->setSeekable(true);
    QCOMPARE(audio.isSeekable(), true);
    QCOMPARE(spy.count(), 1);

    provider.playerControl()->setSeekable(true);
    QCOMPARE(audio.isSeekable(), true);
    QCOMPARE(spy.count(), 2);

    provider.playerControl()->setSeekable(false);
    QCOMPARE(audio.isSeekable(), false);
    QCOMPARE(spy.count(), 3);
}

void tst_QDeclarativeAudio::playbackRate()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(&audio, SIGNAL(playbackRateChanged()));

    QCOMPARE(audio.playbackRate(), qreal(1.0));

    audio.setPlaybackRate(0.5);
    QCOMPARE(audio.playbackRate(), qreal(0.5));
    QCOMPARE(provider.playerControl()->playbackRate(), qreal(0.5));
    QCOMPARE(spy.count(), 1);

    provider.playerControl()->setPlaybackRate(2.0);
    QCOMPARE(provider.playerControl()->playbackRate(), qreal(2.0));
    QCOMPARE(spy.count(), 2);

    audio.setPlaybackRate(2.0);
    QCOMPARE(audio.playbackRate(), qreal(2.0));
    QCOMPARE(provider.playerControl()->playbackRate(), qreal(2.0));
    QCOMPARE(spy.count(), 2);
}

void tst_QDeclarativeAudio::status()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy statusChangedSpy(&audio, SIGNAL(statusChanged()));

    QCOMPARE(audio.status(), QDeclarativeAudio::NoMedia);

    // Set media, start loading.
    provider.playerControl()->updateMediaStatus(QMediaPlayer::LoadingMedia);
    QCOMPARE(audio.status(), QDeclarativeAudio::Loading);
    QCOMPARE(statusChangedSpy.count(), 1);

    // Finish loading.
    provider.playerControl()->updateMediaStatus(QMediaPlayer::LoadedMedia);
    QCOMPARE(audio.status(), QDeclarativeAudio::Loaded);
    QCOMPARE(statusChangedSpy.count(), 2);

    // Play, start buffering.
    provider.playerControl()->updateMediaStatus(
            QMediaPlayer::StalledMedia, QMediaPlayer::PlayingState);
    QCOMPARE(audio.status(), QDeclarativeAudio::Stalled);
    QCOMPARE(statusChangedSpy.count(), 3);

    // Enough data buffered to proceed.
    provider.playerControl()->updateMediaStatus(QMediaPlayer::BufferingMedia);
    QCOMPARE(audio.status(), QDeclarativeAudio::Buffering);
    QCOMPARE(statusChangedSpy.count(), 4);

    // Errant second buffering status changed.
    provider.playerControl()->updateMediaStatus(QMediaPlayer::BufferingMedia);
    QCOMPARE(audio.status(), QDeclarativeAudio::Buffering);
    QCOMPARE(statusChangedSpy.count(), 4);

    // Buffer full.
    provider.playerControl()->updateMediaStatus(QMediaPlayer::BufferedMedia);
    QCOMPARE(audio.status(), QDeclarativeAudio::Buffered);
    QCOMPARE(statusChangedSpy.count(), 5);

    // Buffer getting low.
    provider.playerControl()->updateMediaStatus(QMediaPlayer::BufferingMedia);
    QCOMPARE(audio.status(), QDeclarativeAudio::Buffering);
    QCOMPARE(statusChangedSpy.count(), 6);

    // Buffer full.
    provider.playerControl()->updateMediaStatus(QMediaPlayer::BufferedMedia);
    QCOMPARE(audio.status(), QDeclarativeAudio::Buffered);
    QCOMPARE(statusChangedSpy.count(), 7);

    // Finished.
    provider.playerControl()->updateMediaStatus(
            QMediaPlayer::EndOfMedia, QMediaPlayer::StoppedState);
    QCOMPARE(audio.status(), QDeclarativeAudio::EndOfMedia);
    QCOMPARE(statusChangedSpy.count(), 8);
}

void tst_QDeclarativeAudio::metaData_data()
{
    QTest::addColumn<QByteArray>("propertyName");
    QTest::addColumn<QString>("propertyKey");
    QTest::addColumn<QVariant>("value");

    QTest::newRow("title")
            << QByteArray("title")
            << QMediaMetaData::Title
            << QVariant(QString::fromLatin1("This is a title"));

    QTest::newRow("genre")
            << QByteArray("genre")
            << QMediaMetaData::Genre
            << QVariant(QString::fromLatin1("rock"));

    QTest::newRow("trackNumber")
            << QByteArray("trackNumber")
            << QMediaMetaData::TrackNumber
            << QVariant(8);
}

void tst_QDeclarativeAudio::metaData()
{
    QFETCH(QByteArray, propertyName);
    QFETCH(QString, propertyKey);
    QFETCH(QVariant, value);

    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy spy(audio.metaData(), SIGNAL(metaDataChanged()));

    const int index = audio.metaData()->metaObject()->indexOfProperty(propertyName.constData());
    QVERIFY(index != -1);

    QMetaProperty property = audio.metaData()->metaObject()->property(index);
    QCOMPARE(property.read(&audio), QVariant());

    property.write(audio.metaData(), value);
    QCOMPARE(property.read(audio.metaData()), QVariant());
    QCOMPARE(provider.metaDataControl()->metaData(propertyKey), QVariant());
    QCOMPARE(spy.count(), 0);

    provider.metaDataControl()->setMetaData(propertyKey, value);
    QCOMPARE(property.read(audio.metaData()), value);
    QCOMPARE(spy.count(), 1);
}

void tst_QDeclarativeAudio::error()
{
    const QString errorString = QLatin1String("Failed to open device.");

    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;
    audio.classBegin();
    audio.componentComplete();

    QSignalSpy errorSpy(&audio, SIGNAL(error(QDeclarativeAudio::Error,QString)));
    QSignalSpy errorChangedSpy(&audio, SIGNAL(errorChanged()));

    QCOMPARE(audio.error(), QDeclarativeAudio::NoError);
    QCOMPARE(audio.errorString(), QString());

    provider.playerControl()->emitError(QMediaPlayer::ResourceError, errorString);

    QCOMPARE(audio.error(), QDeclarativeAudio::ResourceError);
    QCOMPARE(audio.errorString(), errorString);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorChangedSpy.count(), 1);

    // Changing the source resets the error properties.
    audio.setSource(QUrl("http://example.com"));
    QCOMPARE(audio.error(), QDeclarativeAudio::NoError);
    QCOMPARE(audio.errorString(), QString());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorChangedSpy.count(), 2);

    // But isn't noisy.
    audio.setSource(QUrl("file:///file/path"));
    QCOMPARE(audio.error(), QDeclarativeAudio::NoError);
    QCOMPARE(audio.errorString(), QString());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorChangedSpy.count(), 2);
}

void tst_QDeclarativeAudio::loops()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeAudio audio;

    QSignalSpy loopsChangedSpy(&audio, SIGNAL(loopCountChanged()));
    QSignalSpy stateChangedSpy(&audio, SIGNAL(playbackStateChanged()));
    QSignalSpy stoppedSpy(&audio, SIGNAL(stopped()));

    int stateChanged = 0;
    int loopsChanged = 0;
    int stoppedCount = 0;

    audio.classBegin();
    audio.componentComplete();

    QCOMPARE(audio.playbackState(), audio.StoppedState);

    //setLoopCount(3) when stopped.
    audio.setLoopCount(3);
    QCOMPARE(audio.loopCount(), 3);
    QCOMPARE(loopsChangedSpy.count(), ++loopsChanged);

    //play till end
    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(stoppedSpy.count(), stoppedCount);

    // play() when playing.
    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(),   stateChanged);
    QCOMPARE(stoppedSpy.count(), stoppedCount);

    provider.playerControl()->updateMediaStatus(QMediaPlayer::EndOfMedia, QMediaPlayer::StoppedState);
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(),   stateChanged);
    QCOMPARE(stoppedSpy.count(), stoppedCount);

    //play to end
    provider.playerControl()->updateMediaStatus(QMediaPlayer::EndOfMedia, QMediaPlayer::StoppedState);
    QCOMPARE(stoppedSpy.count(), stoppedCount);
    //play to end
    provider.playerControl()->updateMediaStatus(QMediaPlayer::EndOfMedia, QMediaPlayer::StoppedState);
    QCOMPARE(audio.playbackState(), audio.StoppedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(stoppedSpy.count(), ++stoppedCount);

    // stop when playing
    audio.play();
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(stoppedSpy.count(), stoppedCount);
    provider.playerControl()->updateMediaStatus(QMediaPlayer::EndOfMedia, QMediaPlayer::StoppedState);
    audio.stop();
    QCOMPARE(audio.playbackState(), audio.StoppedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(stoppedSpy.count(), ++stoppedCount);

    //play() with infinite loop
    audio.setLoopCount(-1);
    QCOMPARE(audio.loopCount(), -1);
    QCOMPARE(loopsChangedSpy.count(), ++loopsChanged);
    audio.play();
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    provider.playerControl()->updateMediaStatus(QMediaPlayer::EndOfMedia, QMediaPlayer::StoppedState);
    QCOMPARE(audio.playbackState(), audio.PlayingState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateChangedSpy.count(), stateChanged);

    // stop() when playing in infinite loop.
    audio.stop();
    QCOMPARE(audio.playbackState(), audio.StoppedState);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateChangedSpy.count(), ++stateChanged);
    QCOMPARE(stoppedSpy.count(), ++stoppedCount);

    qDebug() << "Testing version 5";
}

void tst_QDeclarativeAudio::audioRole()
{
    MockMediaPlayerService mockService;
    MockMediaServiceProvider mockProvider(&mockService);
    QMediaServiceProvider::setDefaultServiceProvider(&mockProvider);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0 \n import QtMultimedia 5.6 \n Audio { }", QUrl());

    {
        mockService.setHasAudioRole(false);
        QDeclarativeAudio *audio = static_cast<QDeclarativeAudio*>(component.create());

        QCOMPARE(audio->audioRole(), QDeclarativeAudio::UnknownRole);
        QVERIFY(audio->supportedAudioRoles().isArray());
        QVERIFY(audio->supportedAudioRoles().toVariant().toList().isEmpty());

        QSignalSpy spy(audio, SIGNAL(audioRoleChanged()));
        audio->setAudioRole(QDeclarativeAudio::MusicRole);
        QCOMPARE(audio->audioRole(), QDeclarativeAudio::UnknownRole);
        QCOMPARE(spy.count(), 0);
    }

    {
        mockService.reset();
        mockService.setHasAudioRole(true);
        QDeclarativeAudio *audio = static_cast<QDeclarativeAudio*>(component.create());
        QSignalSpy spy(audio, SIGNAL(audioRoleChanged()));

        QCOMPARE(audio->audioRole(), QDeclarativeAudio::UnknownRole);
        QVERIFY(audio->supportedAudioRoles().isArray());
        QVERIFY(!audio->supportedAudioRoles().toVariant().toList().isEmpty());

        audio->setAudioRole(QDeclarativeAudio::MusicRole);
        QCOMPARE(audio->audioRole(), QDeclarativeAudio::MusicRole);
        QCOMPARE(mockService.mockAudioRoleControl->audioRole(), QAudio::MusicRole);
        QCOMPARE(spy.count(), 1);
    }
}

QTEST_MAIN(tst_QDeclarativeAudio)

#include "tst_qdeclarativeaudio.moc"
