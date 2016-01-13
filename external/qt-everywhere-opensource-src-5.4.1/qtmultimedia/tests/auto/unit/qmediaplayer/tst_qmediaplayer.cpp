/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>

#include <QtCore/qdebug.h>
#include <QtCore/qbuffer.h>
#include <QtNetwork/qnetworkconfiguration.h>
#include <QtNetwork/qnetworkconfigmanager.h>

#include <qabstractvideosurface.h>
#include <qmediaplayer.h>
#include <qmediaplayercontrol.h>
#include <qmediaplaylist.h>
#include <qmediaservice.h>
#include <qmediastreamscontrol.h>
#include <qmedianetworkaccesscontrol.h>
#include <qvideorenderercontrol.h>

#include "mockmediaserviceprovider.h"
#include "mockmediaplayerservice.h"
#include "mockvideosurface.h"

QT_USE_NAMESPACE

class AutoConnection
{
public:
    AutoConnection(QObject *sender, const char *signal, QObject *receiver, const char *method)
            : sender(sender), signal(signal), receiver(receiver), method(method)
    {
        QObject::connect(sender, signal, receiver, method);
    }

    ~AutoConnection()
    {
        QObject::disconnect(sender, signal, receiver, method);
    }

private:
    QObject *sender;
    const char *signal;
    QObject *receiver;
    const char *method;
};

class tst_QMediaPlayer: public QObject
{
    Q_OBJECT

public slots:
    void initTestCase_data();
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void testNullService();
    void testValid();
    void testMedia();
    void testDuration();
    void testPosition();
    void testVolume();
    void testMuted();
    void testIsAvailable();
    void testVideoAvailable();
    void testBufferStatus();
    void testSeekable();
    void testPlaybackRate();
    void testError();
    void testErrorString();
    void testService();
    void testPlay();
    void testPause();
    void testStop();
    void testMediaStatus();
    void testPlaylist();
    void testNetworkAccess();
    void testSetVideoOutput();
    void testSetVideoOutputNoService();
    void testSetVideoOutputNoControl();
    void testSetVideoOutputDestruction();
    void testPositionPropertyWatch();
    void debugEnums();
    void testPlayerFlags();
    void testDestructor();
    void testSupportedMimeTypes();

private:
    MockMediaServiceProvider *mockProvider;
    MockMediaPlayerService  *mockService;
    QMediaPlayer *player;
};

void tst_QMediaPlayer::initTestCase_data()
{
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QMediaPlayer::State>("state");
    QTest::addColumn<QMediaPlayer::MediaStatus>("status");
    QTest::addColumn<QMediaContent>("mediaContent");
    QTest::addColumn<qint64>("duration");
    QTest::addColumn<qint64>("position");
    QTest::addColumn<bool>("seekable");
    QTest::addColumn<int>("volume");
    QTest::addColumn<bool>("muted");
    QTest::addColumn<bool>("videoAvailable");
    QTest::addColumn<int>("bufferStatus");
    QTest::addColumn<qreal>("playbackRate");
    QTest::addColumn<QMediaPlayer::Error>("error");
    QTest::addColumn<QString>("errorString");

    QTest::newRow("invalid") << false << QMediaPlayer::StoppedState << QMediaPlayer::UnknownMediaStatus <<
                                QMediaContent() << qint64(0) << qint64(0) << false << 0 << false << false << 0 <<
                                qreal(0) << QMediaPlayer::NoError << QString();
    QTest::newRow("valid+null") << true << QMediaPlayer::StoppedState << QMediaPlayer::UnknownMediaStatus <<
                                QMediaContent() << qint64(0) << qint64(0) << false << 0 << false << false << 50 <<
                                qreal(0) << QMediaPlayer::NoError << QString();
    QTest::newRow("valid+content+stopped") << true << QMediaPlayer::StoppedState << QMediaPlayer::UnknownMediaStatus <<
                                QMediaContent(QUrl("file:///some.mp3")) << qint64(0) << qint64(0) << false << 50 << false << false << 0 <<
                                qreal(1) << QMediaPlayer::NoError << QString();
    QTest::newRow("valid+content+playing") << true << QMediaPlayer::PlayingState << QMediaPlayer::LoadedMedia <<
                                QMediaContent(QUrl("file:///some.mp3")) << qint64(10000) << qint64(10) << true << 50 << true << false << 0 <<
                                qreal(1) << QMediaPlayer::NoError << QString();
    QTest::newRow("valid+content+paused") << true << QMediaPlayer::PausedState << QMediaPlayer::LoadedMedia <<
                                QMediaContent(QUrl("file:///some.mp3")) << qint64(10000) << qint64(10) << true << 50 << true << false << 0 <<
                                qreal(1)  << QMediaPlayer::NoError << QString();
    QTest::newRow("valud+streaming") << true << QMediaPlayer::PlayingState << QMediaPlayer::LoadedMedia <<
                                QMediaContent(QUrl("http://example.com/stream")) << qint64(10000) << qint64(10000) << false << 50 << false << true << 0 <<
                                qreal(1)  << QMediaPlayer::NoError << QString();
    QTest::newRow("valid+error") << true << QMediaPlayer::StoppedState << QMediaPlayer::UnknownMediaStatus <<
                                QMediaContent(QUrl("http://example.com/stream")) << qint64(0) << qint64(0) << false << 50 << false << false << 0 <<
                                qreal(0) << QMediaPlayer::ResourceError << QString("Resource unavailable");
}

