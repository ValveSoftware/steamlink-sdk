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

//TESTED_COMPONENT=src/multimedia

#include <QtTest/QtTest>
#include <private/qwavedecoder_p.h>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

class tst_QWaveDecoder : public QObject
{
    Q_OBJECT
public:
    enum Corruption {
        None = 1,
        NotAWav = 2,
        NoSampleData = 4,
        FormatDescriptor = 8,
        FormatString = 16,
        DataDescriptor = 32
    };

public slots:

    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:

    void file_data();
    void file();

    void http_data() {file_data();}
    void http();

    void readAllAtOnce();
    void readPerByte();
};

Q_DECLARE_METATYPE(tst_QWaveDecoder::Corruption)

void tst_QWaveDecoder::init()
{
}

void tst_QWaveDecoder::cleanup()
{
}

void tst_QWaveDecoder::initTestCase()
{
}

void tst_QWaveDecoder::cleanupTestCase()
{
}

static QString testFilePath(const char *filename)
{
    QString path = QString("data/%1").arg(filename);
    return QFINDTESTDATA(path);
}

void tst_QWaveDecoder::file_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<tst_QWaveDecoder::Corruption>("corruption");
    QTest::addColumn<int>("channels");
    QTest::addColumn<int>("samplesize");
    QTest::addColumn<int>("samplerate");
    QTest::addColumn<QAudioFormat::Endian>("byteorder");

    QTest::newRow("File is empty")  << testFilePath("empty.wav") << tst_QWaveDecoder::NotAWav << -1 << -1 << -1 << QAudioFormat::LittleEndian;
    QTest::newRow("File is one byte")  << testFilePath("onebyte.wav") << tst_QWaveDecoder::NotAWav << -1 << -1 << -1 << QAudioFormat::LittleEndian;
    QTest::newRow("File is not a wav(text)")  << testFilePath("notawav.wav") << tst_QWaveDecoder::NotAWav << -1 << -1 << -1 << QAudioFormat::LittleEndian;
    QTest::newRow("Wav file has no sample data")  << testFilePath("nosampledata.wav") << tst_QWaveDecoder::NoSampleData << -1 << -1 << -1 << QAudioFormat::LittleEndian;
    QTest::newRow("corrupt fmt chunk descriptor")  << testFilePath("corrupt_fmtdesc_1_16_8000.le.wav") << tst_QWaveDecoder::FormatDescriptor << -1 << -1 << -1 << QAudioFormat::LittleEndian;
    QTest::newRow("corrupt fmt string")  << testFilePath("corrupt_fmtstring_1_16_8000.le.wav") << tst_QWaveDecoder::FormatString << -1 << -1 << -1 << QAudioFormat::LittleEndian;
    QTest::newRow("corrupt data chunk descriptor")  << testFilePath("corrupt_datadesc_1_16_8000.le.wav") << tst_QWaveDecoder::DataDescriptor << -1 << -1 << -1 << QAudioFormat::LittleEndian;

    QTest::newRow("File isawav_1_8_8000.wav") << testFilePath("isawav_1_8_8000.wav")  << tst_QWaveDecoder::None << 1 << 8 << 8000 << QAudioFormat::LittleEndian;
    QTest::newRow("File isawav_1_8_44100.wav") << testFilePath("isawav_1_8_44100.wav")  << tst_QWaveDecoder::None << 1 << 8 << 44100 << QAudioFormat::LittleEndian;
    QTest::newRow("File isawav_2_8_8000.wav") << testFilePath("isawav_2_8_8000.wav")  << tst_QWaveDecoder::None << 2 << 8 << 8000 << QAudioFormat::LittleEndian;
    QTest::newRow("File isawav_2_8_44100.wav") << testFilePath("isawav_2_8_44100.wav")  << tst_QWaveDecoder::None << 2 << 8 << 44100 << QAudioFormat::LittleEndian;

    QTest::newRow("File isawav_1_16_8000_le.wav") << testFilePath("isawav_1_16_8000_le.wav")  << tst_QWaveDecoder::None << 1 << 16 << 8000 << QAudioFormat::LittleEndian;
    QTest::newRow("File isawav_1_16_44100_le.wav") << testFilePath("isawav_1_16_44100_le.wav")  << tst_QWaveDecoder::None << 1 << 16 << 44100 << QAudioFormat::LittleEndian;
    QTest::newRow("File isawav_2_16_8000_be.wav") << testFilePath("isawav_2_16_8000_be.wav")  << tst_QWaveDecoder::None << 2 << 16 << 8000 << QAudioFormat::BigEndian;
    QTest::newRow("File isawav_2_16_44100_be.wav") << testFilePath("isawav_2_16_44100_be.wav")  << tst_QWaveDecoder::None << 2 << 16 << 44100 << QAudioFormat::BigEndian;
    // The next file has extra data in the wave header.
    QTest::newRow("File isawav_1_16_44100_le_2.wav") << testFilePath("isawav_1_16_44100_le_2.wav")  << tst_QWaveDecoder::None << 1 << 16 << 44100 << QAudioFormat::LittleEndian;

    // 32 bit waves are not supported
    QTest::newRow("File isawav_1_32_8000_le.wav") << testFilePath("isawav_1_32_8000_le.wav")  << tst_QWaveDecoder::FormatDescriptor << 1 << 32 << 8000 << QAudioFormat::LittleEndian;
    QTest::newRow("File isawav_1_32_44100_le.wav") << testFilePath("isawav_1_32_44100_le.wav")  << tst_QWaveDecoder::FormatDescriptor << 1 << 32 << 44100 << QAudioFormat::LittleEndian;
    QTest::newRow("File isawav_2_32_8000_be.wav") << testFilePath("isawav_2_32_8000_be.wav")  << tst_QWaveDecoder::FormatDescriptor << 2 << 32 << 8000 << QAudioFormat::BigEndian;
    QTest::newRow("File isawav_2_32_44100_be.wav") << testFilePath("isawav_2_32_44100_be.wav")  << tst_QWaveDecoder::FormatDescriptor << 2 << 32 << 44100 << QAudioFormat::BigEndian;
}

