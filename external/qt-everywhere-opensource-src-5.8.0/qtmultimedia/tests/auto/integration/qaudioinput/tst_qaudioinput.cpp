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

#include <QtTest/QtTest>
#include <QtCore/qlocale.h>
#include <QtCore/QTemporaryDir>
#include <QtCore/QSharedPointer>
#include <QtCore/QScopedPointer>

#include <qaudioinput.h>
#include <qaudiodeviceinfo.h>
#include <qaudioformat.h>
#include <qaudio.h>

#include "wavheader.h"

//TESTED_COMPONENT=src/multimedia

#define AUDIO_BUFFER 192000
#define RANGE_ERR 0.5

template<typename T> inline bool qTolerantCompare(T value, T expected)
{
    return qAbs(value - expected) < (RANGE_ERR * expected);
}

#ifndef QTRY_VERIFY2
#define QTRY_VERIFY2(__expr,__msg) \
    do { \
        const int __step = 50; \
        const int __timeout = 5000; \
        if (!(__expr)) { \
            QTest::qWait(0); \
        } \
        for (int __i = 0; __i < __timeout && !(__expr); __i+=__step) { \
            QTest::qWait(__step); \
        } \
        QVERIFY2(__expr,__msg); \
    } while(0)
#endif

class tst_QAudioInput : public QObject
{
    Q_OBJECT
public:
    tst_QAudioInput(QObject* parent=0) : QObject(parent) {}

private slots:
    void initTestCase();

    void format();
    void invalidFormat_data();
    void invalidFormat();

    void bufferSize();
    void notifyInterval();
    void disableNotifyInterval();

    void stopWhileStopped();
    void suspendWhileStopped();
    void resumeWhileStopped();

    void pull_data(){generate_audiofile_testrows();}
    void pull();

    void pullSuspendResume_data(){generate_audiofile_testrows();}
    void pullSuspendResume();

    void push_data(){generate_audiofile_testrows();}
    void push();

    void pushSuspendResume_data(){generate_audiofile_testrows();}
    void pushSuspendResume();

    void reset_data(){generate_audiofile_testrows();}
    void reset();

    void volume_data(){generate_audiofile_testrows();}
    void volume();

private:
    typedef QSharedPointer<QFile> FilePtr;

    QString formatToFileName(const QAudioFormat &format);

    void generate_audiofile_testrows();

    QAudioDeviceInfo audioDevice;
    QList<QAudioFormat> testFormats;
    QList<FilePtr> audioFiles;
    QScopedPointer<QTemporaryDir> m_temporaryDir;

    QScopedPointer<QByteArray> m_byteArray;
    QScopedPointer<QBuffer> m_buffer;

    bool m_inCISystem;
};

void tst_QAudioInput::generate_audiofile_testrows()
{
    QTest::addColumn<FilePtr>("audioFile");
    QTest::addColumn<QAudioFormat>("audioFormat");

    for (int i=0; i<audioFiles.count(); i++) {
        QTest::newRow(QString("Audio File %1").arg(i).toLocal8Bit().constData())
                << audioFiles.at(i) << testFormats.at(i);

        // Only run first format in CI system to reduce test times
        if (m_inCISystem)
            break;
    }
}

QString tst_QAudioInput::formatToFileName(const QAudioFormat &format)
{
    const QString formatEndian = (format.byteOrder() == QAudioFormat::LittleEndian)
        ?   QString("LE") : QString("BE");

    const QString formatSigned = (format.sampleType() == QAudioFormat::SignedInt)
        ?   QString("signed") : QString("unsigned");

    return QString("%1_%2_%3_%4_%5")
        .arg(format.sampleRate())
        .arg(format.sampleSize())
        .arg(formatSigned)
        .arg(formatEndian)
        .arg(format.channelCount());
}

