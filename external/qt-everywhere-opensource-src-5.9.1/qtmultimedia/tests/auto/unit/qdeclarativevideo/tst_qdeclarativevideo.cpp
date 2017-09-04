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

#include "qdeclarativevideo_p.h"


#include <qabstractvideosurface.h>
#include <qgraphicsvideoitem.h>
#include <qmediaplayercontrol.h>
#include <qmediaservice.h>
#include <qmediaserviceprovider.h>
#include <qvideorenderercontrol.h>
#include <qvideowindowcontrol.h>
#include <qvideosurfaceformat.h>

#include <QtWidgets/qapplication.h>
#include <QtGui/qpainter.h>

class tst_QDeclarativeVideo : public QObject
{
    Q_OBJECT
public slots:
    void initTestCase();

private slots:
    void nullPlayerControl();
    void nullService();

    void playing();
    void paused();
    void error();

    void hasAudio();
    void hasVideo();
    void fillMode();
    void geometry();
};

Q_DECLARE_METATYPE(QDeclarativeVideo::Error);

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
        , m_volume(50)
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

    void play() { emit stateChanged(m_state = QMediaPlayer::PlayingState); }
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

class QtTestRendererControl : public QVideoRendererControl
{
public:
    QtTestRendererControl(QObject *parent ) : QVideoRendererControl(parent), m_surface(0) {}

    QAbstractVideoSurface *surface() const { return m_surface; }
    void setSurface(QAbstractVideoSurface *surface) { m_surface = surface; }

private:
    QAbstractVideoSurface *m_surface;
};

class QtTestWindowControl : public QVideoWindowControl
{
public:
    QtTestWindowControl(QObject *parent)
        : QVideoWindowControl(parent)
        , m_winId(0)
        , m_repaintCount(0)
        , m_brightness(0)
        , m_contrast(0)
        , m_saturation(0)
        , m_aspectRatioMode(Qt::KeepAspectRatio)
        , m_fullScreen(0)
    {
    }

    WId winId() const { return m_winId; }
    void setWinId(WId id) { m_winId = id; }

    QRect displayRect() const { return m_displayRect; }
    void setDisplayRect(const QRect &rect) { m_displayRect = rect; }

    bool isFullScreen() const { return m_fullScreen; }
    void setFullScreen(bool fullScreen) { emit fullScreenChanged(m_fullScreen = fullScreen); }

    int repaintCount() const { return m_repaintCount; }
    void setRepaintCount(int count) { m_repaintCount = count; }
    void repaint() { ++m_repaintCount; }

    QSize nativeSize() const { return m_nativeSize; }
    void setNativeSize(const QSize &size) { m_nativeSize = size; emit nativeSizeChanged(); }

    Qt::AspectRatioMode aspectRatioMode() const { return m_aspectRatioMode; }
    void setAspectRatioMode(Qt::AspectRatioMode mode) { m_aspectRatioMode = mode; }

    int brightness() const { return m_brightness; }
    void setBrightness(int brightness) { emit brightnessChanged(m_brightness = brightness); }

    int contrast() const { return m_contrast; }
    void setContrast(int contrast) { emit contrastChanged(m_contrast = contrast); }

    int hue() const { return m_hue; }
    void setHue(int hue) { emit hueChanged(m_hue = hue); }

    int saturation() const { return m_saturation; }
    void setSaturation(int saturation) { emit saturationChanged(m_saturation = saturation); }

private:
    WId m_winId;
    int m_repaintCount;
    int m_brightness;
    int m_contrast;
    int m_hue;
    int m_saturation;
    Qt::AspectRatioMode m_aspectRatioMode;
    QRect m_displayRect;
    QSize m_nativeSize;
    bool m_fullScreen;
};