void tst_QWaveDecoder::file()
{
    QFETCH(QString, file);
    QFETCH(tst_QWaveDecoder::Corruption, corruption);
    QFETCH(int, channels);
    QFETCH(int, samplesize);
    QFETCH(int, samplerate);
    QFETCH(QAudioFormat::Endian, byteorder);

    QFile stream;
    stream.setFileName(file);
    stream.open(QIODevice::ReadOnly);

    QVERIFY(stream.isOpen());

    QWaveDecoder waveDecoder(&stream);
    QSignalSpy validFormatSpy(&waveDecoder, SIGNAL(formatKnown()));
    QSignalSpy parsingErrorSpy(&waveDecoder, SIGNAL(parsingError()));

    if (corruption == NotAWav) {
        QSKIP("Not all failures detected correctly yet");
        QTRY_COMPARE(parsingErrorSpy.count(), 1);
        QCOMPARE(validFormatSpy.count(), 0);
    } else if (corruption == NoSampleData) {
        QTRY_COMPARE(validFormatSpy.count(), 1);
        QCOMPARE(parsingErrorSpy.count(), 0);
        QVERIFY(waveDecoder.audioFormat().isValid());
        QVERIFY(waveDecoder.size() == 0);
        QVERIFY(waveDecoder.duration() == 0);
    } else if (corruption == FormatDescriptor) {
        QTRY_COMPARE(parsingErrorSpy.count(), 1);
        QCOMPARE(validFormatSpy.count(), 0);
    } else if (corruption == FormatString) {
        QTRY_COMPARE(parsingErrorSpy.count(), 1);
        QCOMPARE(validFormatSpy.count(), 0);
        QVERIFY(!waveDecoder.audioFormat().isValid());
    } else if (corruption == DataDescriptor) {
        QTRY_COMPARE(parsingErrorSpy.count(), 1);
        QCOMPARE(validFormatSpy.count(), 0);
        QVERIFY(waveDecoder.size() == 0);
    } else if (corruption == None) {
        QTRY_COMPARE(validFormatSpy.count(), 1);
        QCOMPARE(parsingErrorSpy.count(), 0);
        QVERIFY(waveDecoder.audioFormat().isValid());
        QVERIFY(waveDecoder.size() > 0);
        QVERIFY(waveDecoder.duration() == 250);
        QAudioFormat format = waveDecoder.audioFormat();
        QVERIFY(format.isValid());
        QVERIFY(format.channelCount() == channels);
        QVERIFY(format.sampleSize() == samplesize);
        QVERIFY(format.sampleRate() == samplerate);
        if (format.sampleSize() != 8) {
            QVERIFY(format.byteOrder() == byteorder);
        }
    }

    stream.close();
}