void tst_QMediaPlayer::initTestCase()
{
    qRegisterMetaType<QMediaPlayer::State>("QMediaPlayer::State");
    qRegisterMetaType<QMediaPlayer::Error>("QMediaPlayer::Error");
    qRegisterMetaType<QMediaPlayer::MediaStatus>("QMediaPlayer::MediaStatus");
    qRegisterMetaType<QMediaContent>("QMediaContent");
}

void tst_QMediaPlayer::cleanupTestCase()
{
}

void tst_QMediaPlayer::init()
{
    mockService = new MockMediaPlayerService;
    mockProvider = new MockMediaServiceProvider(mockService);
    QMediaServiceProvider::setDefaultServiceProvider(mockProvider);

    player = new QMediaPlayer;
}

void tst_QMediaPlayer::cleanup()
{
    delete player;
    delete mockProvider;
    delete mockService;
}

void tst_QMediaPlayer::testNullService()
{
    mockProvider->service = 0;
    QMediaPlayer player;

    const QIODevice *nullDevice = 0;

    QCOMPARE(player.media(), QMediaContent());
    QCOMPARE(player.mediaStream(), nullDevice);
    QCOMPARE(player.state(), QMediaPlayer::StoppedState);
    QCOMPARE(player.mediaStatus(), QMediaPlayer::UnknownMediaStatus);
    QCOMPARE(player.duration(), qint64(-1));
    QCOMPARE(player.position(), qint64(0));
    QCOMPARE(player.volume(), 0);
    QCOMPARE(player.isMuted(), false);
    QCOMPARE(player.isVideoAvailable(), false);
    QCOMPARE(player.bufferStatus(), 0);
    QCOMPARE(player.isSeekable(), false);
    QCOMPARE(player.playbackRate(), qreal(0));
    QCOMPARE(player.error(), QMediaPlayer::ServiceMissingError);
    QCOMPARE(player.isAvailable(), false);
    QCOMPARE(player.availability(), QMultimedia::ServiceMissing);

    {
        QFETCH_GLOBAL(QMediaContent, mediaContent);

        QSignalSpy spy(&player, SIGNAL(currentMediaChanged(QMediaContent)));
        QFile file;

        player.setMedia(mediaContent, &file);
        QCOMPARE(player.currentMedia(), QMediaContent());
        QCOMPARE(player.media(), mediaContent);
        QCOMPARE(player.mediaStream(), nullDevice);
        QCOMPARE(spy.count(), 0);
    } {
        QSignalSpy stateSpy(&player, SIGNAL(stateChanged(QMediaPlayer::State)));
        QSignalSpy statusSpy(&player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)));

        player.play();
        QCOMPARE(player.state(), QMediaPlayer::StoppedState);
        QCOMPARE(player.mediaStatus(), QMediaPlayer::UnknownMediaStatus);
        QCOMPARE(stateSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 0);

        player.pause();
        QCOMPARE(player.state(), QMediaPlayer::StoppedState);
        QCOMPARE(player.mediaStatus(), QMediaPlayer::UnknownMediaStatus);
        QCOMPARE(stateSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 0);

        player.stop();
        QCOMPARE(player.state(), QMediaPlayer::StoppedState);
        QCOMPARE(player.mediaStatus(), QMediaPlayer::UnknownMediaStatus);
        QCOMPARE(stateSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 0);
    } {
        QFETCH_GLOBAL(int, volume);
        QFETCH_GLOBAL(bool, muted);

        QSignalSpy volumeSpy(&player, SIGNAL(volumeChanged(int)));
        QSignalSpy mutingSpy(&player, SIGNAL(mutedChanged(bool)));

        player.setVolume(volume);
        QCOMPARE(player.volume(), 0);
        QCOMPARE(volumeSpy.count(), 0);

        player.setMuted(muted);
        QCOMPARE(player.isMuted(), false);
        QCOMPARE(mutingSpy.count(), 0);
    } {
        QFETCH_GLOBAL(qint64, position);

        QSignalSpy spy(&player, SIGNAL(positionChanged(qint64)));

        player.setPosition(position);
        QCOMPARE(player.position(), qint64(0));
        QCOMPARE(spy.count(), 0);
    } {
        QFETCH_GLOBAL(qreal, playbackRate);

        QSignalSpy spy(&player, SIGNAL(playbackRateChanged(qreal)));

        player.setPlaybackRate(playbackRate);
        QCOMPARE(player.playbackRate(), qreal(0));
        QCOMPARE(spy.count(), 0);
    } {
        QMediaPlaylist playlist;
        player.setPlaylist(&playlist);

        QSignalSpy mediaSpy(&player, SIGNAL(mediaChanged(QMediaContent)));
        QSignalSpy statusSpy(&player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)));

        playlist.addMedia(QUrl("http://example.com/stream"));
        playlist.addMedia(QUrl("file:///some.mp3"));

        playlist.setCurrentIndex(0);
        QCOMPARE(playlist.currentIndex(), 0);
        QCOMPARE(player.currentMedia(), QMediaContent());
        QCOMPARE(player.media().playlist(), &playlist);
        QCOMPARE(mediaSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 0);

        playlist.next();
        QCOMPARE(playlist.currentIndex(), 1);
        QCOMPARE(player.currentMedia(), QMediaContent());
        QCOMPARE(player.media().playlist(), &playlist);
        QCOMPARE(mediaSpy.count(), 0);
        QCOMPARE(statusSpy.count(), 0);
    }
}