class QtTestMediaService : public QMediaService
{
    Q_OBJECT
public:
    QtTestMediaService(
            QtTestMediaPlayerControl *playerControl,
            QtTestRendererControl *rendererControl,
            QtTestWindowControl *windowControl,
            QObject *parent)
        : QMediaService(parent)
        , playerControl(playerControl)
        , rendererControl(rendererControl)
        , windowControl(windowControl)
    {
    }

    QMediaControl *requestControl(const char *name)
    {
        if (qstrcmp(name, QMediaPlayerControl_iid) == 0)
            return playerControl;
        else if (qstrcmp(name, QVideoRendererControl_iid) == 0)
            return rendererControl;
        else if (qstrcmp(name, QVideoWindowControl_iid) == 0)
            return windowControl;
        else
            return 0;
    }

    void releaseControl(QMediaControl *) {}

    QtTestMediaPlayerControl *playerControl;
    QtTestRendererControl *rendererControl;
    QtTestWindowControl *windowControl;
};

class QtTestMediaServiceProvider : public QMediaServiceProvider
{
    Q_OBJECT
public:
    QtTestMediaServiceProvider()
        : service(
            new QtTestMediaService(
            new QtTestMediaPlayerControl(this),
            new QtTestRendererControl(this),
            new QtTestWindowControl(this),
            this))
    {
        setDefaultServiceProvider(this);
    }

    QtTestMediaServiceProvider(QtTestMediaService *service)
        : service(service)
    {
        setDefaultServiceProvider(this);
    }

    QtTestMediaServiceProvider(
            QtTestMediaPlayerControl *playerControl,
            QtTestRendererControl *rendererControl,
            QtTestWindowControl *windowControl)
        : service(new QtTestMediaService(playerControl, rendererControl, windowControl, this))
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
    inline QtTestRendererControl *rendererControl() { return service->rendererControl; }

    QtTestMediaService *service;
    QByteArray requestedService;
};


void tst_QDeclarativeVideo::initTestCase()
{
    qRegisterMetaType<QDeclarativeVideo::Error>();
}

void tst_QDeclarativeVideo::nullPlayerControl()
{
    QtTestMediaServiceProvider provider(0, 0, 0);

    QDeclarativeVideo video;
    video.classBegin();

    QCOMPARE(video.source(), QUrl());
    video.setSource(QUrl("http://example.com"));
    QCOMPARE(video.source(), QUrl("http://example.com"));

    QCOMPARE(video.isPlaying(), false);
    video.setPlaying(true);
    QCOMPARE(video.isPlaying(), true);
    video.setPlaying(false);
    video.play();
    QCOMPARE(video.isPlaying(), false);

    QCOMPARE(video.isPaused(), false);
    video.pause();
    QCOMPARE(video.isPaused(), false);
    video.setPaused(true);
    QCOMPARE(video.isPaused(), true);

    QCOMPARE(video.duration(), 0);

    QCOMPARE(video.position(), 0);
    video.setPosition(10000);
    QCOMPARE(video.position(), 10000);

    QCOMPARE(video.volume(), qreal(1.0));
    video.setVolume(0.5);
    QCOMPARE(video.volume(), qreal(0.5));

    QCOMPARE(video.isMuted(), false);
    video.setMuted(true);
    QCOMPARE(video.isMuted(), true);

    QCOMPARE(video.bufferProgress(), qreal(0));

    QCOMPARE(video.isSeekable(), false);

    QCOMPARE(video.playbackRate(), qreal(1.0));

    QCOMPARE(video.hasAudio(), false);
    QCOMPARE(video.hasVideo(), false);

    QCOMPARE(video.status(), QDeclarativeVideo::NoMedia);

    QCOMPARE(video.error(), QDeclarativeVideo::ServiceMissing);
}

