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


#include <QtCore/QString>
#include <QtTest/QtTest>

#include "qaudiodecoder.h"
#include "mockaudiodecoderservice.h"
#include "mockmediaserviceprovider.h"

class tst_QAudioDecoder : public QObject
{
    Q_OBJECT

public:
    tst_QAudioDecoder();

private Q_SLOTS:
    void init();
    void ctors();
    void read();
    void stop();
    void format();
    void source();
    void readAll();
    void nullControl();
    void nullService();

private:
    MockAudioDecoderService  *mockAudioDecoderService;
    MockMediaServiceProvider *mockProvider;
};

tst_QAudioDecoder::tst_QAudioDecoder()
{
}

void tst_QAudioDecoder::init()
{
    mockAudioDecoderService = new MockAudioDecoderService(this);
    mockProvider = new MockMediaServiceProvider(mockAudioDecoderService);

    QMediaServiceProvider::setDefaultServiceProvider(mockProvider);
}

void tst_QAudioDecoder::ctors()
{
    QAudioDecoder d;
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);
    QCOMPARE(d.sourceFilename(), QString(""));

    d.setSourceFilename("");
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);
    QCOMPARE(d.sourceFilename(), QString(""));
}

void tst_QAudioDecoder::read()
{
    QAudioDecoder d;
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);

    QSignalSpy readySpy(&d, SIGNAL(bufferReady()));
    QSignalSpy bufferChangedSpy(&d, SIGNAL(bufferAvailableChanged(bool)));
    QSignalSpy errorSpy(&d, SIGNAL(error(QAudioDecoder::Error)));

    // Starting with empty source == error
    d.start();

    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);

    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(bufferChangedSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 1);

    // Set the source to something
    d.setSourceFilename("Blah");
    QCOMPARE(d.sourceFilename(), QString("Blah"));

    readySpy.clear();
    errorSpy.clear();
    bufferChangedSpy.clear();

    d.start();
    QCOMPARE(d.state(), QAudioDecoder::DecodingState);
    QCOMPARE(d.bufferAvailable(), false); // not yet

    // Try to read
    QAudioBuffer b = d.read();
    QVERIFY(!b.isValid());

    // Read again with no parameter
    b = d.read();
    QVERIFY(!b.isValid());

    // Wait a while
    QTRY_VERIFY(d.bufferAvailable());

    QVERIFY(d.bufferAvailable());

    b = d.read();
    QVERIFY(b.format().isValid());
    QVERIFY(b.isValid());
    QVERIFY(b.format().channelCount() == 1);
    QVERIFY(b.sampleCount() == 4);

    QVERIFY(readySpy.count() >= 1);
    QVERIFY(errorSpy.count() == 0);

    if (d.bufferAvailable()) {
        QVERIFY(bufferChangedSpy.count() == 1);
    } else {
        QVERIFY(bufferChangedSpy.count() == 2);
    }
}

void tst_QAudioDecoder::stop()
{
    QAudioDecoder d;
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);

    QSignalSpy readySpy(&d, SIGNAL(bufferReady()));
    QSignalSpy bufferChangedSpy(&d, SIGNAL(bufferAvailableChanged(bool)));
    QSignalSpy errorSpy(&d, SIGNAL(error(QAudioDecoder::Error)));

    // Starting with empty source == error
    d.start();

    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);

    QCOMPARE(readySpy.count(), 0);
    QCOMPARE(bufferChangedSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 1);

    // Set the source to something
    d.setSourceFilename("Blah");
    QCOMPARE(d.sourceFilename(), QString("Blah"));

    readySpy.clear();
    errorSpy.clear();
    bufferChangedSpy.clear();

    d.start();
    QCOMPARE(d.state(), QAudioDecoder::DecodingState);
    QCOMPARE(d.bufferAvailable(), false); // not yet

    // Try to read
    QAudioBuffer b = d.read();
    QVERIFY(!b.isValid());

    // Read again with no parameter
    b = d.read();
    QVERIFY(!b.isValid());

    // Wait a while
    QTRY_VERIFY(d.bufferAvailable());

    QVERIFY(d.bufferAvailable());

    // Now stop
    d.stop();

    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);
}