void tst_QWaveDecoder::http()
{
    QFETCH(QString, file);
    QFETCH(tst_QWaveDecoder::Corruption, corruption);
    QFETCH(int, channels);
    QFETCH(int, samplesize);
    QFETCH(int, samplerate);
    QFETCH(QAudioFormat::Endian, byteorder);

    QFile stream;
    stream.setFileName(file);
    stream.open(QIODevice::ReadOnly);

    QVERIFY(stream.isOpen());

    QNetworkAccessManager nam;

    QNetworkReply *reply = nam.get(QNetworkRequest(QUrl::fromLocalFile(file)));

    QWaveDecoder waveDecoder(reply);
    QSignalSpy validFormatSpy(&waveDecoder, SIGNAL(formatKnown()));
    QSignalSpy parsingErrorSpy(&waveDecoder, SIGNAL(parsingError()));

    if (corruption == NotAWav) {
        QSKIP("Not all failures detected correctly yet");
        QTRY_COMPARE(parsingErrorSpy.count(), 1);
        QCOMPARE(validFormatSpy.count(), 0);
    } else if (corruption == NoSampleData) {
        QTRY_COMPARE(validFormatSpy.count(), 1);
        QCOMPARE(parsingErrorSpy.count(), 0);
        QVERIFY(waveDecoder.audioFormat().isValid());
        QVERIFY(waveDecoder.size() == 0);
        QVERIFY(waveDecoder.duration() == 0);
    } else if (corruption == FormatDescriptor) {
        QTRY_COMPARE(parsingErrorSpy.count(), 1);
        QCOMPARE(validFormatSpy.count(), 0);
    } else if (corruption == FormatString) {
        QTRY_COMPARE(parsingErrorSpy.count(), 1);
        QCOMPARE(validFormatSpy.count(), 0);
        QVERIFY(!waveDecoder.audioFormat().isValid());
    } else if (corruption == DataDescriptor) {
        QTRY_COMPARE(parsingErrorSpy.count(), 1);
        QCOMPARE(validFormatSpy.count(), 0);
        QVERIFY(waveDecoder.size() == 0);
    } else if (corruption == None) {
        QTRY_COMPARE(validFormatSpy.count(), 1);
        QCOMPARE(parsingErrorSpy.count(), 0);
        QVERIFY(waveDecoder.audioFormat().isValid());
        QVERIFY(waveDecoder.size() > 0);
        QVERIFY(waveDecoder.duration() == 250);
        QAudioFormat format = waveDecoder.audioFormat();
        QVERIFY(format.isValid());
        QVERIFY(format.channelCount() == channels);
        QVERIFY(format.sampleSize() == samplesize);
        QVERIFY(format.sampleRate() == samplerate);
        if (format.sampleSize() != 8) {
            QVERIFY(format.byteOrder() == byteorder);
        }
    }

    delete reply;
}

void tst_QWaveDecoder::readAllAtOnce()
{
    QFile stream;
    stream.setFileName(testFilePath("isawav_2_8_44100.wav"));
    stream.open(QIODevice::ReadOnly);

    QVERIFY(stream.isOpen());

    QWaveDecoder waveDecoder(&stream);
    QSignalSpy validFormatSpy(&waveDecoder, SIGNAL(formatKnown()));

    QTRY_COMPARE(validFormatSpy.count(), 1);
    QVERIFY(waveDecoder.size() > 0);

    QByteArray buffer;
    buffer.resize(waveDecoder.size());

    qint64 readSize = waveDecoder.read(buffer.data(), waveDecoder.size());
    QVERIFY(readSize == waveDecoder.size());

    readSize = waveDecoder.read(buffer.data(), 1);
    QVERIFY(readSize == 0);

    stream.close();
}

void tst_QWaveDecoder::readPerByte()
{
    QFile stream;
    stream.setFileName(testFilePath("isawav_2_8_44100.wav"));
    stream.open(QIODevice::ReadOnly);

    QVERIFY(stream.isOpen());

    QWaveDecoder waveDecoder(&stream);
    QSignalSpy validFormatSpy(&waveDecoder, SIGNAL(formatKnown()));

    QTRY_COMPARE(validFormatSpy.count(), 1);
    QVERIFY(waveDecoder.size() > 0);

    qint64 readSize = 0;
    char buf;
    for (int ii = 0; ii < waveDecoder.size(); ++ii)
        readSize += waveDecoder.read(&buf, 1);
    QVERIFY(readSize == waveDecoder.size());
    QVERIFY(waveDecoder.read(&buf,1) == 0);

    stream.close();
}

QTEST_MAIN(tst_QWaveDecoder)

#include "tst_qwavedecoder.moc"