void tst_QDeclarativeVideo::nullService()
{
    QtTestMediaServiceProvider provider(0);

    QDeclarativeVideo video;
    video.classBegin();

    QCOMPARE(video.source(), QUrl());
    video.setSource(QUrl("http://example.com"));
    QCOMPARE(video.source(), QUrl("http://example.com"));

    QCOMPARE(video.isPlaying(), false);
    video.setPlaying(true);
    QCOMPARE(video.isPlaying(), true);
    video.setPlaying(false);
    video.play();
    QCOMPARE(video.isPlaying(), false);

    QCOMPARE(video.isPaused(), false);
    video.pause();
    QCOMPARE(video.isPaused(), false);
    video.setPaused(true);
    QCOMPARE(video.isPaused(), true);

    QCOMPARE(video.duration(), 0);

    QCOMPARE(video.position(), 0);
    video.setPosition(10000);
    QCOMPARE(video.position(), 10000);

    QCOMPARE(video.volume(), qreal(1.0));
    video.setVolume(0.5);
    QCOMPARE(video.volume(), qreal(0.5));

    QCOMPARE(video.isMuted(), false);
    video.setMuted(true);
    QCOMPARE(video.isMuted(), true);

    QCOMPARE(video.bufferProgress(), qreal(0));

    QCOMPARE(video.isSeekable(), false);

    QCOMPARE(video.playbackRate(), qreal(1.0));

    QCOMPARE(video.hasAudio(), false);
    QCOMPARE(video.hasVideo(), false);

    QCOMPARE(video.status(), QDeclarativeVideo::NoMedia);

    QCOMPARE(video.error(), QDeclarativeVideo::ServiceMissing);

    QCOMPARE(video.metaObject()->indexOfProperty("title"), -1);
    QCOMPARE(video.metaObject()->indexOfProperty("genre"), -1);
    QCOMPARE(video.metaObject()->indexOfProperty("description"), -1);
}

void tst_QDeclarativeVideo::playing()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeVideo video;
    video.classBegin();
    video.componentComplete();
    video.setSource(QUrl("http://example.com"));

    QSignalSpy playingChangedSpy(&video, SIGNAL(playingChanged()));
    QSignalSpy startedSpy(&video, SIGNAL(started()));
    QSignalSpy stoppedSpy(&video, SIGNAL(stopped()));

    int playingChanged = 0;
    int started = 0;
    int stopped = 0;

    QCOMPARE(video.isPlaying(), false);

    // setPlaying(true) when stopped.
    video.setPlaying(true);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(startedSpy.count(),        ++started);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPlaying(false) when playing.
    video.setPlaying(false);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(stoppedSpy.count(),        ++stopped);

    // play() when stopped.
    video.play();
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(startedSpy.count(),        ++started);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // stop() when playing.
    video.stop();
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(stoppedSpy.count(),        ++stopped);

    // stop() when stopped.
    video.stop();
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPlaying(false) when stopped.
    video.setPlaying(false);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(stoppedSpy.count(),          stopped);

    video.setPlaying(true);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(startedSpy.count(),        ++started);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPlaying(true) when playing.
    video.setPlaying(true);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // play() when playing.
    video.play();
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(stoppedSpy.count(),          stopped);
}

