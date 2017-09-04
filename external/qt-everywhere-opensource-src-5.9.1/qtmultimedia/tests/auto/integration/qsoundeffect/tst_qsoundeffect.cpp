/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <QtCore/qlocale.h>
#include <qaudiooutput.h>
#include <qaudiodeviceinfo.h>
#include <qaudio.h>
#include "qsoundeffect.h"

class tst_QSoundEffect : public QObject
{
    Q_OBJECT
public:
    tst_QSoundEffect(QObject* parent=0) : QObject(parent) {}

public slots:
    void init();
    void cleanup();

private slots:
    void initTestCase();
    void testSource();
    void testLooping();
    void testVolume();
    void testMuting();

    void testPlaying();
    void testStatus();

    void testDestroyWhilePlaying();
    void testDestroyWhileRestartPlaying();

    void testSetSourceWhileLoading();
    void testSetSourceWhilePlaying();
    void testSupportedMimeTypes();
    void testCorruptFile();

private:
    QSoundEffect* sound;
    QUrl url; // test.wav: pcm_s16le, 48000 Hz, stereo, s16
    QUrl url2; // test_tone.wav: pcm_s16le, 44100 Hz, mono
    QUrl urlCorrupted; // test_corrupted.wav: corrupted
};

void tst_QSoundEffect::init()
{
    sound->stop();
    sound->setSource(QUrl());
    sound->setLoopCount(1);
    sound->setVolume(1.0);
    sound->setMuted(false);
}

void tst_QSoundEffect::cleanup()
{
}

void tst_QSoundEffect::initTestCase()
{
    // Only perform tests if audio device exists
    QStringList mimeTypes = sound->supportedMimeTypes();
    if (mimeTypes.empty())
        QSKIP("No audio devices available");

    QString testFileName = QStringLiteral("test.wav");
    QString fullPath = QFINDTESTDATA(testFileName);
    QVERIFY2(!fullPath.isEmpty(), qPrintable(QStringLiteral("Unable to locate ") + testFileName));
    url = QUrl::fromLocalFile(fullPath);

    testFileName = QStringLiteral("test_tone.wav");
    fullPath = QFINDTESTDATA(testFileName);
    QVERIFY2(!fullPath.isEmpty(), qPrintable(QStringLiteral("Unable to locate ") + testFileName));
    url2 = QUrl::fromLocalFile(fullPath);

    testFileName = QStringLiteral("test_corrupted.wav");
    fullPath = QFINDTESTDATA(testFileName);
    QVERIFY2(!fullPath.isEmpty(), qPrintable(QStringLiteral("Unable to locate ") + testFileName));
    urlCorrupted = QUrl::fromLocalFile(fullPath);

    sound = new QSoundEffect(this);

    QVERIFY(sound->source().isEmpty());
    QVERIFY(sound->loopCount() == 1);
    QVERIFY(sound->volume() == 1);
    QVERIFY(sound->isMuted() == false);
}

void tst_QSoundEffect::testSource()
{
    QSignalSpy readSignal(sound, SIGNAL(sourceChanged()));

    sound->setSource(url);
    sound->setVolume(0.1f);

    QCOMPARE(sound->source(),url);
    QCOMPARE(readSignal.count(),1);

    QTestEventLoop::instance().enterLoop(1);
    sound->play();

    QTest::qWait(3000);
}