void tst_QAudioDecoder::format()
{
    QAudioDecoder d;
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    QVERIFY(d.bufferAvailable() == false);

    QSignalSpy readySpy(&d, SIGNAL(bufferReady()));
    QSignalSpy bufferChangedSpy(&d, SIGNAL(bufferAvailableChanged(bool)));
    QSignalSpy errorSpy(&d, SIGNAL(error(QAudioDecoder::Error)));

    // Set the source to something
    d.setSourceFilename("Blah");
    QCOMPARE(d.sourceFilename(), QString("Blah"));

    readySpy.clear();
    errorSpy.clear();
    bufferChangedSpy.clear();

    d.start();
    QCOMPARE(d.state(), QAudioDecoder::DecodingState);
    QCOMPARE(d.bufferAvailable(), false); // not yet

    // Try to read
    QAudioBuffer b = d.read();
    QVERIFY(!b.isValid());

    // Read again with no parameter
    b = d.read();
    QVERIFY(!b.isValid());

    // Wait a while
    QTRY_VERIFY(d.bufferAvailable());

    b = d.read();
    QVERIFY(d.audioFormat() == b.format());

    // Setting format while decoding is forbidden
    QAudioFormat f(d.audioFormat());
    f.setChannelCount(2);

    d.setAudioFormat(f);
    QVERIFY(d.audioFormat() != f);
    QVERIFY(d.audioFormat() == b.format());

    // Now stop, and set something specific
    d.stop();
    d.setAudioFormat(f);
    QVERIFY(d.audioFormat() == f);

    // Decode again
    d.start();
    QTRY_VERIFY(d.bufferAvailable());

    b = d.read();
    QVERIFY(d.audioFormat() == f);
    QVERIFY(b.format() == f);
}

void tst_QAudioDecoder::source()
{
    QAudioDecoder d;

    QVERIFY(d.sourceFilename().isEmpty());
    QVERIFY(d.sourceDevice() == 0);

    QFile f;
    d.setSourceDevice(&f);
    QVERIFY(d.sourceFilename().isEmpty());
    QVERIFY(d.sourceDevice() == &f);

    d.setSourceFilename("Foo");
    QVERIFY(d.sourceFilename() == QString("Foo"));
    QVERIFY(d.sourceDevice() == 0);

    d.setSourceDevice(0);
    QVERIFY(d.sourceFilename().isEmpty());
    QVERIFY(d.sourceDevice() == 0);

    d.setSourceFilename("Foo");
    QVERIFY(d.sourceFilename() == QString("Foo"));
    QVERIFY(d.sourceDevice() == 0);

    d.setSourceFilename(QString());
    QVERIFY(d.sourceFilename() == QString());
    QVERIFY(d.sourceDevice() == 0);
}

