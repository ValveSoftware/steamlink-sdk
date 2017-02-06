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

#include <QtTest/QtTest>
#include <QDebug>
#include "qaudiodecoder.h"

#define TEST_FILE_NAME "testdata/test.wav"
#define TEST_UNSUPPORTED_FILE_NAME "testdata/test-unsupported.avi"
#define TEST_CORRUPTED_FILE_NAME "testdata/test-corrupted.wav"

QT_USE_NAMESPACE

/*
 This is the backend conformance test.

 Since it relies on platform media framework
 it may be less stable.
*/

class tst_QAudioDecoderBackend : public QObject
{
    Q_OBJECT
public slots:
    void init();
    void cleanup();
    void initTestCase();

private slots:
    void fileTest();
    void unsupportedFileTest();
    void corruptedFileTest();
    void deviceTest();
};

void tst_QAudioDecoderBackend::init()
{
}

void tst_QAudioDecoderBackend::initTestCase()
{
    QAudioDecoder d;
    if (!d.isAvailable())
        QSKIP("Audio decoder service is not available");
}

void tst_QAudioDecoderBackend::cleanup()
{
}

void tst_QAudioDecoderBackend::fileTest()
{
    QAudioDecoder d;
    if (d.error() == QAudioDecoder::ServiceMissingError)
        QSKIP("There is no audio decoding support on this platform.");
    QAudioBuffer buffer;
    quint64 duration = 0;
    int byteCount = 0;
    int sampleCount = 0;

    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);
    QCOMPARE(d.sourceFilename(), QString(""));
    QVERIFY(d.audioFormat() == QAudioFormat());

    // Test local file
    QFileInfo fileInfo(QFINDTESTDATA(TEST_FILE_NAME));
    d.setSourceFilename(fileInfo.absoluteFilePath());
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(!d.bufferAvailable());
    QCOMPARE(d.sourceFilename(), fileInfo.absoluteFilePath());

    QSignalSpy readySpy(&d, SIGNAL(bufferReady()));
    QSignalSpy bufferChangedSpy(&d, SIGNAL(bufferAvailableChanged(bool)));
    QSignalSpy errorSpy(&d, SIGNAL(error(QAudioDecoder::Error)));
    QSignalSpy stateSpy(&d, SIGNAL(stateChanged(QAudioDecoder::State)));
    QSignalSpy durationSpy(&d, SIGNAL(durationChanged(qint64)));
    QSignalSpy finishedSpy(&d, SIGNAL(finished()));
    QSignalSpy positionSpy(&d, SIGNAL(positionChanged(qint64)));

    d.start();
    QTRY_VERIFY(d.state() == QAudioDecoder::DecodingState);
    QTRY_VERIFY(!stateSpy.isEmpty());
    QTRY_VERIFY(!readySpy.isEmpty());
    QTRY_VERIFY(!bufferChangedSpy.isEmpty());
    QVERIFY(d.bufferAvailable());
    QTRY_VERIFY(!durationSpy.isEmpty());
    QVERIFY(qAbs(d.duration() - 1000) < 20);

    buffer = d.read();
    QVERIFY(buffer.isValid());

    // Test file is 44.1K 16bit mono, 44094 samples
    QCOMPARE(buffer.format().channelCount(), 1);
    QCOMPARE(buffer.format().sampleRate(), 44100);
    QCOMPARE(buffer.format().sampleSize(), 16);
    QCOMPARE(buffer.format().sampleType(), QAudioFormat::SignedInt);
    QCOMPARE(buffer.format().codec(), QString("audio/pcm"));
    QCOMPARE(buffer.byteCount(), buffer.sampleCount() * 2); // 16bit mono

    // The decoder should still have no format set
    QVERIFY(d.audioFormat() == QAudioFormat());

    QVERIFY(errorSpy.isEmpty());

    duration += buffer.duration();
    sampleCount += buffer.sampleCount();
    byteCount += buffer.byteCount();

    // Now drain the decoder
    if (sampleCount < 44094) {
        QTRY_COMPARE(d.bufferAvailable(), true);
    }

    while (d.bufferAvailable()) {
        buffer = d.read();
        QVERIFY(buffer.isValid());
        QTRY_VERIFY(!positionSpy.isEmpty());
        QVERIFY(positionSpy.takeLast().at(0).toLongLong() == qint64(duration / 1000));

        duration += buffer.duration();
        sampleCount += buffer.sampleCount();
        byteCount += buffer.byteCount();

        if (sampleCount < 44094) {
            QTRY_COMPARE(d.bufferAvailable(), true);
        }
    }

    // Make sure the duration is roughly correct (+/- 20ms)
    QCOMPARE(sampleCount, 44094);
    QCOMPARE(byteCount, 44094 * 2);
    QVERIFY(qAbs(qint64(duration) - 1000000) < 20000);
    QVERIFY(qAbs((d.position() + (buffer.duration() / 1000)) - 1000) < 20);
    QTRY_COMPARE(finishedSpy.count(), 1);
    QVERIFY(!d.bufferAvailable());
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);

    d.stop();
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);
    QTRY_COMPARE(durationSpy.count(), 2);
    QCOMPARE(d.duration(), qint64(-1));
    QVERIFY(!d.bufferAvailable());
    readySpy.clear();
    bufferChangedSpy.clear();
    stateSpy.clear();
    durationSpy.clear();
    finishedSpy.clear();
    positionSpy.clear();

    // change output audio format
    QAudioFormat format;
    format.setChannelCount(2);
    format.setSampleSize(8);
    format.setSampleRate(11050);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);

    d.setAudioFormat(format);

    // We expect 1 second still, at 11050 * 2 samples == 22k samples.
    // (at 1 byte/sample -> 22kb)

    // Make sure it stuck
    QVERIFY(d.audioFormat() == format);

    duration = 0;
    sampleCount = 0;
    byteCount = 0;

    d.start();
    QTRY_VERIFY(d.state() == QAudioDecoder::DecodingState);
    QTRY_VERIFY(!stateSpy.isEmpty());
    QTRY_VERIFY(!readySpy.isEmpty());
    QTRY_VERIFY(!bufferChangedSpy.isEmpty());
    QVERIFY(d.bufferAvailable());
    QTRY_VERIFY(!durationSpy.isEmpty());
    QVERIFY(qAbs(d.duration() - 1000) < 20);

    buffer = d.read();
    QVERIFY(buffer.isValid());
    // See if we got the right format
    QVERIFY(buffer.format() == format);

    // The decoder should still have the same format
    QVERIFY(d.audioFormat() == format);

    QVERIFY(errorSpy.isEmpty());

    duration += buffer.duration();
    sampleCount += buffer.sampleCount();
    byteCount += buffer.byteCount();

    // Now drain the decoder
    if (duration < 998000) {
        QTRY_COMPARE(d.bufferAvailable(), true);
    }

    while (d.bufferAvailable()) {
        buffer = d.read();
        QVERIFY(buffer.isValid());
        QTRY_VERIFY(!positionSpy.isEmpty());
        QVERIFY(positionSpy.takeLast().at(0).toLongLong() == qint64(duration / 1000));
        QVERIFY(d.position() - (duration / 1000) < 20);

        duration += buffer.duration();
        sampleCount += buffer.sampleCount();
        byteCount += buffer.byteCount();

        if (duration < 998000) {
            QTRY_COMPARE(d.bufferAvailable(), true);
        }
    }

    // Resampling might end up with fewer or more samples
    // so be a bit sloppy
    QVERIFY(qAbs(sampleCount - 22047) < 100);
    QVERIFY(qAbs(byteCount - 22047) < 100);
    QVERIFY(qAbs(qint64(duration) - 1000000) < 20000);
    QVERIFY(qAbs((d.position() + (buffer.duration() / 1000)) - 1000) < 20);
    QTRY_COMPARE(finishedSpy.count(), 1);
    QVERIFY(!d.bufferAvailable());
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);

    d.stop();
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);
    QTRY_COMPARE(durationSpy.count(), 2);
    QCOMPARE(d.duration(), qint64(-1));
    QVERIFY(!d.bufferAvailable());
}