void tst_QMediaPlayer::testValid()
{
    /*
    QFETCH_GLOBAL(bool, valid);

    mockService->setIsValid(valid);
    QCOMPARE(player->isValid(), valid);
    */
}

void tst_QMediaPlayer::testMedia()
{
    QFETCH_GLOBAL(QMediaContent, mediaContent);

    mockService->setMedia(mediaContent);
    QCOMPARE(player->currentMedia(), mediaContent);

    QBuffer stream;
    player->setMedia(mediaContent, &stream);
    QCOMPARE(player->currentMedia(), mediaContent);
    QCOMPARE((QBuffer*)player->mediaStream(), &stream);
}

void tst_QMediaPlayer::testDuration()
{
    QFETCH_GLOBAL(qint64, duration);

    mockService->setDuration(duration);
    QVERIFY(player->duration() == duration);
}

void tst_QMediaPlayer::testPosition()
{
    QFETCH_GLOBAL(bool, valid);
    QFETCH_GLOBAL(bool, seekable);
    QFETCH_GLOBAL(qint64, position);
    QFETCH_GLOBAL(qint64, duration);

    mockService->setIsValid(valid);
    mockService->setSeekable(seekable);
    mockService->setPosition(position);
    mockService->setDuration(duration);
    QVERIFY(player->isSeekable() == seekable);
    QVERIFY(player->position() == position);
    QVERIFY(player->duration() == duration);

    if (seekable) {
        { QSignalSpy spy(player, SIGNAL(positionChanged(qint64)));
        player->setPosition(position);
        QCOMPARE(player->position(), position);
        QCOMPARE(spy.count(), 0); }

        mockService->setPosition(position);
        { QSignalSpy spy(player, SIGNAL(positionChanged(qint64)));
        player->setPosition(0);
        QCOMPARE(player->position(), qint64(0));
        QCOMPARE(spy.count(), position == 0 ? 0 : 1); }

        mockService->setPosition(position);
        { QSignalSpy spy(player, SIGNAL(positionChanged(qint64)));
        player->setPosition(duration);
        QCOMPARE(player->position(), duration);
        QCOMPARE(spy.count(), position == duration ? 0 : 1); }

        mockService->setPosition(position);
        { QSignalSpy spy(player, SIGNAL(positionChanged(qint64)));
        player->setPosition(-1);
        QCOMPARE(player->position(), qint64(0));
        QCOMPARE(spy.count(), position == 0 ? 0 : 1); }

    }
    else {
        QSignalSpy spy(player, SIGNAL(positionChanged(qint64)));
        player->setPosition(position);

        QCOMPARE(player->position(), position);
        QCOMPARE(spy.count(), 0);
    }
}

void tst_QMediaPlayer::testVolume()
{
    QFETCH_GLOBAL(bool, valid);
    QFETCH_GLOBAL(int, volume);

    mockService->setVolume(volume);
    QVERIFY(player->volume() == volume);

    if (valid) {
        { QSignalSpy spy(player, SIGNAL(volumeChanged(int)));
        player->setVolume(10);
        QCOMPARE(player->volume(), 10);
        QCOMPARE(spy.count(), 1); }

        { QSignalSpy spy(player, SIGNAL(volumeChanged(int)));
        player->setVolume(-1000);
        QCOMPARE(player->volume(), 0);
        QCOMPARE(spy.count(), 1); }

        { QSignalSpy spy(player, SIGNAL(volumeChanged(int)));
        player->setVolume(100);
        QCOMPARE(player->volume(), 100);
        QCOMPARE(spy.count(), 1); }

        { QSignalSpy spy(player, SIGNAL(volumeChanged(int)));
        player->setVolume(1000);
        QCOMPARE(player->volume(), 100);
        QCOMPARE(spy.count(), 0); }
    }
}

void tst_QMediaPlayer::testMuted()
{
    QFETCH_GLOBAL(bool, valid);
    QFETCH_GLOBAL(bool, muted);
    QFETCH_GLOBAL(int, volume);

    if (valid) {
        mockService->setMuted(muted);
        mockService->setVolume(volume);
        QVERIFY(player->isMuted() == muted);

        QSignalSpy spy(player, SIGNAL(mutedChanged(bool)));
        player->setMuted(!muted);
        QCOMPARE(player->isMuted(), !muted);
        QCOMPARE(player->volume(), volume);
        QCOMPARE(spy.count(), 1);
    }
}

