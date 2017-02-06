/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtSerialBus/QCanBusFrame>

class tst_QCanBusFrame : public QObject
{
    Q_OBJECT
public:
    explicit tst_QCanBusFrame();

private slots:
    void constructors();
    void id();
    void payload();
    void timeStamp();

    void tst_isValid_data();
    void tst_isValid();

    void tst_toString_data();
    void tst_toString();

    void streaming_data();
    void streaming();

    void tst_error();
};

tst_QCanBusFrame::tst_QCanBusFrame()
{
}

void tst_QCanBusFrame::constructors()
{
    QCanBusFrame frame1;
    QCanBusFrame frame2(123, "tst");
    QCanBusFrame frame3(123456, "tst");
    QCanBusFrame frame4(1234, "tst tst tst");
    QCanBusFrame::TimeStamp timeStamp1;
    QCanBusFrame::TimeStamp timeStamp2(5, 5);

    QVERIFY(frame1.payload().isEmpty());
    QVERIFY(!frame1.frameId());
    QVERIFY(!frame1.hasFlexibleDataRateFormat());
    QVERIFY(!frame1.hasExtendedFrameFormat());
    QCOMPARE(frame1.frameType(), QCanBusFrame::DataFrame);

    QVERIFY(!frame2.payload().isEmpty());
    QVERIFY(frame2.frameId());
    QVERIFY(!frame2.hasFlexibleDataRateFormat());
    QVERIFY(!frame2.hasExtendedFrameFormat());
    QCOMPARE(frame2.frameType(), QCanBusFrame::DataFrame);

    QVERIFY(!frame3.payload().isEmpty());
    QVERIFY(frame3.frameId());
    QVERIFY(!frame3.hasFlexibleDataRateFormat());
    QVERIFY(frame3.hasExtendedFrameFormat());
    QCOMPARE(frame3.frameType(), QCanBusFrame::DataFrame);

    QVERIFY(!frame4.payload().isEmpty());
    QVERIFY(frame4.frameId());
    QVERIFY(frame4.hasFlexibleDataRateFormat());
    QVERIFY(!frame4.hasExtendedFrameFormat());
    QCOMPARE(frame4.frameType(), QCanBusFrame::DataFrame);

    QVERIFY(!timeStamp1.seconds());
    QVERIFY(!timeStamp1.microSeconds());

    QVERIFY(timeStamp2.seconds());
    QVERIFY(timeStamp2.microSeconds());
}

void tst_QCanBusFrame::id()
{
    QCanBusFrame frame;
    QVERIFY(!frame.frameId());
    frame.setExtendedFrameFormat(false);
    frame.setFrameId(2047u);
    QCOMPARE(frame.frameId(), 2047u);
    QVERIFY(frame.isValid());
    QVERIFY(!frame.hasExtendedFrameFormat());
    // id > 2^11 -> extended format
    frame.setExtendedFrameFormat(false);
    frame.setFrameId(2048u);
    QCOMPARE(frame.frameId(), 2048u);
    QVERIFY(frame.isValid());
    QVERIFY(frame.hasExtendedFrameFormat());
    // id < 2^11 -> no extended format
    frame.setExtendedFrameFormat(false);
    frame.setFrameId(512u);
    QCOMPARE(frame.frameId(), 512u);
    QVERIFY(frame.isValid());
    QVERIFY(!frame.hasExtendedFrameFormat());
    // id < 2^11 -> keep extended format
    frame.setExtendedFrameFormat(true);
    frame.setFrameId(512u);
    QCOMPARE(frame.frameId(), 512u);
    QVERIFY(frame.isValid());
    QVERIFY(frame.hasExtendedFrameFormat());
    // id >= 2^29 -> invalid
    frame.setExtendedFrameFormat(false);
    frame.setFrameId(536870912u);
    QCOMPARE(frame.frameId(), 0u);
    QVERIFY(!frame.isValid());
    QVERIFY(!frame.hasExtendedFrameFormat());
}

void tst_QCanBusFrame::payload()
{
    QCanBusFrame frame;
    QVERIFY(frame.payload().isEmpty());
    frame.setPayload("test");
    QCOMPARE(frame.payload().data(), "test");
    QVERIFY(!frame.hasFlexibleDataRateFormat());
    // setting long payload should automatically set hasFlexibleDataRateFormat()
    frame.setPayload("testtesttest");
    QCOMPARE(frame.payload().data(), "testtesttest");
    QVERIFY(frame.hasFlexibleDataRateFormat());
    // setting short payload should not change hasFlexibleDataRateFormat()
    frame.setPayload("test");
    QCOMPARE(frame.payload().data(), "test");
    QVERIFY(frame.hasFlexibleDataRateFormat());
}