/*
 The avi file has an audio stream not supported by any codec.
*/
void tst_QAudioDecoderBackend::unsupportedFileTest()
{
    QAudioDecoder d;
    if (d.error() == QAudioDecoder::ServiceMissingError)
        QSKIP("There is no audio decoding support on this platform.");
    QAudioBuffer buffer;

    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);
    QCOMPARE(d.sourceFilename(), QString(""));
    QVERIFY(d.audioFormat() == QAudioFormat());

    // Test local file
    QFileInfo fileInfo(QFINDTESTDATA(TEST_UNSUPPORTED_FILE_NAME));
    d.setSourceFilename(fileInfo.absoluteFilePath());
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(!d.bufferAvailable());
    QCOMPARE(d.sourceFilename(), fileInfo.absoluteFilePath());

    QSignalSpy readySpy(&d, SIGNAL(bufferReady()));
    QSignalSpy bufferChangedSpy(&d, SIGNAL(bufferAvailableChanged(bool)));
    QSignalSpy errorSpy(&d, SIGNAL(error(QAudioDecoder::Error)));
    QSignalSpy stateSpy(&d, SIGNAL(stateChanged(QAudioDecoder::State)));
    QSignalSpy durationSpy(&d, SIGNAL(durationChanged(qint64)));
    QSignalSpy finishedSpy(&d, SIGNAL(finished()));
    QSignalSpy positionSpy(&d, SIGNAL(positionChanged(qint64)));

    d.start();
    QTRY_VERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(!d.bufferAvailable());
    QCOMPARE(d.audioFormat(), QAudioFormat());
    QCOMPARE(d.duration(), qint64(-1));
    QCOMPARE(d.position(), qint64(-1));

    // Check the error code.
    QTRY_VERIFY(!errorSpy.isEmpty());

    // Have to use qvariant_cast, toInt will return 0 because unrecognized type;
    QAudioDecoder::Error errorCode = qvariant_cast<QAudioDecoder::Error>(errorSpy.takeLast().at(0));
    QCOMPARE(errorCode, QAudioDecoder::FormatError);
    QCOMPARE(d.error(), QAudioDecoder::FormatError);

    // Check all other spies.
    QVERIFY(readySpy.isEmpty());
    QVERIFY(bufferChangedSpy.isEmpty());
    QVERIFY(stateSpy.isEmpty());
    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(positionSpy.isEmpty());
    QVERIFY(durationSpy.isEmpty());

    errorSpy.clear();

    // Try read even if the file is not supported to test robustness.
    buffer = d.read();
    QTRY_VERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(!buffer.isValid());
    QVERIFY(!d.bufferAvailable());
    QCOMPARE(d.position(), qint64(-1));

    QVERIFY(errorSpy.isEmpty());
    QVERIFY(readySpy.isEmpty());
    QVERIFY(bufferChangedSpy.isEmpty());
    QVERIFY(stateSpy.isEmpty());
    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(positionSpy.isEmpty());
    QVERIFY(durationSpy.isEmpty());


    d.stop();
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);
    QCOMPARE(d.duration(), qint64(-1));
    QVERIFY(!d.bufferAvailable());
}