void tst_QMediaPlayer::testVideoAvailable()
{
    QFETCH_GLOBAL(bool, videoAvailable);

    mockService->setVideoAvailable(videoAvailable);
    QVERIFY(player->isVideoAvailable() == videoAvailable);
}

void tst_QMediaPlayer::testBufferStatus()
{
    QFETCH_GLOBAL(int, bufferStatus);

    mockService->setBufferStatus(bufferStatus);
    QVERIFY(player->bufferStatus() == bufferStatus);
}

void tst_QMediaPlayer::testSeekable()
{
    QFETCH_GLOBAL(bool, seekable);

    mockService->setSeekable(seekable);
    QVERIFY(player->isSeekable() == seekable);
}

void tst_QMediaPlayer::testPlaybackRate()
{
    QFETCH_GLOBAL(bool, valid);
    QFETCH_GLOBAL(qreal, playbackRate);

    if (valid) {
        mockService->setPlaybackRate(playbackRate);
        QVERIFY(player->playbackRate() == playbackRate);

        QSignalSpy spy(player, SIGNAL(playbackRateChanged(qreal)));
        player->setPlaybackRate(playbackRate + 0.5f);
        QCOMPARE(player->playbackRate(), playbackRate + 0.5f);
        QCOMPARE(spy.count(), 1);
    }
}

void tst_QMediaPlayer::testError()
{
    QFETCH_GLOBAL(QMediaPlayer::Error, error);

    mockService->setError(error);
    QVERIFY(player->error() == error);
}

void tst_QMediaPlayer::testErrorString()
{
    QFETCH_GLOBAL(QString, errorString);

    mockService->setErrorString(errorString);
    QVERIFY(player->errorString() == errorString);
}

void tst_QMediaPlayer::testIsAvailable()
{
    QCOMPARE(player->isAvailable(), true);
    QCOMPARE(player->availability(), QMultimedia::Available);
}

void tst_QMediaPlayer::testService()
{
    /*
    QFETCH_GLOBAL(bool, valid);

    mockService->setIsValid(valid);

    if (valid)
        QVERIFY(player->service() != 0);
    else
        QVERIFY(player->service() == 0);
        */
}

void tst_QMediaPlayer::testPlay()
{
    QFETCH_GLOBAL(bool, valid);
    QFETCH_GLOBAL(QMediaContent, mediaContent);
    QFETCH_GLOBAL(QMediaPlayer::State, state);

    mockService->setIsValid(valid);
    mockService->setState(state);
    mockService->setMedia(mediaContent);
    QVERIFY(player->state() == state);
    QVERIFY(player->currentMedia() == mediaContent);

    QSignalSpy spy(player, SIGNAL(stateChanged(QMediaPlayer::State)));

    player->play();

    if (!valid || mediaContent.isNull())  {
        QCOMPARE(player->state(), QMediaPlayer::StoppedState);
        QCOMPARE(spy.count(), 0);
    }
    else {
        QCOMPARE(player->state(), QMediaPlayer::PlayingState);
        QCOMPARE(spy.count(), state == QMediaPlayer::PlayingState ? 0 : 1);
    }
}

void tst_QMediaPlayer::testPause()
{
    QFETCH_GLOBAL(bool, valid);
    QFETCH_GLOBAL(QMediaContent, mediaContent);
    QFETCH_GLOBAL(QMediaPlayer::State, state);

    mockService->setIsValid(valid);
    mockService->setState(state);
    mockService->setMedia(mediaContent);
    QVERIFY(player->state() == state);
    QVERIFY(player->currentMedia() == mediaContent);

    QSignalSpy spy(player, SIGNAL(stateChanged(QMediaPlayer::State)));

    player->pause();

    if (!valid || mediaContent.isNull()) {
        QCOMPARE(player->state(), QMediaPlayer::StoppedState);
        QCOMPARE(spy.count(), 0);
    }
    else {
        QCOMPARE(player->state(), QMediaPlayer::PausedState);
        QCOMPARE(spy.count(), state == QMediaPlayer::PausedState ? 0 : 1);
    }
}

void tst_QMediaPlayer::testStop()
{
    QFETCH_GLOBAL(QMediaContent, mediaContent);
    QFETCH_GLOBAL(QMediaPlayer::State, state);

    mockService->setState(state);
    mockService->setMedia(mediaContent);
    QVERIFY(player->state() == state);
    QVERIFY(player->currentMedia() == mediaContent);

    QSignalSpy spy(player, SIGNAL(stateChanged(QMediaPlayer::State)));

    player->stop();

    if (mediaContent.isNull() || state == QMediaPlayer::StoppedState) {
        QCOMPARE(player->state(), QMediaPlayer::StoppedState);
        QCOMPARE(spy.count(), 0);
    }
    else {
        QCOMPARE(player->state(), QMediaPlayer::StoppedState);
        QCOMPARE(spy.count(), 1);
    }
}