void tst_QAudioInput::initTestCase()
{
    qRegisterMetaType<QAudioFormat>();

    // Only perform tests if audio output device exists
    const QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

    if (devices.size() <= 0)
        QSKIP("No audio backend");

    audioDevice = QAudioDeviceInfo::defaultInputDevice();


    QAudioFormat format;

    format.setCodec("audio/pcm");

    if (audioDevice.isFormatSupported(audioDevice.preferredFormat()))
        testFormats.append(audioDevice.preferredFormat());

    // PCM 8000  mono S8
    format.setSampleRate(8000);
    format.setSampleSize(8);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
    format.setChannelCount(1);
    if (audioDevice.isFormatSupported(format))
        testFormats.append(format);

    // PCM 11025 mono S16LE
    format.setSampleRate(11025);
    format.setSampleSize(16);
    if (audioDevice.isFormatSupported(format))
        testFormats.append(format);

    // PCM 22050 mono S16LE
    format.setSampleRate(22050);
    if (audioDevice.isFormatSupported(format))
        testFormats.append(format);

    // PCM 22050 stereo S16LE
    format.setChannelCount(2);
    if (audioDevice.isFormatSupported(format))
        testFormats.append(format);

    // PCM 44100 stereo S16LE
    format.setSampleRate(44100);
    if (audioDevice.isFormatSupported(format))
        testFormats.append(format);

    // PCM 48000 stereo S16LE
    format.setSampleRate(48000);
    if (audioDevice.isFormatSupported(format))
        testFormats.append(format);

    QVERIFY(testFormats.size());

    const QChar slash = QLatin1Char('/');
    QString temporaryPattern = QDir::tempPath();
    if (!temporaryPattern.endsWith(slash))
        temporaryPattern += slash;
    temporaryPattern += "tst_qaudioinputXXXXXX";
    m_temporaryDir.reset(new QTemporaryDir(temporaryPattern));
    m_temporaryDir->setAutoRemove(true);
    QVERIFY(m_temporaryDir->isValid());

    const QString temporaryAudioPath = m_temporaryDir->path() + slash;
    foreach (const QAudioFormat &format, testFormats) {
        const QString fileName = temporaryAudioPath + formatToFileName(format) + QStringLiteral(".wav");
        audioFiles.append(FilePtr(new QFile(fileName)));
    }
    qgetenv("QT_TEST_CI").toInt(&m_inCISystem,10);
}

void tst_QAudioInput::format()
{
    QAudioInput audioInput(audioDevice.preferredFormat(), this);

    QAudioFormat requested = audioDevice.preferredFormat();
    QAudioFormat actual    = audioInput.format();

    QVERIFY2((requested.channelCount() == actual.channelCount()),
            QString("channels: requested=%1, actual=%2").arg(requested.channelCount()).arg(actual.channelCount()).toLocal8Bit().constData());
    QVERIFY2((requested.sampleRate() == actual.sampleRate()),
            QString("sampleRate: requested=%1, actual=%2").arg(requested.sampleRate()).arg(actual.sampleRate()).toLocal8Bit().constData());
    QVERIFY2((requested.sampleSize() == actual.sampleSize()),
            QString("sampleSize: requested=%1, actual=%2").arg(requested.sampleSize()).arg(actual.sampleSize()).toLocal8Bit().constData());
    QVERIFY2((requested.codec() == actual.codec()),
            QString("codec: requested=%1, actual=%2").arg(requested.codec()).arg(actual.codec()).toLocal8Bit().constData());
    QVERIFY2((requested.byteOrder() == actual.byteOrder()),
            QString("byteOrder: requested=%1, actual=%2").arg(requested.byteOrder()).arg(actual.byteOrder()).toLocal8Bit().constData());
    QVERIFY2((requested.sampleType() == actual.sampleType()),
            QString("sampleType: requested=%1, actual=%2").arg(requested.sampleType()).arg(actual.sampleType()).toLocal8Bit().constData());
}

void tst_QAudioInput::invalidFormat_data()
{
    QTest::addColumn<QAudioFormat>("invalidFormat");

    QAudioFormat format;

    QTest::newRow("Null Format")
            << format;

    format = audioDevice.preferredFormat();
    format.setChannelCount(0);
    QTest::newRow("Channel count 0")
            << format;

    format = audioDevice.preferredFormat();
    format.setSampleRate(0);
    QTest::newRow("Sample rate 0")
            << format;

    format = audioDevice.preferredFormat();
    format.setSampleSize(0);
    QTest::newRow("Sample size 0")
            << format;
}