void tst_QAudioDecoder::readAll()
{
    QAudioDecoder d;
    d.setSourceFilename("Foo");
    QVERIFY(d.state() == QAudioDecoder::StoppedState);

    QSignalSpy durationSpy(&d, SIGNAL(durationChanged(qint64)));
    QSignalSpy positionSpy(&d, SIGNAL(positionChanged(qint64)));
    QSignalSpy stateSpy(&d, SIGNAL(stateChanged(QAudioDecoder::State)));
    QSignalSpy finishedSpy(&d, SIGNAL(finished()));
    QSignalSpy bufferAvailableSpy(&d, SIGNAL(bufferAvailableChanged(bool)));
    d.start();
    int i = 0;
    forever {
        QVERIFY(d.state() == QAudioDecoder::DecodingState);
        QCOMPARE(stateSpy.count(), 1);
        QCOMPARE(durationSpy.count(), 1);
        QVERIFY(finishedSpy.isEmpty());
        QTRY_VERIFY(bufferAvailableSpy.count() >= 1);
        if (d.bufferAvailable()) {
            QAudioBuffer b = d.read();
            QVERIFY(b.isValid());
            QCOMPARE(b.startTime() / 1000, d.position());
            QVERIFY(!positionSpy.isEmpty());
            QList<QVariant> arguments = positionSpy.takeLast();
            QCOMPARE(arguments.at(0).toLongLong(), b.startTime() / 1000);

            i++;
            if (i == MOCK_DECODER_MAX_BUFFERS) {
                QCOMPARE(finishedSpy.count(), 1);
                QCOMPARE(stateSpy.count(), 2);
                QVERIFY(d.state() == QAudioDecoder::StoppedState);
                QList<QVariant> arguments = stateSpy.takeLast();
                QVERIFY(arguments.at(0).toInt() == (int)QAudioDecoder::StoppedState);
                QVERIFY(!d.bufferAvailable());
                QVERIFY(!bufferAvailableSpy.isEmpty());
                arguments = bufferAvailableSpy.takeLast();
                QVERIFY(arguments.at(0).toBool() == false);
                break;
            }
        } else
            QTest::qWait(30);
    }
}

void tst_QAudioDecoder::nullControl()
{
    mockAudioDecoderService->setControlNull();
    QAudioDecoder d;

    QVERIFY(d.error() == QAudioDecoder::ServiceMissingError);
    QVERIFY(!d.errorString().isEmpty());

    QVERIFY(d.hasSupport("MIME") == QMultimedia::MaybeSupported);

    QVERIFY(d.state() == QAudioDecoder::StoppedState);

    QVERIFY(d.sourceFilename().isEmpty());
    d.setSourceFilename("test");
    QVERIFY(d.sourceFilename().isEmpty());

    QFile f;
    QVERIFY(d.sourceDevice() == 0);
    d.setSourceDevice(&f);
    QVERIFY(d.sourceDevice() == 0);

    QAudioFormat format;
    format.setChannelCount(2);
    QVERIFY(!d.audioFormat().isValid());
    d.setAudioFormat(format);
    QVERIFY(!d.audioFormat().isValid());

    QVERIFY(!d.read().isValid());
    QVERIFY(!d.bufferAvailable());

    QVERIFY(d.position() == -1);
    QVERIFY(d.duration() == -1);

    d.start();
    QVERIFY(d.error() == QAudioDecoder::ServiceMissingError);
    QVERIFY(!d.errorString().isEmpty());
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    d.stop();
}


void tst_QAudioDecoder::nullService()
{
    mockProvider->service = 0;
    QAudioDecoder d;

    QVERIFY(d.error() == QAudioDecoder::ServiceMissingError);
    QVERIFY(!d.errorString().isEmpty());

    QVERIFY(d.hasSupport("MIME") == QMultimedia::MaybeSupported);

    QVERIFY(d.state() == QAudioDecoder::StoppedState);

    QVERIFY(d.sourceFilename().isEmpty());
    d.setSourceFilename("test");
    QVERIFY(d.sourceFilename().isEmpty());

    QFile f;
    QVERIFY(d.sourceDevice() == 0);
    d.setSourceDevice(&f);
    QVERIFY(d.sourceDevice() == 0);

    QAudioFormat format;
    format.setChannelCount(2);
    QVERIFY(!d.audioFormat().isValid());
    d.setAudioFormat(format);
    QVERIFY(!d.audioFormat().isValid());

    QVERIFY(!d.read().isValid());
    QVERIFY(!d.bufferAvailable());

    QVERIFY(d.position() == -1);
    QVERIFY(d.duration() == -1);

    d.start();
    QVERIFY(d.error() == QAudioDecoder::ServiceMissingError);
    QVERIFY(!d.errorString().isEmpty());
    QVERIFY(d.state() == QAudioDecoder::StoppedState);
    d.stop();
}

QTEST_MAIN(tst_QAudioDecoder)

#include "tst_qaudiodecoder.moc"