/*
 The corrupted file is generated by copying a few random numbers
 from /dev/random on a linux machine.
*/
void tst_QAudioDecoderBackend::corruptedFileTest()
{
    QAudioDecoder d;
    if (d.error() == QAudioDecoder::ServiceMissingError)
        QSKIP("There is no audio decoding support on this platform.");
    QAudioBuffer buffer;

    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);
    QCOMPARE(d.sourceFilename(), QString(""));
    QVERIFY(d.audioFormat() == QAudioFormat());

    // Test local file
    QFileInfo fileInfo(QFINDTESTDATA(TEST_CORRUPTED_FILE_NAME));
    d.setSourceFilename(fileInfo.absoluteFilePath());
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(!d.bufferAvailable());
    QCOMPARE(d.sourceFilename(), fileInfo.absoluteFilePath());

    QSignalSpy readySpy(&d, SIGNAL(bufferReady()));
    QSignalSpy bufferChangedSpy(&d, SIGNAL(bufferAvailableChanged(bool)));
    QSignalSpy errorSpy(&d, SIGNAL(error(QAudioDecoder::Error)));
    QSignalSpy stateSpy(&d, SIGNAL(stateChanged(QAudioDecoder::State)));
    QSignalSpy durationSpy(&d, SIGNAL(durationChanged(qint64)));
    QSignalSpy finishedSpy(&d, SIGNAL(finished()));
    QSignalSpy positionSpy(&d, SIGNAL(positionChanged(qint64)));

    d.start();
    QTRY_VERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(!d.bufferAvailable());
    QCOMPARE(d.audioFormat(), QAudioFormat());
    QCOMPARE(d.duration(), qint64(-1));
    QCOMPARE(d.position(), qint64(-1));

    // Check the error code.
    QTRY_VERIFY(!errorSpy.isEmpty());

    // Have to use qvariant_cast, toInt will return 0 because unrecognized type;
    QAudioDecoder::Error errorCode = qvariant_cast<QAudioDecoder::Error>(errorSpy.takeLast().at(0));
    QCOMPARE(errorCode, QAudioDecoder::FormatError);
    QCOMPARE(d.error(), QAudioDecoder::FormatError);

    // Check all other spies.
    QVERIFY(readySpy.isEmpty());
    QVERIFY(bufferChangedSpy.isEmpty());
    QVERIFY(stateSpy.isEmpty());
    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(positionSpy.isEmpty());
    QVERIFY(durationSpy.isEmpty());

    errorSpy.clear();

    // Try read even if the file is corrupted to test the robustness.
    buffer = d.read();
    QTRY_VERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(!buffer.isValid());
    QVERIFY(!d.bufferAvailable());
    QCOMPARE(d.position(), qint64(-1));

    QVERIFY(errorSpy.isEmpty());
    QVERIFY(readySpy.isEmpty());
    QVERIFY(bufferChangedSpy.isEmpty());
    QVERIFY(stateSpy.isEmpty());
    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(positionSpy.isEmpty());
    QVERIFY(durationSpy.isEmpty());


    d.stop();
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);
    QCOMPARE(d.duration(), qint64(-1));
    QVERIFY(!d.bufferAvailable());
}