void tst_QAudioInput::invalidFormat()
{
    QFETCH(QAudioFormat, invalidFormat);

    QVERIFY2(!audioDevice.isFormatSupported(invalidFormat),
            "isFormatSupported() is returning true on an invalid format");

    QAudioInput audioInput(invalidFormat, this);

    // Check that we are in the default state before calling start
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");

    audioInput.start();

    // Check that error is raised
    QTRY_VERIFY2((audioInput.error() == QAudio::OpenError),"error() was not set to QAudio::OpenError after start()");
}

void tst_QAudioInput::bufferSize()
{
    QAudioInput audioInput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError on creation");

    audioInput.setBufferSize(512);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setBufferSize(512)");
    QVERIFY2((audioInput.bufferSize() == 512),
            QString("bufferSize: requested=512, actual=%2").arg(audioInput.bufferSize()).toLocal8Bit().constData());

    audioInput.setBufferSize(4096);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setBufferSize(4096)");
    QVERIFY2((audioInput.bufferSize() == 4096),
            QString("bufferSize: requested=4096, actual=%2").arg(audioInput.bufferSize()).toLocal8Bit().constData());

    audioInput.setBufferSize(8192);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setBufferSize(8192)");
    QVERIFY2((audioInput.bufferSize() == 8192),
            QString("bufferSize: requested=8192, actual=%2").arg(audioInput.bufferSize()).toLocal8Bit().constData());
}

void tst_QAudioInput::notifyInterval()
{
    QAudioInput audioInput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError on creation");

    audioInput.setNotifyInterval(50);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setNotifyInterval(50)");
    QVERIFY2((audioInput.notifyInterval() == 50),
            QString("notifyInterval: requested=50, actual=%2").arg(audioInput.notifyInterval()).toLocal8Bit().constData());

    audioInput.setNotifyInterval(100);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setNotifyInterval(100)");
    QVERIFY2((audioInput.notifyInterval() == 100),
            QString("notifyInterval: requested=100, actual=%2").arg(audioInput.notifyInterval()).toLocal8Bit().constData());

    audioInput.setNotifyInterval(250);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setNotifyInterval(250)");
    QVERIFY2((audioInput.notifyInterval() == 250),
            QString("notifyInterval: requested=250, actual=%2").arg(audioInput.notifyInterval()).toLocal8Bit().constData());

    audioInput.setNotifyInterval(1000);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setNotifyInterval(1000)");
    QVERIFY2((audioInput.notifyInterval() == 1000),
            QString("notifyInterval: requested=1000, actual=%2").arg(audioInput.notifyInterval()).toLocal8Bit().constData());
}

void tst_QAudioInput::disableNotifyInterval()
{
    // Sets an invalid notification interval (QAudioInput::setNotifyInterval(0))
    // Checks that
    //  - No error is raised (QAudioInput::error() returns QAudio::NoError)
    //  - if <= 0, set to zero and disable notify signal

    QAudioInput audioInput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError on creation");

    audioInput.setNotifyInterval(0);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setNotifyInterval(0)");
    QVERIFY2((audioInput.notifyInterval() == 0),
            "notifyInterval() is not zero after setNotifyInterval(0)");

    audioInput.setNotifyInterval(-1);
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after setNotifyInterval(-1)");
    QVERIFY2((audioInput.notifyInterval() == 0),
            "notifyInterval() is not zero after setNotifyInterval(-1)");

    //start and run to check if notify() is emitted
    if (audioFiles.size() > 0) {
        QAudioInput audioInputCheck(testFormats.at(0), this);
        audioInputCheck.setNotifyInterval(0);
        QSignalSpy notifySignal(&audioInputCheck, SIGNAL(notify()));
        QFile *audioFile = audioFiles.at(0).data();
        audioFile->open(QIODevice::WriteOnly);
        audioInputCheck.start(audioFile);
        QTest::qWait(3000); // 3 seconds should be plenty
        audioInputCheck.stop();
        QVERIFY2((notifySignal.count() == 0),
                QString("didn't disable notify interval: shouldn't have got any but got %1").arg(notifySignal.count()).toLocal8Bit().constData());
        audioFile->close();
    }
}

