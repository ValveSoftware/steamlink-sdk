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
#include <QtCore/QTemporaryDir>
#include <QtCore/QSharedPointer>
#include <QtCore/QScopedPointer>

#include <qaudiooutput.h>
#include <qaudiodeviceinfo.h>
#include <qaudioformat.h>
#include <qaudio.h>

#include "wavheader.h"

#define AUDIO_BUFFER 192000

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
    } while (0)
#endif

class tst_QAudioOutput : public QObject
{
    Q_OBJECT
public:
    tst_QAudioOutput(QObject* parent=0) : QObject(parent) {}

private slots:
    void initTestCase();

    void format();
    void invalidFormat_data();
    void invalidFormat();

    void bufferSize_data();
    void bufferSize();

    void notifyInterval_data();
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

    void pushUnderrun_data(){generate_audiofile_testrows();}
    void pushUnderrun();

    void volume_data();
    void volume();

private:
    typedef QSharedPointer<QFile> FilePtr;

    QString formatToFileName(const QAudioFormat &format);
    void createSineWaveData(const QAudioFormat &format, qint64 length, int sampleRate = 440);

    void generate_audiofile_testrows();

    QAudioDeviceInfo audioDevice;
    QList<QAudioFormat> testFormats;
    QList<FilePtr> audioFiles;
    QScopedPointer<QTemporaryDir> m_temporaryDir;

    QScopedPointer<QByteArray> m_byteArray;
    QScopedPointer<QBuffer> m_buffer;

    bool m_inCISystem;
};

QString tst_QAudioOutput::formatToFileName(const QAudioFormat &format)
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

void tst_QAudioOutput::createSineWaveData(const QAudioFormat &format, qint64 length, int sampleRate)
{
    const int channelBytes = format.sampleSize() / 8;
    const int sampleBytes = format.channelCount() * channelBytes;

    Q_ASSERT(length % sampleBytes == 0);
    Q_UNUSED(sampleBytes) // suppress warning in release builds

    m_byteArray.reset(new QByteArray(length, 0));
    unsigned char *ptr = reinterpret_cast<unsigned char *>(m_byteArray->data());
    int sampleIndex = 0;

    while (length) {
        const qreal x = qSin(2 * M_PI * sampleRate * qreal(sampleIndex % format.sampleRate()) / format.sampleRate());
        for (int i=0; i<format.channelCount(); ++i) {
            if (format.sampleSize() == 8 && format.sampleType() == QAudioFormat::UnSignedInt) {
                const quint8 value = static_cast<quint8>((1.0 + x) / 2 * 255);
                *reinterpret_cast<quint8*>(ptr) = value;
            } else if (format.sampleSize() == 8 && format.sampleType() == QAudioFormat::SignedInt) {
                const qint8 value = static_cast<qint8>(x * 127);
                *reinterpret_cast<quint8*>(ptr) = value;
            } else if (format.sampleSize() == 16 && format.sampleType() == QAudioFormat::UnSignedInt) {
                quint16 value = static_cast<quint16>((1.0 + x) / 2 * 65535);
                if (format.byteOrder() == QAudioFormat::LittleEndian)
                    qToLittleEndian<quint16>(value, ptr);
                else
                    qToBigEndian<quint16>(value, ptr);
            } else if (format.sampleSize() == 16 && format.sampleType() == QAudioFormat::SignedInt) {
                qint16 value = static_cast<qint16>(x * 32767);
                if (format.byteOrder() == QAudioFormat::LittleEndian)
                    qToLittleEndian<qint16>(value, ptr);
                else
                    qToBigEndian<qint16>(value, ptr);
            }

            ptr += channelBytes;
            length -= channelBytes;
        }
        ++sampleIndex;
    }

    m_buffer.reset(new QBuffer(m_byteArray.data(), this));
    Q_ASSERT(m_buffer->open(QIODevice::ReadOnly));
}

void tst_QAudioOutput::generate_audiofile_testrows()
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