void tst_QSoundEffect::testLooping()
{
    sound->setSource(url);
    QTRY_COMPARE(sound->status(), QSoundEffect::Ready);

    QSignalSpy readSignal_Count(sound, SIGNAL(loopCountChanged()));
    QSignalSpy readSignal_Remaining(sound, SIGNAL(loopsRemainingChanged()));

    sound->setLoopCount(5);
    sound->setVolume(0.1f);
    QCOMPARE(sound->loopCount(), 5);
    QCOMPARE(readSignal_Count.count(), 1);
    QCOMPARE(sound->loopsRemaining(), 0);
    QCOMPARE(readSignal_Remaining.count(), 0);

    sound->play();
    QVERIFY(readSignal_Remaining.count() > 0);

    // test.wav is about 200ms, wait until it has finished playing 5 times
    QTestEventLoop::instance().enterLoop(3);

    QTRY_COMPARE(sound->loopsRemaining(), 0);
    QVERIFY(readSignal_Remaining.count() >= 6);
    QTRY_VERIFY(!sound->isPlaying());

    // QTBUG-36643 (setting the loop count while playing should work)
    {
        readSignal_Count.clear();
        readSignal_Remaining.clear();

        sound->setLoopCount(30);
        QCOMPARE(sound->loopCount(), 30);
        QCOMPARE(readSignal_Count.count(), 1);
        QCOMPARE(sound->loopsRemaining(), 0);
        QCOMPARE(readSignal_Remaining.count(), 0);

        sound->play();
        QVERIFY(readSignal_Remaining.count() > 0);

        // wait for the sound to be played several times
        QTRY_VERIFY(sound->loopsRemaining() <= 20);
        QVERIFY(readSignal_Remaining.count() >= 10);
        readSignal_Count.clear();
        readSignal_Remaining.clear();

        // change the loop count while playing
        sound->setLoopCount(5);
        QCOMPARE(sound->loopCount(), 5);
        QCOMPARE(readSignal_Count.count(), 1);
        QCOMPARE(sound->loopsRemaining(), 5);
        QCOMPARE(readSignal_Remaining.count(), 1);

        // wait for all the loops to be completed
        QTRY_COMPARE(sound->loopsRemaining(), 0);
        QTRY_VERIFY(readSignal_Remaining.count() >= 6);
        QTRY_VERIFY(!sound->isPlaying());
    }

    {
        readSignal_Count.clear();
        readSignal_Remaining.clear();

        sound->setLoopCount(QSoundEffect::Infinite);
        QCOMPARE(sound->loopCount(), int(QSoundEffect::Infinite));
        QCOMPARE(readSignal_Count.count(), 1);
        QCOMPARE(sound->loopsRemaining(), 0);
        QCOMPARE(readSignal_Remaining.count(), 0);

        sound->play();
        QTRY_COMPARE(sound->loopsRemaining(), int(QSoundEffect::Infinite));
        QCOMPARE(readSignal_Remaining.count(), 1);

        QTest::qWait(1500);
        QVERIFY(sound->isPlaying());
        readSignal_Count.clear();
        readSignal_Remaining.clear();

        // Setting the loop count to 0 should play it one last time
        sound->setLoopCount(0);
        QCOMPARE(sound->loopCount(), 1);
        QCOMPARE(readSignal_Count.count(), 1);
        QCOMPARE(sound->loopsRemaining(), 1);
        QCOMPARE(readSignal_Remaining.count(), 1);

        QTRY_COMPARE(sound->loopsRemaining(), 0);
        QTRY_VERIFY(readSignal_Remaining.count() >= 2);
        QTRY_VERIFY(!sound->isPlaying());
    }
}

void tst_QSoundEffect::testVolume()
{
    QSignalSpy readSignal(sound, SIGNAL(volumeChanged()));

    sound->setVolume(0.5);
    QCOMPARE(sound->volume(),0.5);

    QTest::qWait(20);
    QCOMPARE(readSignal.count(),1);
}

void tst_QSoundEffect::testMuting()
{
    QSignalSpy readSignal(sound, SIGNAL(mutedChanged()));

    sound->setMuted(true);
    QCOMPARE(sound->isMuted(),true);

    QTest::qWait(20);
    QCOMPARE(readSignal.count(),1);
}

void tst_QSoundEffect::testPlaying()
{
    sound->setLoopCount(QSoundEffect::Infinite);
    sound->setVolume(0.1f);
    //valid source
    sound->setSource(url);
    QTestEventLoop::instance().enterLoop(1);
    sound->play();
    QTestEventLoop::instance().enterLoop(1);
    QTRY_COMPARE(sound->isPlaying(), true);
    sound->stop();

    //empty source
    sound->setSource(QUrl());
    QTestEventLoop::instance().enterLoop(1);
    sound->play();
    QTestEventLoop::instance().enterLoop(1);
    QTRY_COMPARE(sound->isPlaying(), false);

    //invalid source
    sound->setSource(QUrl((QLatin1String("invalid source"))));
    QTestEventLoop::instance().enterLoop(1);
    sound->play();
    QTestEventLoop::instance().enterLoop(1);
    QTRY_COMPARE(sound->isPlaying(), false);

    sound->setLoopCount(1); // TODO: What if one of the tests fail?
}

void tst_QSoundEffect::testStatus()
{
    sound->setSource(QUrl());
    QCOMPARE(sound->status(), QSoundEffect::Null);

    //valid source
    sound->setSource(url);

    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(sound->status(), QSoundEffect::Ready);

    //empty source
    sound->setSource(QUrl());
    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(sound->status(), QSoundEffect::Null);

    //invalid source
    sound->setLoopCount(QSoundEffect::Infinite);

    sound->setSource(QUrl(QLatin1String("invalid source")));
    QTestEventLoop::instance().enterLoop(1);
    QCOMPARE(sound->status(), QSoundEffect::Error);
}