void tst_QAudioInput::stopWhileStopped()
{
    // Calls QAudioInput::stop() when object is already in StoppedState
    // Checks that
    //  - No state change occurs
    //  - No error is raised (QAudioInput::error() returns QAudio::NoError)

    QAudioInput audioInput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");

    QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));
    audioInput.stop();

    // Check that no state transition occurred
    QVERIFY2((stateSignal.count() == 0), "stop() while stopped is emitting a signal and it shouldn't");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError after stop()");
}

void tst_QAudioInput::suspendWhileStopped()
{
    // Calls QAudioInput::suspend() when object is already in StoppedState
    // Checks that
    //  - No state change occurs
    //  - No error is raised (QAudioInput::error() returns QAudio::NoError)

    QAudioInput audioInput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");

    QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));
    audioInput.suspend();

    // Check that no state transition occurred
    QVERIFY2((stateSignal.count() == 0), "stop() while suspended is emitting a signal and it shouldn't");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError after stop()");
}

void tst_QAudioInput::resumeWhileStopped()
{
    // Calls QAudioInput::resume() when object is already in StoppedState
    // Checks that
    //  - No state change occurs
    //  - No error is raised (QAudioInput::error() returns QAudio::NoError)

    QAudioInput audioInput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");

    QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));
    audioInput.resume();

    // Check that no state transition occurred
    QVERIFY2((stateSignal.count() == 0), "resume() while stopped is emitting a signal and it shouldn't");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError after resume()");
}