void tst_QAudioOutput::initTestCase()
{
    qRegisterMetaType<QAudioFormat>();

    // Only perform tests if audio output device exists
    const QList<QAudioDeviceInfo> devices =
        QAudioDeviceInfo::availableDevices(QAudio::AudioOutput);

    if (devices.size() <= 0)
        QSKIP("No audio backend");

    audioDevice = QAudioDeviceInfo::defaultOutputDevice();


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
    temporaryPattern += "tst_qaudiooutputXXXXXX";
    m_temporaryDir.reset(new QTemporaryDir(temporaryPattern));
    m_temporaryDir->setAutoRemove(true);
    QVERIFY(m_temporaryDir->isValid());

    const QString temporaryAudioPath = m_temporaryDir->path() + slash;
    foreach (const QAudioFormat &format, testFormats) {
        qint64 len = (format.sampleRate()*format.channelCount()*(format.sampleSize()/8)*2); // 2 seconds
        createSineWaveData(format, len);
        // Write generate sine wave data to file
        const QString fileName = temporaryAudioPath + QStringLiteral("generated")
                                 + formatToFileName(format) + QStringLiteral(".wav");
        FilePtr file(new QFile(fileName));
        QVERIFY2(file->open(QIODevice::WriteOnly), qPrintable(file->errorString()));
        WavHeader wavHeader(format, len);
        wavHeader.write(*file.data());
        file->write(m_byteArray->data(), len);
        file->close();
        audioFiles.append(file);
    }
    qgetenv("QT_TEST_CI").toInt(&m_inCISystem,10);
}

void tst_QAudioOutput::format()
{
    QAudioOutput audioOutput(audioDevice.preferredFormat(), this);

    QAudioFormat requested = audioDevice.preferredFormat();
    QAudioFormat actual    = audioOutput.format();

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

void tst_QAudioOutput::invalidFormat_data()
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

void tst_QAudioOutput::invalidFormat()
{
    QFETCH(QAudioFormat, invalidFormat);

    QVERIFY2(!audioDevice.isFormatSupported(invalidFormat),
            "isFormatSupported() is returning true on an invalid format");

    QAudioOutput audioOutput(invalidFormat, this);

    // Check that we are in the default state before calling start
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");

    audioOutput.start();
    // Check that error is raised
    QTRY_VERIFY2((audioOutput.error() == QAudio::OpenError),"error() was not set to QAudio::OpenError after start()");
}

void tst_QAudioOutput::bufferSize_data()
{
    QTest::addColumn<int>("bufferSize");
    QTest::newRow("Buffer size 512") << 512;
    QTest::newRow("Buffer size 4096") << 4096;
    QTest::newRow("Buffer size 8192") << 8192;
}

void tst_QAudioOutput::bufferSize()
{
    QFETCH(int, bufferSize);
    QAudioOutput audioOutput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioOutput.error() == QAudio::NoError), QString("error() was not set to QAudio::NoError on creation(%1)").arg(bufferSize).toLocal8Bit().constData());

    audioOutput.setBufferSize(bufferSize);
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() is not QAudio::NoError after setBufferSize");
    QVERIFY2((audioOutput.bufferSize() == bufferSize),
             QString("bufferSize: requested=%1, actual=%2").arg(bufferSize).arg(audioOutput.bufferSize()).toLocal8Bit().constData());
}

void tst_QAudioOutput::notifyInterval_data()
{
    QTest::addColumn<int>("interval");
    QTest::newRow("Notify interval 50") << 50;
    QTest::newRow("Notify interval 100") << 100;
    QTest::newRow("Notify interval 250") << 250;
    QTest::newRow("Notify interval 1000") << 1000;
}

void tst_QAudioOutput::notifyInterval()
{
    QFETCH(int, interval);
    QAudioOutput audioOutput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError on creation");

    audioOutput.setNotifyInterval(interval);
    QVERIFY2((audioOutput.error() == QAudio::NoError), QString("error() is not QAudio::NoError after setNotifyInterval(%1)").arg(interval).toLocal8Bit().constData());
    QVERIFY2((audioOutput.notifyInterval() == interval),
             QString("notifyInterval: requested=%1, actual=%2").arg(interval).arg(audioOutput.notifyInterval()).toLocal8Bit().constData());
}