void tst_QDeclarativeVideo::paused()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeVideo video;
    video.classBegin();
    video.componentComplete();
    video.setSource(QUrl("http://example.com"));

    QSignalSpy playingChangedSpy(&video, SIGNAL(playingChanged()));
    QSignalSpy pausedChangedSpy(&video, SIGNAL(pausedChanged()));
    QSignalSpy startedSpy(&video, SIGNAL(started()));
    QSignalSpy pausedSpy(&video, SIGNAL(paused()));
    QSignalSpy resumedSpy(&video, SIGNAL(resumed()));
    QSignalSpy stoppedSpy(&video, SIGNAL(stopped()));

    int playingChanged = 0;
    int pausedChanged = 0;
    int started = 0;
    int paused = 0;
    int resumed = 0;
    int stopped = 0;

    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), false);

    // setPlaying(true) when stopped.
    video.setPlaying(true);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),        ++started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPaused(true) when playing.
    video.setPaused(true);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),         ++paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPaused(true) when paused.
    video.setPaused(true);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // pause() when paused.
    video.pause();
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPaused(false) when paused.
    video.setPaused(false);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),        ++resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPaused(false) when playing.
    video.setPaused(false);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // pause() when playing.
    video.pause();
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),         ++paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPlaying(false) when paused.
    video.setPlaying(false);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),        ++stopped);

    // setPaused(true) when stopped and paused.
    video.setPaused(true);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPaused(false) when stopped and paused.
    video.setPaused(false);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPaused(true) when stopped.
    video.setPaused(true);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPlaying(true) when stopped and paused.
    video.setPlaying(true);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),        ++started);
    QCOMPARE(pausedSpy.count(),         ++paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // play() when paused.
    video.play();
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),        ++resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPaused(true) when playing.
    video.setPaused(true);
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),         ++paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // stop() when paused.
    video.stop();
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),        ++stopped);

    // setPaused(true) when stopped.
    video.setPaused(true);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // stop() when stopped and paused.
    video.stop();
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(),   playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // pause() when stopped.
    video.pause();
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),        ++started);
    QCOMPARE(pausedSpy.count(),         ++paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPlaying(false) when paused.
    video.setPlaying(false);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),        ++stopped);

    // pause() when stopped and paused.
    video.pause();
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PausedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),        ++started);
    QCOMPARE(pausedSpy.count(),         ++paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);

    // setPlaying(false) when paused.
    video.setPlaying(false);
    QCOMPARE(video.isPlaying(), false);
    QCOMPARE(video.isPaused(), true);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::StoppedState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),    pausedChanged);
    QCOMPARE(startedSpy.count(),          started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),        ++stopped);

    // play() when stopped and paused.
    video.play();
    QCOMPARE(video.isPlaying(), true);
    QCOMPARE(video.isPaused(), false);
    QCOMPARE(provider.playerControl()->state(), QMediaPlayer::PlayingState);
    QCOMPARE(playingChangedSpy.count(), ++playingChanged);
    QCOMPARE(pausedChangedSpy.count(),  ++pausedChanged);
    QCOMPARE(startedSpy.count(),        ++started);
    QCOMPARE(pausedSpy.count(),           paused);
    QCOMPARE(resumedSpy.count(),          resumed);
    QCOMPARE(stoppedSpy.count(),          stopped);
}

void tst_QDeclarativeVideo::error()
{
    const QString errorString = QLatin1String("Failed to open device.");

    QtTestMediaServiceProvider provider;
    QDeclarativeVideo video;
    video.classBegin();
    video.componentComplete();

    QSignalSpy errorSpy(&video, SIGNAL(error(QDeclarativeVideo::Error,QString)));
    QSignalSpy errorChangedSpy(&video, SIGNAL(errorChanged()));

    QCOMPARE(video.error(), QDeclarativeVideo::NoError);
    QCOMPARE(video.errorString(), QString());

    provider.playerControl()->emitError(QMediaPlayer::ResourceError, errorString);

    QCOMPARE(video.error(), QDeclarativeVideo::ResourceError);
    QCOMPARE(video.errorString(), errorString);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorChangedSpy.count(), 1);

    // Changing the source resets the error properties.
    video.setSource(QUrl("http://example.com"));
    QCOMPARE(video.error(), QDeclarativeVideo::NoError);
    QCOMPARE(video.errorString(), QString());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorChangedSpy.count(), 2);

    // But isn't noisy.
    video.setSource(QUrl("file:///file/path"));
    QCOMPARE(video.error(), QDeclarativeVideo::NoError);
    QCOMPARE(video.errorString(), QString());
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(errorChangedSpy.count(), 2);
}