void tst_QAudioInput::pull()
{
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);

    QAudioInput audioInput(audioFormat, this);

    audioInput.setNotifyInterval(100);

    QSignalSpy notifySignal(&audioInput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioInput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::WriteOnly);
    WavHeader wavHeader(audioFormat);
    QVERIFY(wavHeader.write(*audioFile));

    audioInput.start(audioFile.data());

    // Check that QAudioInput immediately transitions to ActiveState or IdleState
    QTRY_VERIFY2((stateSignal.count() > 0),"didn't emit signals on start()");
    QVERIFY2((audioInput.state() == QAudio::ActiveState || audioInput.state() == QAudio::IdleState),
             "didn't transition to ActiveState or IdleState after start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioInput.periodSize() > 0);
    stateSignal.clear();

    // Check that 'elapsed' increases
    QTest::qWait(40);
    QVERIFY2((audioInput.elapsedUSecs() > 0), "elapsedUSecs() is still zero after start()");

    // Allow some recording to happen
    QTest::qWait(3000); // 3 seconds should be plenty

    stateSignal.clear();

    qint64 processedUs = audioInput.processedUSecs();

    audioInput.stop();
    QTest::qWait(40);
    QTRY_VERIFY2((stateSignal.count() == 1),
                 QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2(qTolerantCompare(processedUs, 3040000LL),
             QString("processedUSecs() doesn't fall in acceptable range, should be 3040000 (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    QVERIFY2((audioInput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");
    QVERIFY2(notifySignal.count() > 0, "not emitting notify() signal");

    WavHeader::writeDataLength(*audioFile, audioFile->pos() - WavHeader::headerLength());
    audioFile->close();

}

void tst_QAudioInput::pullSuspendResume()
{
#ifdef Q_OS_LINUX
    if (m_inCISystem)
        QSKIP("QTBUG-26504 Fails 20% of time with pulseaudio backend");
#endif
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);

    QAudioInput audioInput(audioFormat, this);

    audioInput.setNotifyInterval(100);

    QSignalSpy notifySignal(&audioInput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioInput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::WriteOnly);
    WavHeader wavHeader(audioFormat);
    QVERIFY(wavHeader.write(*audioFile));

    audioInput.start(audioFile.data());

    // Check that QAudioInput immediately transitions to ActiveState or IdleState
    QTRY_VERIFY2((stateSignal.count() > 0),"didn't emit signals on start()");
    QVERIFY2((audioInput.state() == QAudio::ActiveState || audioInput.state() == QAudio::IdleState),
             "didn't transition to ActiveState or IdleState after start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioInput.periodSize() > 0);
    stateSignal.clear();

    // Check that 'elapsed' increases
    QTest::qWait(40);
    QVERIFY2((audioInput.elapsedUSecs() > 0), "elapsedUSecs() is still zero after start()");

    // Allow some recording to happen
    QTest::qWait(3000); // 3 seconds should be plenty

    QVERIFY2((audioInput.state() == QAudio::ActiveState),
             "didn't transition to ActiveState after some recording");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after some recording");

    stateSignal.clear();

    audioInput.suspend();

    // Give backends running in separate threads a chance to suspend.
    QTest::qWait(100);

    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit SuspendedState signal after suspend(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioInput.state() == QAudio::SuspendedState), "didn't transitions to SuspendedState after stop()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    stateSignal.clear();

    // Check that only 'elapsed', and not 'processed' increases while suspended
    qint64 elapsedUs = audioInput.elapsedUSecs();
    qint64 processedUs = audioInput.processedUSecs();
    QTest::qWait(1000);
    QVERIFY(audioInput.elapsedUSecs() > elapsedUs);
    QVERIFY(audioInput.processedUSecs() == processedUs);

    audioInput.resume();

    // Give backends running in separate threads a chance to resume.
    QTest::qWait(100);

    // Check that QAudioInput immediately transitions to ActiveState
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit signal after resume(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioInput.state() == QAudio::ActiveState), "didn't transition to ActiveState after resume()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after resume()");
    stateSignal.clear();

    processedUs = audioInput.processedUSecs();

    audioInput.stop();
    QTest::qWait(40);
    QTRY_VERIFY2((stateSignal.count() == 1),
                 QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2(qTolerantCompare(processedUs, 3040000LL),
             QString("processedUSecs() doesn't fall in acceptable range, should be 3040000 (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    QVERIFY2((audioInput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");
    QVERIFY2(notifySignal.count() > 0, "not emitting notify() signal");

    WavHeader::writeDataLength(*audioFile,audioFile->pos()-WavHeader::headerLength());
    audioFile->close();
}

void tst_QAudioInput::push()
{
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);

    QAudioInput audioInput(audioFormat, this);

    audioInput.setNotifyInterval(100);

    QSignalSpy notifySignal(&audioInput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioInput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::WriteOnly);
    WavHeader wavHeader(audioFormat);
    QVERIFY(wavHeader.write(*audioFile));

    // Set a large buffer to avoid underruns during QTest::qWaits
    audioInput.setBufferSize(audioFormat.bytesForDuration(1000000));

    QIODevice* feed = audioInput.start();

    // Check that QAudioInput immediately transitions to IdleState
    QTRY_VERIFY2((stateSignal.count() == 1),"didn't emit IdleState signal on start()");
    QVERIFY2((audioInput.state() == QAudio::IdleState),
             "didn't transition to IdleState after start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioInput.periodSize() > 0);
    stateSignal.clear();

    // Check that 'elapsed' increases
    QTest::qWait(40);
    QVERIFY2((audioInput.elapsedUSecs() > 0), "elapsedUSecs() is still zero after start()");

    qint64 totalBytesRead = 0;
    bool firstBuffer = true;
    QByteArray buffer(AUDIO_BUFFER, 0);
    qint64 len = (audioFormat.sampleRate()*audioFormat.channelCount()*(audioFormat.sampleSize()/8)*2); // 2 seconds
    while (totalBytesRead < len) {
        QTRY_VERIFY_WITH_TIMEOUT(audioInput.bytesReady() >= audioInput.periodSize(), 10000);
        qint64 bytesRead = feed->read(buffer.data(), audioInput.periodSize());
        audioFile->write(buffer.constData(),bytesRead);
        totalBytesRead+=bytesRead;
        if (firstBuffer && bytesRead) {
            // Check for transition to ActiveState when data is provided
            QTRY_VERIFY2((stateSignal.count() == 1),"didn't emit ActiveState signal on data");
            QVERIFY2((audioInput.state() == QAudio::ActiveState),
                     "didn't transition to ActiveState after data");
            QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
            firstBuffer = false;
        }
    }

    QTest::qWait(1000);

    stateSignal.clear();

    qint64 processedUs = audioInput.processedUSecs();

    audioInput.stop();
    QTest::qWait(40);
    QTRY_VERIFY2((stateSignal.count() == 1),
                 QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2(qTolerantCompare(processedUs, 2040000LL),
             QString("processedUSecs() doesn't fall in acceptable range, should be 2040000 (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    QVERIFY2((audioInput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");
    QVERIFY2(notifySignal.count() > 0, "not emitting notify() signal");

    WavHeader::writeDataLength(*audioFile,audioFile->pos()-WavHeader::headerLength());
    audioFile->close();
}

void tst_QAudioInput::pushSuspendResume()
{
#ifdef Q_OS_LINUX
    if (m_inCISystem)
        QSKIP("QTBUG-26504 Fails 20% of time with pulseaudio backend");
#endif
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);
    QAudioInput audioInput(audioFormat, this);

    audioInput.setNotifyInterval(100);
    audioInput.setBufferSize(audioFormat.bytesForDuration(1000000));

    QSignalSpy notifySignal(&audioInput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioInput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::WriteOnly);
    WavHeader wavHeader(audioFormat);
    QVERIFY(wavHeader.write(*audioFile));

    QIODevice* feed = audioInput.start();

    // Check that QAudioInput immediately transitions to IdleState
    QTRY_VERIFY2((stateSignal.count() == 1),"didn't emit IdleState signal on start()");
    QVERIFY2((audioInput.state() == QAudio::IdleState),
             "didn't transition to IdleState after start()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioInput.periodSize() > 0);
    stateSignal.clear();

    // Check that 'elapsed' increases
    QTest::qWait(40);
    QTRY_VERIFY2((audioInput.elapsedUSecs() > 0), "elapsedUSecs() is still zero after start()");

    qint64 totalBytesRead = 0;
    bool firstBuffer = true;
    QByteArray buffer(AUDIO_BUFFER, 0);
    qint64 len = (audioFormat.sampleRate()*audioFormat.channelCount()*(audioFormat.sampleSize()/8)); // 1 seconds
    while (totalBytesRead < len) {
        QTRY_VERIFY_WITH_TIMEOUT(audioInput.bytesReady() >= audioInput.periodSize(), 10000);
        qint64 bytesRead = feed->read(buffer.data(), audioInput.periodSize());
        audioFile->write(buffer.constData(),bytesRead);
        totalBytesRead+=bytesRead;
        if (firstBuffer && bytesRead) {
            // Check for transition to ActiveState when data is provided
            QTRY_VERIFY2((stateSignal.count() == 1),"didn't emit ActiveState signal on data");
            QVERIFY2((audioInput.state() == QAudio::ActiveState),
                     "didn't transition to ActiveState after data");
            QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
            firstBuffer = false;
        }
    }
    stateSignal.clear();

    audioInput.suspend();

    // Give backends running in separate threads a chance to suspend
    QTest::qWait(100);

    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit SuspendedState signal after suspend(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioInput.state() == QAudio::SuspendedState), "didn't transitions to SuspendedState after stop()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    stateSignal.clear();

    // Check that only 'elapsed', and not 'processed' increases while suspended
    qint64 elapsedUs = audioInput.elapsedUSecs();
    qint64 processedUs = audioInput.processedUSecs();
    QTest::qWait(1000);
    QVERIFY(audioInput.elapsedUSecs() > elapsedUs);
    QVERIFY(audioInput.processedUSecs() == processedUs);

    // Drain any data, in case we run out of space when resuming
    const int reads = audioInput.bytesReady() / audioInput.periodSize();
    for (int r = 0; r < reads; ++r)
        feed->read(buffer.data(), audioInput.periodSize());

    audioInput.resume();

    // Check that QAudioInput immediately transitions to Active or IdleState
    QTRY_VERIFY2((stateSignal.count() > 0),"didn't emit signals on resume()");
    QVERIFY2((audioInput.state() == QAudio::ActiveState || audioInput.state() == QAudio::IdleState),
             "didn't transition to ActiveState or IdleState after resume()");
    QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after resume()");
    QVERIFY(audioInput.periodSize() > 0);

    // Let it play out what is in buffer and go to Idle before continue
    QTest::qWait(1000);
    stateSignal.clear();

    // Read another seconds worth
    totalBytesRead = 0;
    firstBuffer = true;
    while (totalBytesRead < len && audioInput.state() != QAudio::StoppedState) {
        QTRY_VERIFY_WITH_TIMEOUT(audioInput.bytesReady() >= audioInput.periodSize(), 10000);
        qint64 bytesRead = feed->read(buffer.data(), audioInput.periodSize());
        audioFile->write(buffer.constData(),bytesRead);
        totalBytesRead+=bytesRead;
    }
    stateSignal.clear();

    processedUs = audioInput.processedUSecs();

    audioInput.stop();
    QTest::qWait(40);
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioInput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2(qTolerantCompare(processedUs, 2040000LL),
             QString("processedUSecs() doesn't fall in acceptable range, should be 2040000 (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioInput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");

    WavHeader::writeDataLength(*audioFile,audioFile->pos()-WavHeader::headerLength());
    audioFile->close();
}

void tst_QAudioInput::reset()
{
    QFETCH(QAudioFormat, audioFormat);

    // Try both push/pull.. the vagaries of Active vs Idle are tested elsewhere
    {
        QAudioInput audioInput(audioFormat, this);

        audioInput.setNotifyInterval(100);

        QSignalSpy notifySignal(&audioInput, SIGNAL(notify()));
        QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));

        // Check that we are in the default state before calling start
        QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
        QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
        QVERIFY2((audioInput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

        QIODevice* device = audioInput.start();
        // Check that QAudioInput immediately transitions to IdleState
        QTRY_VERIFY2((stateSignal.count() == 1),"didn't emit IdleState signal on start()");
        QVERIFY2((audioInput.state() == QAudio::IdleState), "didn't transition to IdleState after start()");
        QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
        QVERIFY(audioInput.periodSize() > 0);
        QTRY_VERIFY2_WITH_TIMEOUT((audioInput.bytesReady() > audioInput.periodSize()), "no bytes available after starting", 10000);

        // Trigger a read
        QByteArray data = device->read(audioInput.periodSize());
        QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
        stateSignal.clear();

        audioInput.reset();
        QTRY_VERIFY2((stateSignal.count() == 1),"didn't emit StoppedState signal after reset()");
        QVERIFY2((audioInput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after reset()");
        QVERIFY2((audioInput.bytesReady() == 0), "buffer not cleared after reset()");
    }

    {
        QAudioInput audioInput(audioFormat, this);
        QBuffer buffer;

        audioInput.setNotifyInterval(100);

        QSignalSpy notifySignal(&audioInput, SIGNAL(notify()));
        QSignalSpy stateSignal(&audioInput, SIGNAL(stateChanged(QAudio::State)));

        // Check that we are in the default state before calling start
        QVERIFY2((audioInput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
        QVERIFY2((audioInput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
        QVERIFY2((audioInput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

        audioInput.start(&buffer);

        // Check that QAudioInput immediately transitions to ActiveState
        QTRY_VERIFY2((stateSignal.count() >= 1),"didn't emit state changed signal on start()");
        QTRY_VERIFY2((audioInput.state() == QAudio::ActiveState), "didn't transition to ActiveState after start()");
        QVERIFY2((audioInput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
        QVERIFY(audioInput.periodSize() > 0);
        stateSignal.clear();

        audioInput.reset();
        QTRY_VERIFY2((stateSignal.count() >= 1),"didn't emit StoppedState signal after reset()");
        QVERIFY2((audioInput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after reset()");
        QVERIFY2((audioInput.bytesReady() == 0), "buffer not cleared after reset()");
    }
}

void tst_QAudioInput::volume()
{
    QFETCH(QAudioFormat, audioFormat);

    const qreal half(0.5f);
    const qreal one(1.0f);

    QAudioInput audioInput(audioFormat, this);

    qreal volume = audioInput.volume();
    audioInput.setVolume(half);
    QTRY_VERIFY(qRound(audioInput.volume()*10.0f) == 5);
    // Wait a while to see if this changes
    QTest::qWait(500);
    QTRY_VERIFY(qRound(audioInput.volume()*10.0f) == 5);

    audioInput.setVolume(one);
    QTRY_VERIFY(qRound(audioInput.volume()*10.0f) == 10);
    // Wait a while to see if this changes
    QTest::qWait(500);
    QTRY_VERIFY(qRound(audioInput.volume()*10.0f) == 10);

    audioInput.setVolume(volume);
}

QTEST_MAIN(tst_QAudioInput)

#include "tst_qaudioinput.moc"