void tst_QAudioOutput::disableNotifyInterval()
{
    // Sets an invalid notification interval (QAudioOutput::setNotifyInterval(0))
    // Checks that
    //  - No error is raised (QAudioOutput::error() returns QAudio::NoError)
    //  - if <= 0, set to zero and disable notify signal

    QAudioOutput audioOutput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError on creation");

    audioOutput.setNotifyInterval(0);
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() is not QAudio::NoError after setNotifyInterval(0)");
    QVERIFY2((audioOutput.notifyInterval() == 0),
            "notifyInterval() is not zero after setNotifyInterval(0)");

    audioOutput.setNotifyInterval(-1);
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() is not QAudio::NoError after setNotifyInterval(-1)");
    QVERIFY2((audioOutput.notifyInterval() == 0),
            "notifyInterval() is not zero after setNotifyInterval(-1)");

    //start and run to check if notify() is emitted
    if (audioFiles.size() > 0) {
        QAudioOutput audioOutputCheck(testFormats.at(0), this);
        audioOutputCheck.setNotifyInterval(0);
        audioOutputCheck.setVolume(0.1f);

        QSignalSpy notifySignal(&audioOutputCheck, SIGNAL(notify()));
        QFile *audioFile = audioFiles.at(0).data();
        audioFile->open(QIODevice::ReadOnly);
        audioOutputCheck.start(audioFile);
        QTest::qWait(3000); // 3 seconds should be plenty
        audioOutputCheck.stop();
        QVERIFY2((notifySignal.count() == 0),
                QString("didn't disable notify interval: shouldn't have got any but got %1").arg(notifySignal.count()).toLocal8Bit().constData());
        audioFile->close();
    }
}

void tst_QAudioOutput::stopWhileStopped()
{
    // Calls QAudioOutput::stop() when object is already in StoppedState
    // Checks that
    //  - No state change occurs
    //  - No error is raised (QAudioOutput::error() returns QAudio::NoError)

    QAudioOutput audioOutput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");

    QSignalSpy stateSignal(&audioOutput, SIGNAL(stateChanged(QAudio::State)));
    audioOutput.stop();

    // Check that no state transition occurred
    QVERIFY2((stateSignal.count() == 0), "stop() while stopped is emitting a signal and it shouldn't");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError after stop()");
}

void tst_QAudioOutput::suspendWhileStopped()
{
    // Calls QAudioOutput::suspend() when object is already in StoppedState
    // Checks that
    //  - No state change occurs
    //  - No error is raised (QAudioOutput::error() returns QAudio::NoError)

    QAudioOutput audioOutput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");

    QSignalSpy stateSignal(&audioOutput, SIGNAL(stateChanged(QAudio::State)));
    audioOutput.suspend();

    // Check that no state transition occurred
    QVERIFY2((stateSignal.count() == 0), "stop() while suspended is emitting a signal and it shouldn't");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError after stop()");
}

void tst_QAudioOutput::resumeWhileStopped()
{
    // Calls QAudioOutput::resume() when object is already in StoppedState
    // Checks that
    //  - No state change occurs
    //  - No error is raised (QAudioOutput::error() returns QAudio::NoError)

    QAudioOutput audioOutput(audioDevice.preferredFormat(), this);

    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");

    QSignalSpy stateSignal(&audioOutput, SIGNAL(stateChanged(QAudio::State)));
    audioOutput.resume();

    // Check that no state transition occurred
    QVERIFY2((stateSignal.count() == 0), "resume() while stopped is emitting a signal and it shouldn't");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError after resume()");
}