void tst_QAudioDecoderBackend::deviceTest()
{
    QAudioDecoder d;
    if (d.error() == QAudioDecoder::ServiceMissingError)
        QSKIP("There is no audio decoding support on this platform.");
    QAudioBuffer buffer;
    quint64 duration = 0;
    int sampleCount = 0;

    QSignalSpy readySpy(&d, SIGNAL(bufferReady()));
    QSignalSpy bufferChangedSpy(&d, SIGNAL(bufferAvailableChanged(bool)));
    QSignalSpy errorSpy(&d, SIGNAL(error(QAudioDecoder::Error)));
    QSignalSpy stateSpy(&d, SIGNAL(stateChanged(QAudioDecoder::State)));
    QSignalSpy durationSpy(&d, SIGNAL(durationChanged(qint64)));
    QSignalSpy finishedSpy(&d, SIGNAL(finished()));
    QSignalSpy positionSpy(&d, SIGNAL(positionChanged(qint64)));

    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);
    QCOMPARE(d.sourceFilename(), QString(""));
    QVERIFY(d.audioFormat() == QAudioFormat());

    QFileInfo fileInfo(QFINDTESTDATA(TEST_FILE_NAME));
    QFile file(fileInfo.absoluteFilePath());
    QVERIFY(file.open(QIODevice::ReadOnly));
    d.setSourceDevice(&file);

    QVERIFY(d.sourceDevice() == &file);
    QVERIFY(d.sourceFilename().isEmpty());

    // We haven't set the format yet
    QVERIFY(d.audioFormat() == QAudioFormat());

    d.start();
    QTRY_VERIFY(d.state() == QAudioDecoder::DecodingState);
    QTRY_VERIFY(!stateSpy.isEmpty());
    QTRY_VERIFY(!readySpy.isEmpty());
    QTRY_VERIFY(!bufferChangedSpy.isEmpty());
    QVERIFY(d.bufferAvailable());
    QTRY_VERIFY(!durationSpy.isEmpty());
    QVERIFY(qAbs(d.duration() - 1000) < 20);

    buffer = d.read();
    QVERIFY(buffer.isValid());

    // Test file is 44.1K 16bit mono
    QCOMPARE(buffer.format().channelCount(), 1);
    QCOMPARE(buffer.format().sampleRate(), 44100);
    QCOMPARE(buffer.format().sampleSize(), 16);
    QCOMPARE(buffer.format().sampleType(), QAudioFormat::SignedInt);
    QCOMPARE(buffer.format().codec(), QString("audio/pcm"));

    QVERIFY(errorSpy.isEmpty());

    duration += buffer.duration();
    sampleCount += buffer.sampleCount();

    // Now drain the decoder
    if (sampleCount < 44094) {
        QTRY_COMPARE(d.bufferAvailable(), true);
    }

    while (d.bufferAvailable()) {
        buffer = d.read();
        QVERIFY(buffer.isValid());
        QTRY_VERIFY(!positionSpy.isEmpty());
        QVERIFY(positionSpy.takeLast().at(0).toLongLong() == qint64(duration / 1000));
        QVERIFY(d.position() - (duration / 1000) < 20);

        duration += buffer.duration();
        sampleCount += buffer.sampleCount();
        if (sampleCount < 44094) {
            QTRY_COMPARE(d.bufferAvailable(), true);
        }
    }

    // Make sure the duration is roughly correct (+/- 20ms)
    QCOMPARE(sampleCount, 44094);
    QVERIFY(qAbs(qint64(duration) - 1000000) < 20000);
    QVERIFY(qAbs((d.position() + (buffer.duration() / 1000)) - 1000) < 20);
    QTRY_COMPARE(finishedSpy.count(), 1);
    QVERIFY(!d.bufferAvailable());
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);

    d.stop();
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);
    QVERIFY(!d.bufferAvailable());
    QTRY_COMPARE(durationSpy.count(), 2);
    QCOMPARE(d.duration(), qint64(-1));
    readySpy.clear();
    bufferChangedSpy.clear();
    stateSpy.clear();
    durationSpy.clear();
    finishedSpy.clear();
    positionSpy.clear();

    // Now try changing formats
    QAudioFormat format;
    format.setChannelCount(2);
    format.setSampleSize(8);
    format.setSampleRate(8000);
    format.setCodec("audio/pcm");
    format.setSampleType(QAudioFormat::SignedInt);

    d.setAudioFormat(format);

    // Make sure it stuck
    QVERIFY(d.audioFormat() == format);

    d.start();
    QTRY_VERIFY(d.state() == QAudioDecoder::DecodingState);
    QTRY_VERIFY(!stateSpy.isEmpty());
    QTRY_VERIFY(!readySpy.isEmpty());
    QTRY_VERIFY(!bufferChangedSpy.isEmpty());
    QVERIFY(d.bufferAvailable());
    QTRY_VERIFY(!durationSpy.isEmpty());
    QVERIFY(qAbs(d.duration() - 1000) < 20);

    buffer = d.read();
    QVERIFY(buffer.isValid());
    // See if we got the right format
    QVERIFY(buffer.format() == format);

    // The decoder should still have the same format
    QVERIFY(d.audioFormat() == format);

    QVERIFY(errorSpy.isEmpty());

    d.stop();
    QTRY_COMPARE(d.state(), QAudioDecoder::StoppedState);
    QVERIFY(!d.bufferAvailable());
    QTRY_COMPARE(durationSpy.count(), 2);
    QCOMPARE(d.duration(), qint64(-1));
}

QTEST_MAIN(tst_QAudioDecoderBackend)

#include "tst_qaudiodecoderbackend.moc"