void tst_QMediaPlayer::testMediaStatus()
{
    QFETCH_GLOBAL(int, bufferStatus);
    int bufferSignals = 0;

    player->setNotifyInterval(10);

    mockService->setMediaStatus(QMediaPlayer::NoMedia);
    mockService->setBufferStatus(bufferStatus);

    AutoConnection connection(
            player, SIGNAL(bufferStatusChanged(int)),
            &QTestEventLoop::instance(), SLOT(exitLoop()));

    QSignalSpy statusSpy(player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)));
    QSignalSpy bufferSpy(player, SIGNAL(bufferStatusChanged(int)));

    QCOMPARE(player->mediaStatus(), QMediaPlayer::NoMedia);

    mockService->setMediaStatus(QMediaPlayer::LoadingMedia);
    QCOMPARE(player->mediaStatus(), QMediaPlayer::LoadingMedia);
    QCOMPARE(statusSpy.count(), 1);

    QCOMPARE(qvariant_cast<QMediaPlayer::MediaStatus>(statusSpy.last().value(0)),
             QMediaPlayer::LoadingMedia);

    mockService->setMediaStatus(QMediaPlayer::LoadedMedia);
    QCOMPARE(player->mediaStatus(), QMediaPlayer::LoadedMedia);
    QCOMPARE(statusSpy.count(), 2);

    QCOMPARE(qvariant_cast<QMediaPlayer::MediaStatus>(statusSpy.last().value(0)),
             QMediaPlayer::LoadedMedia);

    // Verify the bufferStatusChanged() signal isn't being emitted.
    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(bufferSpy.count(), 0);

    mockService->setMediaStatus(QMediaPlayer::StalledMedia);
    QCOMPARE(player->mediaStatus(), QMediaPlayer::StalledMedia);
    QCOMPARE(statusSpy.count(), 3);

    QCOMPARE(qvariant_cast<QMediaPlayer::MediaStatus>(statusSpy.last().value(0)),
             QMediaPlayer::StalledMedia);

    // Verify the bufferStatusChanged() signal is being emitted.
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(bufferSpy.count() > bufferSignals);
    QCOMPARE(bufferSpy.last().value(0).toInt(), bufferStatus);
    bufferSignals = bufferSpy.count();

    mockService->setMediaStatus(QMediaPlayer::BufferingMedia);
    QCOMPARE(player->mediaStatus(), QMediaPlayer::BufferingMedia);
    QCOMPARE(statusSpy.count(), 4);

    QCOMPARE(qvariant_cast<QMediaPlayer::MediaStatus>(statusSpy.last().value(0)),
             QMediaPlayer::BufferingMedia);

    // Verify the bufferStatusChanged() signal is being emitted.
    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(bufferSpy.count() > bufferSignals);
    QCOMPARE(bufferSpy.last().value(0).toInt(), bufferStatus);
    bufferSignals = bufferSpy.count();

    mockService->setMediaStatus(QMediaPlayer::BufferedMedia);
    QCOMPARE(player->mediaStatus(), QMediaPlayer::BufferedMedia);
    QCOMPARE(statusSpy.count(), 5);

    QCOMPARE(qvariant_cast<QMediaPlayer::MediaStatus>(statusSpy.last().value(0)),
             QMediaPlayer::BufferedMedia);

    // Verify the bufferStatusChanged() signal isn't being emitted.
    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(bufferSpy.count(), bufferSignals);

    mockService->setMediaStatus(QMediaPlayer::EndOfMedia);
    QCOMPARE(player->mediaStatus(), QMediaPlayer::EndOfMedia);
    QCOMPARE(statusSpy.count(), 6);

    QCOMPARE(qvariant_cast<QMediaPlayer::MediaStatus>(statusSpy.last().value(0)),
             QMediaPlayer::EndOfMedia);
}