void tst_QSoundEffect::testDestroyWhilePlaying()
{
    QSoundEffect *instance = new QSoundEffect();
    instance->setSource(url);
    instance->setVolume(0.1f);
    QTestEventLoop::instance().enterLoop(1);
    instance->play();
    QTest::qWait(500);
    delete instance;
    QTestEventLoop::instance().enterLoop(1);
}

void tst_QSoundEffect::testDestroyWhileRestartPlaying()
{
    QSoundEffect *instance = new QSoundEffect();
    instance->setSource(url);
    instance->setVolume(0.1f);
    QTestEventLoop::instance().enterLoop(1);
    instance->play();
    QTest::qWait(1000);
    //restart playing
    instance->play();
    delete instance;
    QTestEventLoop::instance().enterLoop(1);
}

void tst_QSoundEffect::testSetSourceWhileLoading()
{
    for (int i = 0; i < 10; i++) {
        sound->setSource(url);
        QVERIFY(sound->status() == QSoundEffect::Loading || sound->status() == QSoundEffect::Ready);
        sound->setSource(url); // set same source again
        QVERIFY(sound->status() == QSoundEffect::Loading || sound->status() == QSoundEffect::Ready);
        QTRY_COMPARE(sound->status(), QSoundEffect::Ready); // make sure it switches to ready state
        sound->play();
        QVERIFY(sound->isPlaying());

        sound->setSource(QUrl());
        QCOMPARE(sound->status(), QSoundEffect::Null);

        sound->setSource(url2);
        QVERIFY(sound->status() == QSoundEffect::Loading || sound->status() == QSoundEffect::Ready);
        sound->setSource(url); // set different source
        QVERIFY(sound->status() == QSoundEffect::Loading || sound->status() == QSoundEffect::Ready);
        QTRY_COMPARE(sound->status(), QSoundEffect::Ready);
        sound->play();
        QVERIFY(sound->isPlaying());
        sound->stop();

        sound->setSource(QUrl());
        QCOMPARE(sound->status(), QSoundEffect::Null);
    }
}

void tst_QSoundEffect::testSetSourceWhilePlaying()
{
    for (int i = 0; i < 10; i++) {
        sound->setSource(url);
        QTRY_COMPARE(sound->status(), QSoundEffect::Ready);
        sound->play();
        QVERIFY(sound->isPlaying());
        sound->setSource(url); // set same source again
        QCOMPARE(sound->status(), QSoundEffect::Ready);
        QVERIFY(sound->isPlaying()); // playback doesn't stop, URL is the same
        sound->play();
        QVERIFY(sound->isPlaying());

        sound->setSource(QUrl());
        QCOMPARE(sound->status(), QSoundEffect::Null);

        sound->setSource(url2);
        QTRY_COMPARE(sound->status(), QSoundEffect::Ready);
        sound->play();
        QVERIFY(sound->isPlaying());
        sound->setSource(url); // set different source
        QTRY_COMPARE(sound->status(), QSoundEffect::Ready);
        QVERIFY(!sound->isPlaying()); // playback stops, URL is different
        sound->play();
        QVERIFY(sound->isPlaying());
        sound->stop();

        sound->setSource(QUrl());
        QCOMPARE(sound->status(), QSoundEffect::Null);
    }
}

void tst_QSoundEffect::testSupportedMimeTypes()
{
    QStringList mimeTypes = sound->supportedMimeTypes();
    QVERIFY(!mimeTypes.empty());
    QVERIFY(mimeTypes.indexOf(QLatin1String("audio/wav")) != -1 ||
            mimeTypes.indexOf(QLatin1String("audio/x-wav")) != -1 ||
            mimeTypes.indexOf(QLatin1String("audio/wave")) != -1 ||
            mimeTypes.indexOf(QLatin1String("audio/x-pn-wav")) != -1);
}

void tst_QSoundEffect::testCorruptFile()
{
    for (int i = 0; i < 10; i++) {
        QSignalSpy statusSpy(sound, SIGNAL(statusChanged()));
        sound->setSource(urlCorrupted);
        QVERIFY(!sound->isPlaying());
        QVERIFY(sound->status() == QSoundEffect::Loading || sound->status() == QSoundEffect::Error);
        QTRY_COMPARE(sound->status(), QSoundEffect::Error);
        QCOMPARE(statusSpy.count(), 2);
        sound->play();
        QVERIFY(!sound->isPlaying());

        sound->setSource(url);
        QTRY_COMPARE(sound->status(), QSoundEffect::Ready);
        sound->play();
        QVERIFY(sound->isPlaying());
    }
}

QTEST_MAIN(tst_QSoundEffect)

#include "tst_qsoundeffect.moc"