void tst_QCanBusFrame::timeStamp()
{
    QCanBusFrame frame;
    QCanBusFrame::TimeStamp timeStamp = frame.timeStamp();
    QVERIFY(!timeStamp.seconds());
    QVERIFY(!timeStamp.microSeconds());

    // fromMicroSeconds: no microsecond overflow
    timeStamp = QCanBusFrame::TimeStamp::fromMicroSeconds(999999);
    QCOMPARE(timeStamp.seconds(), 0);
    QCOMPARE(timeStamp.microSeconds(), 999999);

    // fromMicroSeconds: microsecond overflow
    timeStamp = QCanBusFrame::TimeStamp::fromMicroSeconds(1000000);
    QCOMPARE(timeStamp.seconds(), 1);
    QCOMPARE(timeStamp.microSeconds(), 0);

    // fromMicroSeconds: microsecond overflow
    timeStamp = QCanBusFrame::TimeStamp::fromMicroSeconds(2000001);
    QCOMPARE(timeStamp.seconds(), 2);
    QCOMPARE(timeStamp.microSeconds(), 1);
}

void tst_QCanBusFrame::tst_isValid_data()
{
    QTest::addColumn<QCanBusFrame::FrameType>("frameType");
    QTest::addColumn<bool>("isValid");
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<uint>("id");
    QTest::addColumn<bool>("extended");
    QTest::addColumn<bool>("flexibleData");

    QTest::newRow("invalid frame")
                 << QCanBusFrame::InvalidFrame << false
                 << QByteArray() << 0u << false << false;
    QTest::newRow("data frame")
                 << QCanBusFrame::DataFrame << true
                 << QByteArray() << 0u << false << false;
    QTest::newRow("error frame")
                 << QCanBusFrame::ErrorFrame << true
                 << QByteArray() << 0u << false << false;
    QTest::newRow("remote request frame")
                 << QCanBusFrame::RemoteRequestFrame << true
                 << QByteArray() << 0u << false << false;
    QTest::newRow("remote request CAN FD frame")
                 << QCanBusFrame::RemoteRequestFrame << false
                 << QByteArray() << 0u << false << true;
    QTest::newRow("unknown frame")
                 << QCanBusFrame::UnknownFrame << true
                 << QByteArray() << 0u << false << false;
    QTest::newRow("data frame CAN max payload")
                 << QCanBusFrame::DataFrame << true
                 << QByteArray(8, 0) << 0u << false << false;
    QTest::newRow("data frame CAN FD max payload")
                 << QCanBusFrame::DataFrame << true
                 << QByteArray(64, 0) << 0u << false << true;
    QTest::newRow("data frame CAN too much payload")
                 << QCanBusFrame::DataFrame << false
                 << QByteArray(65, 0) << 0u << false << false;
    QTest::newRow("data frame short id")
                 << QCanBusFrame::DataFrame << true
                 << QByteArray(8, 0) << (1u << 11) - 1 << false << false;
    QTest::newRow("data frame long id")
                 << QCanBusFrame::DataFrame << true
                 << QByteArray(8, 0) << (1u << 11) << true << false;
    QTest::newRow("data frame bad long id")
                 << QCanBusFrame::DataFrame << false
                 << QByteArray(8, 0) << (1u << 11) << false << false;
    QTest::newRow("data frame CAN too long payload")
                 << QCanBusFrame::DataFrame << false
                 << QByteArray(9, 0) << 512u << false << false;
    QTest::newRow("data frame CAN FD long payload")
                 << QCanBusFrame::DataFrame << true
                 << QByteArray(9, 0) << 512u << false << true;
    QTest::newRow("data frame CAN FD too long payload")
                 << QCanBusFrame::DataFrame << false
                 << QByteArray(65, 0) << 512u << false << true;
 }

void tst_QCanBusFrame::tst_isValid()
{
    QFETCH(QCanBusFrame::FrameType, frameType);
    QFETCH(bool, isValid);
    QFETCH(QByteArray, payload);
    QFETCH(uint, id);
    QFETCH(bool, extended);
    QFETCH(bool, flexibleData);

    QCanBusFrame frame(frameType);
    frame.setPayload(payload);
    frame.setFrameId(id);
    frame.setExtendedFrameFormat(extended);
    frame.setFlexibleDataRateFormat(flexibleData);
    QCOMPARE(frame.isValid(), isValid);
    QCOMPARE(frame.frameType(), frameType);
    QCOMPARE(frame.payload(), payload);
    QCOMPARE(frame.frameId(), id);
    QCOMPARE(frame.hasExtendedFrameFormat(), extended);
    QCOMPARE(frame.hasFlexibleDataRateFormat(), flexibleData);

    frame.setFrameType(QCanBusFrame::InvalidFrame);
    QCOMPARE(frame.isValid(), false);
    QCOMPARE(QCanBusFrame::InvalidFrame, frame.frameType());
}