void tst_QMediaPlayer::testPlaylist()
{
    QMediaContent content0(QUrl(QLatin1String("test://audio/song1.mp3")));
    QMediaContent content1(QUrl(QLatin1String("test://audio/song2.mp3")));
    QMediaContent content2(QUrl(QLatin1String("test://video/movie1.mp4")));
    QMediaContent content3(QUrl(QLatin1String("test://video/movie2.mp4")));
    QMediaContent content4(QUrl(QLatin1String("test://image/photo.jpg")));

    mockService->setIsValid(true);
    mockService->setState(QMediaPlayer::StoppedState, QMediaPlayer::NoMedia);

    QMediaPlaylist *playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);
    QCOMPARE(player->media().playlist(), playlist);

    QSignalSpy stateSpy(player, SIGNAL(stateChanged(QMediaPlayer::State)));
    QSignalSpy mediaSpy(player, SIGNAL(currentMediaChanged(QMediaContent)));

    // Test the player does nothing with an empty playlist attached.
    player->play();
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(player->currentMedia(), QMediaContent());
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(mediaSpy.count(), 0);

    playlist->addMedia(content0);
    playlist->addMedia(content1);
    playlist->addMedia(content2);
    playlist->addMedia(content3);

    // Test changing the playlist position, changes the current media, but not the playing state.
    playlist->setCurrentIndex(1);
    QCOMPARE(player->currentMedia(), content1);
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(mediaSpy.count(), 1);

    // Test playing starts with the current media.
    player->play();
    QCOMPARE(player->currentMedia(), content1);
    QCOMPARE(player->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(mediaSpy.count(), 1);

    // Test pausing doesn't change the current media.
    player->pause();
    QCOMPARE(player->currentMedia(), content1);
    QCOMPARE(player->state(), QMediaPlayer::PausedState);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(mediaSpy.count(), 1);

    // Test stopping doesn't change the current media.
    player->stop();
    QCOMPARE(player->currentMedia(), content1);
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateSpy.count(), 3);
    QCOMPARE(mediaSpy.count(), 1);

    // Test when the player service reaches the end of the current media, the player moves onto
    // the next item without stopping.
    player->play();
    QCOMPARE(player->currentMedia(), content1);
    QCOMPARE(player->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateSpy.count(), 4);
    QCOMPARE(mediaSpy.count(), 1);

    mockService->setState(QMediaPlayer::StoppedState, QMediaPlayer::EndOfMedia);
    QCOMPARE(player->currentMedia(), content2);
    QCOMPARE(player->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateSpy.count(), 4);
    QCOMPARE(mediaSpy.count(), 2);

    // Test skipping the current media doesn't change the state.
    playlist->next();
    QCOMPARE(player->currentMedia(), content3);
    QCOMPARE(player->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateSpy.count(), 4);
    QCOMPARE(mediaSpy.count(), 3);

    // Test changing the current media while paused doesn't change the state.
    player->pause();
    mockService->setMediaStatus(QMediaPlayer::BufferedMedia);
    QCOMPARE(player->currentMedia(), content3);
    QCOMPARE(player->state(), QMediaPlayer::PausedState);
    QCOMPARE(stateSpy.count(), 5);
    QCOMPARE(mediaSpy.count(), 3);

    playlist->previous();
    QCOMPARE(player->currentMedia(), content2);
    QCOMPARE(player->state(), QMediaPlayer::PausedState);
    QCOMPARE(stateSpy.count(), 5);
    QCOMPARE(mediaSpy.count(), 4);

    // Test changing the current media while stopped doesn't change the state.
    player->stop();
    mockService->setMediaStatus(QMediaPlayer::LoadedMedia);
    QCOMPARE(player->currentMedia(), content2);
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateSpy.count(), 6);
    QCOMPARE(mediaSpy.count(), 4);

    playlist->next();
    QCOMPARE(player->currentMedia(), content3);
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateSpy.count(), 6);
    QCOMPARE(mediaSpy.count(), 5);

    // Test the player is stopped and the current media cleared when it reaches the end of the last
    // item in the playlist.
    player->play();
    QCOMPARE(player->currentMedia(), content3);
    QCOMPARE(player->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateSpy.count(), 7);
    QCOMPARE(mediaSpy.count(), 5);

    // Double up the signals to ensure some noise doesn't destabalize things.
    mockService->setState(QMediaPlayer::StoppedState, QMediaPlayer::EndOfMedia);
    mockService->setState(QMediaPlayer::StoppedState, QMediaPlayer::EndOfMedia);
    QCOMPARE(player->currentMedia(), QMediaContent());
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateSpy.count(), 8);
    QCOMPARE(mediaSpy.count(), 6);

    // Test starts playing from the start of the playlist if there is no current media selected.
    player->play();
    QCOMPARE(player->currentMedia(), content0);
    QCOMPARE(player->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateSpy.count(), 9);
    // one notification is for playlist and another is for the first media in the playlist
    QCOMPARE(mediaSpy.count(), 8);

    // Test deleting the playlist stops the player and clears the media it set.
    delete playlist;
    QCOMPARE(player->currentMedia(), QMediaContent());
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateSpy.count(), 10);
    QCOMPARE(mediaSpy.count(), 9);

    // Test the player works as normal with the playlist removed.
    player->play();
    QCOMPARE(player->currentMedia(), QMediaContent());
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);
    QCOMPARE(stateSpy.count(), 10);
    QCOMPARE(mediaSpy.count(), 9);

    player->setMedia(content1);
    player->play();

    QCOMPARE(player->currentMedia(), content1);
    QCOMPARE(player->state(), QMediaPlayer::PlayingState);
    QCOMPARE(stateSpy.count(), 11);
    QCOMPARE(mediaSpy.count(), 10);

    // Test the player can bind to playlist again
    playlist = new QMediaPlaylist;
    player->setPlaylist(playlist);

    QCOMPARE(player->currentMedia(), QMediaContent());
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);

    playlist->addMedia(content0);
    playlist->addMedia(content1);
    playlist->addMedia(content2);
    playlist->addMedia(content3);

    playlist->setCurrentIndex(1);
    QCOMPARE(player->currentMedia(), content1);
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);

    // Test attaching the new playlist,
    // player should detach the current one
    QMediaPlaylist *playlist2 = new QMediaPlaylist;
    playlist2->addMedia(content1);
    playlist2->addMedia(content2);
    playlist2->addMedia(content3);
    playlist2->setCurrentIndex(2);

    player->play();
    player->setPlaylist(playlist2);
    QCOMPARE(player->currentMedia(), playlist2->currentMedia());
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);

    playlist2->setCurrentIndex(1);
    QCOMPARE(player->currentMedia(), playlist2->currentMedia());

    {
        QMediaPlaylist playlist;
        playlist.addMedia(content1);
        playlist.addMedia(content2);
        playlist.addMedia(content3);
        playlist.setCurrentIndex(1);

        // playlist resets to the first item
        player->setPlaylist(&playlist);
        QCOMPARE(player->playlist(), &playlist);
        QCOMPARE(player->currentMedia(), content1);
    } //playlist should be detached now

    QVERIFY(player->playlist() == 0);
    QCOMPARE(player->currentMedia(), QMediaContent());

    // Test when the player service encounters an invalid media, the player moves onto
    // the next item without stopping
    {
        QSignalSpy ss(player, SIGNAL(stateChanged(QMediaPlayer::State)));
        QSignalSpy ms(player, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)));

        // playlist index is set to 0 when it is loaded into media player
        player->setPlaylist(playlist);
        player->play();
        QCOMPARE(ss.count(), 1);

        mockService->setState(QMediaPlayer::StoppedState, QMediaPlayer::InvalidMedia);
        QCOMPARE(player->state(), QMediaPlayer::PlayingState);
        QCOMPARE(player->mediaStatus(), QMediaPlayer::InvalidMedia);
        QCOMPARE(ss.count(), 1);
        QCOMPARE(ms.count(), 1);

        // NOTE: status should begin transitioning through to BufferedMedia.
        QCOMPARE(player->currentMedia(), content1);
    }

    delete playlist;
    delete playlist2;
}