void tst_QAudioOutput::pull()
{
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);

    QAudioOutput audioOutput(audioFormat, this);

    audioOutput.setNotifyInterval(100);
    audioOutput.setVolume(0.1f);

    QSignalSpy notifySignal(&audioOutput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioOutput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioOutput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::ReadOnly);
    audioFile->seek(WavHeader::headerLength());

    audioOutput.start(audioFile.data());

    // Check that QAudioOutput immediately transitions to ActiveState
    QTRY_VERIFY2((stateSignal.count() == 1),
                 QString("didn't emit signal on start(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::ActiveState), "didn't transition to ActiveState after start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioOutput.periodSize() > 0);
    stateSignal.clear();

    // Check that 'elapsed' increases
    QTest::qWait(40);
    QVERIFY2((audioOutput.elapsedUSecs() > 0), "elapsedUSecs() is still zero after start()");

    // Wait until playback finishes
    QTRY_VERIFY2(audioFile->atEnd(), "didn't play to EOF");
    QTRY_VERIFY(stateSignal.count() > 0);
    QCOMPARE(qvariant_cast<QAudio::State>(stateSignal.last().at(0)), QAudio::IdleState);
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transitions to IdleState when at EOF");
    stateSignal.clear();

    qint64 processedUs = audioOutput.processedUSecs();

    audioOutput.stop();
    QTest::qWait(40);
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2((processedUs == 2000000),
             QString("processedUSecs() doesn't equal file duration in us (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    QVERIFY2((audioOutput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");
    QVERIFY2(notifySignal.count() > 0, "not emitting notify() signal");

    audioFile->close();
}

void tst_QAudioOutput::pullSuspendResume()
{
#ifdef Q_OS_LINUX
    if (m_inCISystem)
        QSKIP("QTBUG-26504 Fails 20% of time with pulseaudio backend");
#endif
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);
    QAudioOutput audioOutput(audioFormat, this);

    audioOutput.setNotifyInterval(100);
    audioOutput.setVolume(0.1f);

    QSignalSpy notifySignal(&audioOutput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioOutput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioOutput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::ReadOnly);
    audioFile->seek(WavHeader::headerLength());

    audioOutput.start(audioFile.data());
    // Check that QAudioOutput immediately transitions to ActiveState
    QTRY_VERIFY2((stateSignal.count() == 1),
                 QString("didn't emit signal on start(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::ActiveState), "didn't transition to ActiveState after start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioOutput.periodSize() > 0);
    stateSignal.clear();

    // Wait for half of clip to play
    QTest::qWait(1000);

    audioOutput.suspend();

    // Give backends running in separate threads a chance to suspend.
    QTest::qWait(100);

    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit SuspendedState signal after suspend(), got %1 signals instead")
             .arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::SuspendedState), "didn't transition to SuspendedState after suspend()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after suspend()");
    stateSignal.clear();

    // Check that only 'elapsed', and not 'processed' increases while suspended
    qint64 elapsedUs = audioOutput.elapsedUSecs();
    qint64 processedUs = audioOutput.processedUSecs();
    QTest::qWait(1000);
    QVERIFY(audioOutput.elapsedUSecs() > elapsedUs);
    QVERIFY(audioOutput.processedUSecs() == processedUs);

    audioOutput.resume();

    // Check that QAudioOutput immediately transitions to ActiveState
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit signal after resume(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::ActiveState), "didn't transition to ActiveState after resume()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after resume()");
    stateSignal.clear();

    // Wait until playback finishes
    QTest::qWait(3000); // 3 seconds should be plenty

    QVERIFY2(audioFile->atEnd(), "didn't play to EOF");
    QVERIFY(stateSignal.count() > 0);
    QCOMPARE(qvariant_cast<QAudio::State>(stateSignal.last().at(0)), QAudio::IdleState);
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transitions to IdleState when at EOF");
    stateSignal.clear();

    processedUs = audioOutput.processedUSecs();

    audioOutput.stop();
    QTest::qWait(40);
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2((processedUs == 2000000),
             QString("processedUSecs() doesn't equal file duration in us (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    QVERIFY2((audioOutput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");

    audioFile->close();
}

void tst_QAudioOutput::push()
{
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);

    QAudioOutput audioOutput(audioFormat, this);

    audioOutput.setNotifyInterval(100);
    audioOutput.setVolume(0.1f);

    QSignalSpy notifySignal(&audioOutput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioOutput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioOutput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::ReadOnly);
    audioFile->seek(WavHeader::headerLength());

    QIODevice* feed = audioOutput.start();

    // Check that QAudioOutput immediately transitions to IdleState
    QTRY_VERIFY2((stateSignal.count() == 1),
                 QString("didn't emit signal on start(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transition to IdleState after start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioOutput.periodSize() > 0);
    stateSignal.clear();

    // Check that 'elapsed' increases
    QTest::qWait(40);
    QVERIFY2((audioOutput.elapsedUSecs() > 0), "elapsedUSecs() is still zero after start()");
    QVERIFY2((audioOutput.processedUSecs() == qint64(0)), "processedUSecs() is not zero after start()");

    qint64 written = 0;
    bool firstBuffer = true;
    QByteArray buffer(AUDIO_BUFFER, 0);

    while (written < audioFile->size()-WavHeader::headerLength()) {

        if (audioOutput.bytesFree() >= audioOutput.periodSize()) {
            qint64 len = audioFile->read(buffer.data(),audioOutput.periodSize());
            written += feed->write(buffer.constData(), len);

            if (firstBuffer) {
                // Check for transition to ActiveState when data is provided
                QVERIFY2((stateSignal.count() == 1),
                         QString("didn't emit signal after receiving data, got %1 signals instead")
                         .arg(stateSignal.count()).toLocal8Bit().constData());
                QVERIFY2((audioOutput.state() == QAudio::ActiveState), "didn't transition to ActiveState after receiving data");
                QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after receiving data");
                firstBuffer = false;
                stateSignal.clear();
            }
        } else
            QTest::qWait(20);
    }

    // Wait until playback finishes
    QTest::qWait(3000); // 3 seconds should be plenty

    QVERIFY2(audioFile->atEnd(), "didn't play to EOF");
    QVERIFY(stateSignal.count() > 0);
    QCOMPARE(qvariant_cast<QAudio::State>(stateSignal.last().at(0)), QAudio::IdleState);
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transitions to IdleState when at EOF");
    stateSignal.clear();

    qint64 processedUs = audioOutput.processedUSecs();

    audioOutput.stop();
    QTest::qWait(40);
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2((processedUs == 2000000),
             QString("processedUSecs() doesn't equal file duration in us (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    QVERIFY2((audioOutput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");
    QVERIFY2(notifySignal.count() > 0, "not emitting notify signal");

    audioFile->close();
}

void tst_QAudioOutput::pushSuspendResume()
{
#ifdef Q_OS_LINUX
    if (m_inCISystem)
        QSKIP("QTBUG-26504 Fails 20% of time with pulseaudio backend");
#endif
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);

    QAudioOutput audioOutput(audioFormat, this);

    audioOutput.setNotifyInterval(100);
    audioOutput.setVolume(0.1f);

    QSignalSpy notifySignal(&audioOutput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioOutput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioOutput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::ReadOnly);
    audioFile->seek(WavHeader::headerLength());

    QIODevice* feed = audioOutput.start();

    // Check that QAudioOutput immediately transitions to IdleState
    QTRY_VERIFY2((stateSignal.count() == 1),
                 QString("didn't emit signal on start(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transition to IdleState after start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioOutput.periodSize() > 0);
    stateSignal.clear();

    // Check that 'elapsed' increases
    QTest::qWait(40);
    QVERIFY2((audioOutput.elapsedUSecs() > 0), "elapsedUSecs() is still zero after start()");
    QVERIFY2((audioOutput.processedUSecs() == qint64(0)), "processedUSecs() is not zero after start()");

    qint64 written = 0;
    bool firstBuffer = true;
    QByteArray buffer(AUDIO_BUFFER, 0);

    // Play half of the clip
    while (written < (audioFile->size()-WavHeader::headerLength())/2) {

        if (audioOutput.bytesFree() >= audioOutput.periodSize()) {
            qint64 len = audioFile->read(buffer.data(),audioOutput.periodSize());
            written += feed->write(buffer.constData(), len);

            if (firstBuffer) {
                // Check for transition to ActiveState when data is provided
                QVERIFY2((stateSignal.count() == 1),
                         QString("didn't emit signal after receiving data, got %1 signals instead")
                         .arg(stateSignal.count()).toLocal8Bit().constData());
                QVERIFY2((audioOutput.state() == QAudio::ActiveState), "didn't transition to ActiveState after receiving data");
                QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after receiving data");
                firstBuffer = false;
            }
        } else
            QTest::qWait(20);
    }
    stateSignal.clear();

    audioOutput.suspend();

    // Give backends running in separate threads a chance to suspend.
    QTest::qWait(100);

    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit SuspendedState signal after suspend(), got %1 signals instead")
             .arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::SuspendedState), "didn't transition to SuspendedState after suspend()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after suspend()");
    stateSignal.clear();

    // Check that only 'elapsed', and not 'processed' increases while suspended
    qint64 elapsedUs = audioOutput.elapsedUSecs();
    qint64 processedUs = audioOutput.processedUSecs();
    QTest::qWait(1000);
    QVERIFY(audioOutput.elapsedUSecs() > elapsedUs);
    QVERIFY(audioOutput.processedUSecs() == processedUs);

    audioOutput.resume();

    // Give backends running in separate threads a chance to resume
    // but not too much or the rest of the file may be processed
    QTest::qWait(20);

    // Check that QAudioOutput immediately transitions to IdleState
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit signal after resume(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transition to IdleState after resume()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after resume()");
    stateSignal.clear();

    // Play rest of the clip
    while (!audioFile->atEnd()) {
        if (audioOutput.bytesFree() >= audioOutput.periodSize()) {
            qint64 len = audioFile->read(buffer.data(),audioOutput.periodSize());
            written += feed->write(buffer.constData(), len);
            QVERIFY2((audioOutput.state() == QAudio::ActiveState), "didn't transition to ActiveState after writing audio data");
        } else
            QTest::qWait(20);
    }
    stateSignal.clear();

    // Wait until playback finishes
    QTest::qWait(1000); // 1 seconds should be plenty

    QVERIFY2(audioFile->atEnd(), "didn't play to EOF");
    QVERIFY(stateSignal.count() > 0);
    QCOMPARE(qvariant_cast<QAudio::State>(stateSignal.last().at(0)), QAudio::IdleState);
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transitions to IdleState when at EOF");
    stateSignal.clear();

    processedUs = audioOutput.processedUSecs();

    audioOutput.stop();
    QTest::qWait(40);
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2((processedUs == 2000000),
             QString("processedUSecs() doesn't equal file duration in us (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    QVERIFY2((audioOutput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");

    audioFile->close();
}

void tst_QAudioOutput::pushUnderrun()
{
    QFETCH(FilePtr, audioFile);
    QFETCH(QAudioFormat, audioFormat);

    QAudioOutput audioOutput(audioFormat, this);

    audioOutput.setNotifyInterval(100);
    audioOutput.setVolume(0.1f);

    QSignalSpy notifySignal(&audioOutput, SIGNAL(notify()));
    QSignalSpy stateSignal(&audioOutput, SIGNAL(stateChanged(QAudio::State)));

    // Check that we are in the default state before calling start
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "state() was not set to StoppedState before start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() was not set to QAudio::NoError before start()");
    QVERIFY2((audioOutput.elapsedUSecs() == qint64(0)),"elapsedUSecs() not zero on creation");

    audioFile->close();
    audioFile->open(QIODevice::ReadOnly);
    audioFile->seek(WavHeader::headerLength());

    QIODevice* feed = audioOutput.start();

    // Check that QAudioOutput immediately transitions to IdleState
    QTRY_VERIFY2((stateSignal.count() == 1),
                 QString("didn't emit signal on start(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transition to IdleState after start()");
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after start()");
    QVERIFY(audioOutput.periodSize() > 0);
    stateSignal.clear();

    // Check that 'elapsed' increases
    QTest::qWait(40);
    QVERIFY2((audioOutput.elapsedUSecs() > 0), "elapsedUSecs() is still zero after start()");
    QVERIFY2((audioOutput.processedUSecs() == qint64(0)), "processedUSecs() is not zero after start()");

    qint64 written = 0;
    bool firstBuffer = true;
    QByteArray buffer(AUDIO_BUFFER, 0);

    // Play half of the clip
    while (written < (audioFile->size()-WavHeader::headerLength())/2) {

        if (audioOutput.bytesFree() >= audioOutput.periodSize()) {
            qint64 len = audioFile->read(buffer.data(),audioOutput.periodSize());
            written += feed->write(buffer.constData(), len);

            if (firstBuffer) {
                // Check for transition to ActiveState when data is provided
                QVERIFY2((stateSignal.count() == 1),
                         QString("didn't emit signal after receiving data, got %1 signals instead")
                         .arg(stateSignal.count()).toLocal8Bit().constData());
                QVERIFY2((audioOutput.state() == QAudio::ActiveState), "didn't transition to ActiveState after receiving data");
                QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after receiving data");
                firstBuffer = false;
            }
        } else
            QTest::qWait(20);
    }
    stateSignal.clear();

    // Wait for data to be played
    QTest::qWait(1000);

    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit IdleState signal after suspend(), got %1 signals instead")
             .arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transition to IdleState, no data");
    QVERIFY2((audioOutput.error() == QAudio::UnderrunError), "error state is not equal to QAudio::UnderrunError, no data");
    stateSignal.clear();

    firstBuffer = true;
    // Play rest of the clip
    while (!audioFile->atEnd()) {
        if (audioOutput.bytesFree() >= audioOutput.periodSize()) {
            qint64 len = audioFile->read(buffer.data(),audioOutput.periodSize());
            written += feed->write(buffer.constData(), len);
            if (firstBuffer) {
                // Check for transition to ActiveState when data is provided
                QVERIFY2((stateSignal.count() == 1),
                         QString("didn't emit signal after receiving data, got %1 signals instead")
                         .arg(stateSignal.count()).toLocal8Bit().constData());
                QVERIFY2((audioOutput.state() == QAudio::ActiveState), "didn't transition to ActiveState after receiving data");
                QVERIFY2((audioOutput.error() == QAudio::NoError), "error state is not equal to QAudio::NoError after receiving data");
                firstBuffer = false;
            }
        } else
            QTest::qWait(20);
    }
    stateSignal.clear();

    // Wait until playback finishes
    QTest::qWait(1000); // 1 seconds should be plenty

    QVERIFY2(audioFile->atEnd(), "didn't play to EOF");
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit IdleState signal when at EOF, got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::IdleState), "didn't transitions to IdleState when at EOF");
    stateSignal.clear();

    qint64 processedUs = audioOutput.processedUSecs();

    audioOutput.stop();
    QTest::qWait(40);
    QVERIFY2((stateSignal.count() == 1),
             QString("didn't emit StoppedState signal after stop(), got %1 signals instead").arg(stateSignal.count()).toLocal8Bit().constData());
    QVERIFY2((audioOutput.state() == QAudio::StoppedState), "didn't transitions to StoppedState after stop()");

    QVERIFY2((processedUs == 2000000),
             QString("processedUSecs() doesn't equal file duration in us (%1)").arg(processedUs).toLocal8Bit().constData());
    QVERIFY2((audioOutput.error() == QAudio::NoError), "error() is not QAudio::NoError after stop()");
    QVERIFY2((audioOutput.elapsedUSecs() == (qint64)0), "elapsedUSecs() not equal to zero in StoppedState");

    audioFile->close();
}

void tst_QAudioOutput::volume_data()
{
    QTest::addColumn<float>("actualFloat");
    QTest::addColumn<int>("expectedInt");
    QTest::newRow("Volume 0.3") << 0.3f << 3;
    QTest::newRow("Volume 0.6") << 0.6f << 6;
    QTest::newRow("Volume 0.9") << 0.9f << 9;
}

void tst_QAudioOutput::volume()
{
    QFETCH(float, actualFloat);
    QFETCH(int, expectedInt);
    QAudioOutput audioOutput(audioDevice.preferredFormat(), this);

    audioOutput.setVolume(actualFloat);
    QTRY_VERIFY(qRound(audioOutput.volume()*10.0f) == expectedInt);
    // Wait a while to see if this changes
    QTest::qWait(500);
    QTRY_VERIFY(qRound(audioOutput.volume()*10.0f) == expectedInt);
}

QTEST_MAIN(tst_QAudioOutput)

#include "tst_qaudiooutput.moc"