void tst_QDeclarativeVideo::hasAudio()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeVideo video;
    video.classBegin();
    video.componentComplete();

    QSignalSpy spy(&video, SIGNAL(hasAudioChanged()));

    QCOMPARE(video.hasAudio(), false);

    provider.playerControl()->setAudioAvailable(true);
    QCOMPARE(video.hasAudio(), true);
    QCOMPARE(spy.count(), 1);

    provider.playerControl()->setAudioAvailable(true);
    QCOMPARE(video.hasAudio(), true);
    QCOMPARE(spy.count(), 2);

    provider.playerControl()->setAudioAvailable(false);
    QCOMPARE(video.hasAudio(), false);
    QCOMPARE(spy.count(), 3);
}

void tst_QDeclarativeVideo::hasVideo()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeVideo video;
    video.classBegin();
    video.componentComplete();

    QSignalSpy spy(&video, SIGNAL(hasVideoChanged()));

    QCOMPARE(video.hasVideo(), false);

    provider.playerControl()->setVideoAvailable(true);
    QCOMPARE(video.hasVideo(), true);
    QCOMPARE(spy.count(), 1);

    provider.playerControl()->setVideoAvailable(true);
    QCOMPARE(video.hasVideo(), true);
    QCOMPARE(spy.count(), 2);

    provider.playerControl()->setVideoAvailable(false);
    QCOMPARE(video.hasVideo(), false);
    QCOMPARE(spy.count(), 3);
}

void tst_QDeclarativeVideo::fillMode()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeVideo video;
    video.classBegin();
    video.componentComplete();

    QList<QGraphicsItem *> children = video.childItems();
    QCOMPARE(children.count(), 1);
    QGraphicsVideoItem *videoItem = qgraphicsitem_cast<QGraphicsVideoItem *>(children.first());
    QVERIFY(videoItem != 0);

    QCOMPARE(video.fillMode(), QDeclarativeVideo::PreserveAspectFit);

    video.setFillMode(QDeclarativeVideo::PreserveAspectCrop);
    QCOMPARE(video.fillMode(), QDeclarativeVideo::PreserveAspectCrop);
    QCOMPARE(videoItem->aspectRatioMode(), Qt::KeepAspectRatioByExpanding);

    video.setFillMode(QDeclarativeVideo::Stretch);
    QCOMPARE(video.fillMode(), QDeclarativeVideo::Stretch);
    QCOMPARE(videoItem->aspectRatioMode(), Qt::IgnoreAspectRatio);

    video.setFillMode(QDeclarativeVideo::PreserveAspectFit);
    QCOMPARE(video.fillMode(), QDeclarativeVideo::PreserveAspectFit);
    QCOMPARE(videoItem->aspectRatioMode(), Qt::KeepAspectRatio);
}

void tst_QDeclarativeVideo::geometry()
{
    QtTestMediaServiceProvider provider;
    QDeclarativeVideo video;
    video.classBegin();
    video.componentComplete();

    QList<QGraphicsItem *> children = video.childItems();
    QCOMPARE(children.count(), 1);
    QGraphicsVideoItem *videoItem = qgraphicsitem_cast<QGraphicsVideoItem *>(children.first());
    QVERIFY(videoItem != 0);

    {   // Surface setup is deferred until after the first paint.
        QImage image(320, 240, QImage::Format_RGB32);
        QPainter painter(&image);

        videoItem->paint(&painter, 0);
    }

    QAbstractVideoSurface *surface = provider.rendererControl()->surface();

    //video item can use overlay, QAbstractVideoSurface is not used than.
    if (surface) {
        QVideoSurfaceFormat format(QSize(640, 480), QVideoFrame::Format_RGB32);

        QVERIFY(surface->start(format));
        QCoreApplication::processEvents();

        QCOMPARE(video.implicitWidth(), qreal(640));
        QCOMPARE(video.implicitHeight(), qreal(480));
    }

    video.setWidth(560);
    video.setHeight(328);

    QCOMPARE(videoItem->size().width(), qreal(560));
    QCOMPARE(videoItem->size().height(), qreal(328));
}

QTEST_MAIN(tst_QDeclarativeVideo)

#include "tst_qdeclarativevideo.moc"