void tst_QCanBusFrame::tst_toString_data()
{
    QTest::addColumn<QCanBusFrame::FrameType>("frameType");
    QTest::addColumn<quint32>("id");
    QTest::addColumn<bool>("extended");
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QString>("expected");

    QTest::newRow("invalid frame")
            << QCanBusFrame::InvalidFrame << 0x0u << false
            << QByteArray()
            << "(Invalid)";
    QTest::newRow("error frame")
            << QCanBusFrame::ErrorFrame << 0x0u << false
            << QByteArray()
            << "(Error)";
    QTest::newRow("unknown frame")
            << QCanBusFrame::UnknownFrame << 0x0u << false
            << QByteArray()
            << "(Unknown)";
    QTest::newRow("remote request frame")
            << QCanBusFrame::RemoteRequestFrame << 0x123u << false
            << QByteArray::fromHex("01") // fake data to get a DLC > 0
            << QString("     123   [1]  Remote Request");
    QTest::newRow("data frame min std id")
            << QCanBusFrame::DataFrame << 0x0u << false
            << QByteArray()
            << QString("     000   [0]");
    QTest::newRow("data frame max std id")
            << QCanBusFrame::DataFrame << 0x7FFu << false
            << QByteArray()
            << QString("     7FF   [0]");
    QTest::newRow("data frame min ext id")
            << QCanBusFrame::DataFrame << 0x0u << true
            << QByteArray()
            << QString("00000000   [0]");
    QTest::newRow("data frame max ext id")
            << QCanBusFrame::DataFrame << 0x1FFFFFFFu << true
            << QByteArray()
            << QString("1FFFFFFF   [0]");
    QTest::newRow("data frame minimal size")
            << QCanBusFrame::DataFrame << 0x7FFu << false
            << QByteArray::fromHex("01")
            << QString("     7FF   [1]  01");
    QTest::newRow("data frame maximal size")
            << QCanBusFrame::DataFrame << 0x1FFFFFFFu << true
            << QByteArray::fromHex("0123456789ABCDEF")
            << QString("1FFFFFFF   [8]  01 23 45 67 89 AB CD EF");
    QTest::newRow("short data frame FD")
            << QCanBusFrame::DataFrame << 0x123u << false
            << QByteArray::fromHex("001122334455667788")
            << QString("     123  [09]  00 11 22 33 44 55 66 77 88");
    QTest::newRow("long data frame FD")
            << QCanBusFrame::DataFrame << 0x123u << false
            << QByteArray::fromHex("00112233445566778899")
            << QString("     123  [10]  00 11 22 33 44 55 66 77 88 99");
}

void tst_QCanBusFrame::tst_toString()
{
    QFETCH(QCanBusFrame::FrameType, frameType);
    QFETCH(quint32, id);
    QFETCH(bool, extended);
    QFETCH(QByteArray, payload);
    QFETCH(QString, expected);
    QCanBusFrame frame;
    frame.setFrameType(frameType);
    frame.setFrameId(id);
    frame.setExtendedFrameFormat(extended);
    frame.setPayload(payload);

    const QString result = frame.toString();

    QCOMPARE(result, expected);
}

void tst_QCanBusFrame::streaming_data()
{
    QTest::addColumn<quint32>("frameId");
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<qint64>("seconds");
    QTest::addColumn<qint64>("microSeconds");
    QTest::addColumn<bool>("isExtended");
    QTest::addColumn<bool>("isFlexibleDataRate");
    QTest::addColumn<QCanBusFrame::FrameType>("frameType");


    QTest::newRow("emptyFrame") << quint32(0) << QByteArray()
                                << qint64(0) << qint64(0)
                                << false << false << QCanBusFrame::DataFrame;
    QTest::newRow("fullFrame1") << quint32(123) << QByteArray("abcde1")
                               << qint64(456) << qint64(784)
                               << true << false << QCanBusFrame::DataFrame;
    QTest::newRow("fullFrame2") << quint32(123) << QByteArray("abcde2")
                               << qint64(457) << qint64(785)
                               << false << false << QCanBusFrame::DataFrame;
    QTest::newRow("fullFrameFD") << quint32(123) << QByteArray("abcdfd")
                                << qint64(457) << qint64(785)
                                << false << true << QCanBusFrame::DataFrame;
    QTest::newRow("fullFrame3") << quint32(123) << QByteArray("abcde3")
                               << qint64(458) << qint64(786)
                               << true << false << QCanBusFrame::RemoteRequestFrame;
    QTest::newRow("fullFrame4") << quint32(123) << QByteArray("abcde4")
                               << qint64(459) << qint64(787)
                               << false << false << QCanBusFrame::RemoteRequestFrame;
    QTest::newRow("fullFrame5") << quint32(123) << QByteArray("abcde5")
                               << qint64(460) << qint64(789)
                               << true << false << QCanBusFrame::ErrorFrame;
    QTest::newRow("fullFrame6") << quint32(123) << QByteArray("abcde6")
                               << qint64(453) << qint64(788)
                               << false << false << QCanBusFrame::ErrorFrame;
}