void tst_QMediaPlayer::testPlayerFlags()
{
    MockMediaServiceProvider provider(0, true);
    QMediaPlayer::Flag flags = QMediaPlayer::LowLatency;

    QMediaServiceProviderHint::Feature feature;

    if (flags & QMediaPlayer::LowLatency)
    {
        /* if the flag is low latency set the low latency play back for the service provider */
        feature = QMediaServiceProviderHint::LowLatencyPlayback;
        const QByteArray service(Q_MEDIASERVICE_MEDIAPLAYER);
        const QMediaServiceProviderHint providerHint(feature);
        /* request service for the service provider */
        provider.requestService(service,providerHint);

        /* Constructs a SupportedFeatures  media service provider hint. */
        QMediaServiceProviderHint servicepro(feature);

        /* compare the flag value */
        QVERIFY(servicepro.features() == QMediaServiceProviderHint::LowLatencyPlayback);
    }

    /* The player is expected to play QIODevice based streams.
        If passed to QMediaPlayer  constructor,
        the service supporting streams playback will be chosen. */
    flags = QMediaPlayer::StreamPlayback;
    /* Construct a QMediaPlayer  that uses the playback service from provider,
        parented to parent  and with flags.*/

    if (flags & QMediaPlayer::StreamPlayback)
    {
        /* if the flag is stream play back set the stream play back for the service provider */
        feature = QMediaServiceProviderHint::StreamPlayback;
        const QByteArray service(Q_MEDIASERVICE_MEDIAPLAYER);
        const QMediaServiceProviderHint providerHint(feature);

        /* request service for the service provider */
        provider.requestService(service,providerHint);

        /* Constructs a SupportedFeatures media service provider hint. */
        QMediaServiceProviderHint servicepro(feature);

        /* compare the flag value */
        QVERIFY(servicepro.features() == QMediaServiceProviderHint::StreamPlayback);
    }
}

void tst_QMediaPlayer::testDestructor()
{
    //don't use the same service as tst_QMediaPlayer::player
    mockProvider->service = new MockMediaPlayerService;
    mockProvider->deleteServiceOnRelease = true;

    /* create an object for player */
    QMediaPlayer *victim = new QMediaPlayer;

    /* check whether the object is created */
    QVERIFY(victim);

    /* delete the instance (a crash is a failure :) */
    delete victim;

    //service is released
    QVERIFY(mockProvider->service == 0);

    mockProvider->deleteServiceOnRelease = false;
}

void tst_QMediaPlayer::testNetworkAccess()
{
    QNetworkConfigurationManager manager;
    QList<QNetworkConfiguration> configs = manager.allConfigurations();

    if (configs.count() >= 1) {
        QSignalSpy spy(player, SIGNAL(networkConfigurationChanged(QNetworkConfiguration)));
        int index = qFloor((configs.count())/2);
        player->setNetworkConfigurations(configs);
        mockService->selectCurrentConfiguration(configs.at(index));

        QVERIFY(spy.count() == 1);
        QList<QVariant> args = spy.takeFirst();
        QNetworkConfiguration config = args.at(0).value<QNetworkConfiguration>();
        QCOMPARE(config.identifier() , configs.at(index).identifier());
        QCOMPARE(player->currentNetworkConfiguration().identifier() , config.identifier());
    }

    // invalidate current network configuration
    QSignalSpy spy(player, SIGNAL(networkConfigurationChanged(QNetworkConfiguration)));
    mockService->selectCurrentConfiguration(QNetworkConfiguration());
    QVERIFY(spy.count() == 1);
    QList<QVariant> args = spy.takeFirst();
    QNetworkConfiguration config = args.at(0).value<QNetworkConfiguration>();
    QVERIFY(config.isValid() == false);
    QVERIFY(player->currentNetworkConfiguration().isValid() == false);
}