void tst_QCanBusFrame::streaming()
{
    QFETCH(quint32, frameId);
    QFETCH(QByteArray, payload);
    QFETCH(qint64, seconds);
    QFETCH(qint64, microSeconds);
    QFETCH(bool, isExtended);
    QFETCH(bool, isFlexibleDataRate);
    QFETCH(QCanBusFrame::FrameType, frameType);

    QCanBusFrame originalFrame(frameId, payload);
    const QCanBusFrame::TimeStamp originalStamp(seconds, microSeconds);
    originalFrame.setTimeStamp(originalStamp);

    originalFrame.setExtendedFrameFormat(isExtended);
    originalFrame.setFlexibleDataRateFormat(isFlexibleDataRate);
    originalFrame.setFrameType(frameType);

    QByteArray buffer;
    QDataStream out(&buffer, QIODevice::WriteOnly);
    out << originalFrame;

    QDataStream in(buffer);
    QCanBusFrame restoredFrame;
    in >> restoredFrame;
    const QCanBusFrame::TimeStamp restoredStamp(restoredFrame.timeStamp());

    QCOMPARE(restoredFrame.frameId(), originalFrame.frameId());
    QCOMPARE(restoredFrame.payload(), originalFrame.payload());

    QCOMPARE(restoredStamp.seconds(), originalStamp.seconds());
    QCOMPARE(restoredStamp.microSeconds(), originalStamp.microSeconds());

    QCOMPARE(restoredFrame.frameType(), originalFrame.frameType());
    QCOMPARE(restoredFrame.hasExtendedFrameFormat(),
             originalFrame.hasExtendedFrameFormat());
    QCOMPARE(restoredFrame.hasFlexibleDataRateFormat(),
             originalFrame.hasFlexibleDataRateFormat());
}

void tst_QCanBusFrame::tst_error()
{
    QCanBusFrame frame(1, QByteArray());
    QCOMPARE(frame.frameType(), QCanBusFrame::DataFrame);
    QCOMPARE(frame.frameId(), 1u);
    QCOMPARE(frame.error(), QCanBusFrame::NoError);

    //set error -> should be ignored since still DataFrame
    frame.setError(QCanBusFrame::AnyError);
    QCOMPARE(frame.frameType(), QCanBusFrame::DataFrame);
    QCOMPARE(frame.frameId(), 1u);
    QCOMPARE(frame.error(), QCanBusFrame::NoError);

    frame.setFrameType(QCanBusFrame::ErrorFrame);
    QCOMPARE(frame.frameType(), QCanBusFrame::ErrorFrame);
    QCOMPARE(frame.frameId(), 0u); //id of Error frame always 0
    QCOMPARE(frame.error(), QCanBusFrame::TransmissionTimeoutError);

    frame.setError(QCanBusFrame::FrameErrors(QCanBusFrame::ControllerError|QCanBusFrame::ProtocolViolationError));
    QCOMPARE(frame.frameType(), QCanBusFrame::ErrorFrame);
    QCOMPARE(frame.frameId(), 0u); //id of Error frame always 0
    QCOMPARE(frame.error(),
             QCanBusFrame::ControllerError|QCanBusFrame::ProtocolViolationError);

    frame.setFrameType(QCanBusFrame::RemoteRequestFrame);
    QCOMPARE(frame.frameType(), QCanBusFrame::RemoteRequestFrame);
    QCOMPARE(frame.frameId(), (uint)(QCanBusFrame::ControllerError|QCanBusFrame::ProtocolViolationError));
    QCOMPARE(frame.error(), QCanBusFrame::NoError);
}

QTEST_MAIN(tst_QCanBusFrame)

#include "tst_qcanbusframe.moc"