void tst_QMediaPlayer::testSetVideoOutput()
{
    MockVideoSurface surface;

    player->setVideoOutput(reinterpret_cast<QVideoWidget *>(0));
    player->setVideoOutput(reinterpret_cast<QGraphicsVideoItem *>(0));

    QCOMPARE(mockService->rendererRef, 0);

    player->setVideoOutput(&surface);
    QVERIFY(mockService->rendererControl->surface() == &surface);
    QCOMPARE(mockService->rendererRef, 1);

    player->setVideoOutput(reinterpret_cast<QAbstractVideoSurface *>(0));
    QVERIFY(mockService->rendererControl->surface() == 0);

    //rendererControl is released
    QCOMPARE(mockService->rendererRef, 0);

    player->setVideoOutput(&surface);
    QVERIFY(mockService->rendererControl->surface() == &surface);
    QCOMPARE(mockService->rendererRef, 1);

    player->setVideoOutput(reinterpret_cast<QVideoWidget *>(0));
    QVERIFY(mockService->rendererControl->surface() == 0);
    //rendererControl is released
    QCOMPARE(mockService->rendererRef, 0);

    player->setVideoOutput(&surface);
    QVERIFY(mockService->rendererControl->surface() == &surface);
    QCOMPARE(mockService->rendererRef, 1);
}


void tst_QMediaPlayer::testSetVideoOutputNoService()
{
    MockVideoSurface surface;

    MockMediaServiceProvider provider(0, true);
    QMediaServiceProvider::setDefaultServiceProvider(&provider);
    QMediaPlayer player;

    player.setVideoOutput(&surface);
    // Nothing we can verify here other than it doesn't assert.
}

void tst_QMediaPlayer::testSetVideoOutputNoControl()
{
    MockVideoSurface surface;

    MockMediaPlayerService service;
    service.rendererRef = 1;

    MockMediaServiceProvider provider(&service);
    QMediaServiceProvider::setDefaultServiceProvider(&provider);
    QMediaPlayer player;

    player.setVideoOutput(&surface);
    QVERIFY(service.rendererControl->surface() == 0);
}

void tst_QMediaPlayer::testSetVideoOutputDestruction()
{
    MockVideoSurface surface;
    {
        QMediaPlayer player;
        player.setVideoOutput(&surface);
        QVERIFY(mockService->rendererControl->surface() == &surface);
        QCOMPARE(mockService->rendererRef, 1);
    }
    QVERIFY(mockService->rendererControl->surface() == 0);
    QCOMPARE(mockService->rendererRef, 0);
}

void tst_QMediaPlayer::testPositionPropertyWatch()
{
    QMediaContent content0(QUrl(QLatin1String("test://audio/song1.mp3")));
    QMediaContent content1(QUrl(QLatin1String("test://audio/song2.mp3")));

    mockService->setIsValid(true);
    mockService->setState(QMediaPlayer::StoppedState, QMediaPlayer::NoMedia);

    QMediaPlaylist *playlist = new QMediaPlaylist;

    playlist->addMedia(content0);
    playlist->addMedia(content1);

    player->setPlaylist(playlist);
    player->setNotifyInterval(5);

    player->play();
    QSignalSpy positionSpy(player, SIGNAL(positionChanged(qint64)));
    playlist->next();
    QCOMPARE(player->state(), QMediaPlayer::PlayingState);
    QTRY_VERIFY(positionSpy.count() > 0);

    playlist->next();
    QCOMPARE(player->state(), QMediaPlayer::StoppedState);

    positionSpy.clear();
    QTRY_COMPARE(positionSpy.count(), 0);

    delete playlist;
}

void tst_QMediaPlayer::debugEnums()
{
    QTest::ignoreMessage(QtDebugMsg, "QMediaPlayer::PlayingState");
    qDebug() << QMediaPlayer::PlayingState;
    QTest::ignoreMessage(QtDebugMsg, "QMediaPlayer::NoMedia");
    qDebug() << QMediaPlayer::NoMedia;
    QTest::ignoreMessage(QtDebugMsg, "QMediaPlayer::NetworkError");
    qDebug() << QMediaPlayer::NetworkError;
}

void tst_QMediaPlayer::testSupportedMimeTypes()
{
    QStringList mimeList = QMediaPlayer::supportedMimeTypes(QMediaPlayer::LowLatency);

    // This is empty on some platforms, and not on others, so can't test something here at the moment.
}

QTEST_GUILESS_MAIN(tst_QMediaPlayer)
#include "tst_qmediaplayer.moc"
