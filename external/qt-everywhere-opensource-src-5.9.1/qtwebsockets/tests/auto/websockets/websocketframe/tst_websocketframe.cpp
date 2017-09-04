/****************************************************************************
**
** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
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
#include <QtTest/qtestcase.h>
#include <QDebug>
#include <QByteArray>
#include <QtEndian>

#include "private/qwebsocketframe_p.h"
#include "private/qwebsocketprotocol_p.h"
#include "qwebsocketprotocol.h"

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QWebSocketProtocol::CloseCode)
Q_DECLARE_METATYPE(QWebSocketProtocol::OpCode)

/*!
 * \brief class FrameHelper is used to encode a single frame.
 *
 * \internal
 */
class FrameHelper
{
public:
    FrameHelper();

    QByteArray wireRepresentation();

    void setRsv1(int value) { m_rsv1 = value; }
    void setRsv2(int value) { m_rsv2 = value; }
    void setRsv3(int value) { m_rsv3 = value; }
    void setMask(quint32 mask) { m_mask = mask; }
    void setOpCode(QWebSocketProtocol::OpCode opCode) { m_opCode = opCode; }
    void setPayload(const QByteArray &payload) { m_payload = payload; }
    void setFinalFrame(bool isFinal) { m_isFinalFrame = isFinal; }

private:
    int m_rsv1;
    int m_rsv2;
    int m_rsv3;
    quint32 m_mask;
    QWebSocketProtocol::OpCode m_opCode;
    QByteArray m_payload;
    bool m_isFinalFrame;
};

FrameHelper::FrameHelper() :
    m_rsv1(0), m_rsv2(0), m_rsv3(0),
    m_mask(0), m_opCode(QWebSocketProtocol::OpCodeReserved3),
    m_payload(), m_isFinalFrame(false)
{}

QByteArray FrameHelper::wireRepresentation()
{
    quint8 byte = 0x00;
    QByteArray wireRep;
    quint64 payloadLength = m_payload.length();

    //FIN, opcode
    byte = static_cast<quint8>((m_opCode & 0x0F) | (m_isFinalFrame ? 0x80 : 0x00)); //FIN, opcode
    //RSV1-3
    byte |= static_cast<quint8>(((m_rsv1 & 0x01) << 6) | ((m_rsv2 & 0x01) << 5) |
                                ((m_rsv3 & 0x01) << 4));
    wireRep.append(static_cast<char>(byte));

    byte = 0x00;
    if (m_mask != 0)
    {
        byte |= 0x80;
    }
    if (payloadLength <= 125)
    {
        byte |= static_cast<quint8>(payloadLength);
        wireRep.append(static_cast<char>(byte));
    }
    else if (payloadLength <= 0xFFFFU)
    {
        byte |= 126;
        wireRep.append(static_cast<char>(byte));
        quint16 swapped = qToBigEndian<quint16>(static_cast<quint16>(payloadLength));
        wireRep.append(static_cast<const char *>(static_cast<const void *>(&swapped)), 2);
    }
    else
    {
        byte |= 127;
        wireRep.append(static_cast<char>(byte));
        quint64 swapped = qToBigEndian<quint64>(payloadLength);
        wireRep.append(static_cast<const char *>(static_cast<const void *>(&swapped)), 8);
    }
    //Write mask
    if (m_mask != 0)
    {
        wireRep.append(static_cast<const char *>(static_cast<const void *>(&m_mask)),
                       sizeof(quint32));
    }
    QByteArray tmpData = m_payload;
    if (m_mask)
    {
        tmpData.detach();
        QWebSocketProtocol::mask(&tmpData, m_mask);
    }
    wireRep.append(tmpData);
    return wireRep;
}

class tst_WebSocketFrame : public QObject
{
    Q_OBJECT

public:
    tst_WebSocketFrame();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void tst_initialization();
    void tst_copyConstructorAndAssignment();

    void tst_goodFrames_data();
    void tst_goodFrames();

    void tst_invalidFrames_data();
    void tst_invalidFrames();

    void tst_malformedFrames_data();
    void tst_malformedFrames();
};

tst_WebSocketFrame::tst_WebSocketFrame()
{}

void tst_WebSocketFrame::initTestCase()
{
}

void tst_WebSocketFrame::cleanupTestCase()
{}

void tst_WebSocketFrame::init()
{
    qRegisterMetaType<QWebSocketProtocol::OpCode>("QWebSocketProtocol::OpCode");
    qRegisterMetaType<QWebSocketProtocol::CloseCode>("QWebSocketProtocol::CloseCode");
}

void tst_WebSocketFrame::cleanup()
{
}

void tst_WebSocketFrame::tst_initialization()
{
    QWebSocketFrame frame;
    QVERIFY(!frame.isValid());
    QCOMPARE(frame.payload().length(), 0);
}

void tst_WebSocketFrame::tst_copyConstructorAndAssignment()
{
    FrameHelper frameHelper;
    frameHelper.setRsv1(0);
    frameHelper.setRsv2(0);
    frameHelper.setRsv3(0);
    frameHelper.setFinalFrame(true);
    frameHelper.setMask(1234u);
    frameHelper.setOpCode(QWebSocketProtocol::OpCodeBinary);
    frameHelper.setPayload(QByteArrayLiteral("12345"));

    QByteArray payload = frameHelper.wireRepresentation();
    QBuffer buffer(&payload);
    buffer.open(QIODevice::ReadOnly);

    QWebSocketFrame frame = QWebSocketFrame::readFrame(&buffer);
    buffer.close();

    {
        QWebSocketFrame other(frame);
        QCOMPARE(other.closeCode(), frame.closeCode());
        QCOMPARE(other.closeReason(), frame.closeReason());
        QCOMPARE(other.hasMask(), frame.hasMask());
        QCOMPARE(other.isContinuationFrame(), frame.isContinuationFrame());
        QCOMPARE(other.isControlFrame(), frame.isControlFrame());
        QCOMPARE(other.isDataFrame(), frame.isDataFrame());
        QCOMPARE(other.isFinalFrame(), frame.isFinalFrame());
        QCOMPARE(other.isValid(), frame.isValid());
        QCOMPARE(other.mask(), frame.mask());
        QCOMPARE(other.opCode(), frame.opCode());
        QCOMPARE(other.payload(), frame.payload());
        QCOMPARE(other.rsv1(), frame.rsv1());
        QCOMPARE(other.rsv2(), frame.rsv2());
        QCOMPARE(other.rsv3(), frame.rsv3());
    }
    {
        QWebSocketFrame other;
        other = frame;
        QCOMPARE(other.closeCode(), frame.closeCode());
        QCOMPARE(other.closeReason(), frame.closeReason());
        QCOMPARE(other.hasMask(), frame.hasMask());
        QCOMPARE(other.isContinuationFrame(), frame.isContinuationFrame());
        QCOMPARE(other.isControlFrame(), frame.isControlFrame());
        QCOMPARE(other.isDataFrame(), frame.isDataFrame());
        QCOMPARE(other.isFinalFrame(), frame.isFinalFrame());
        QCOMPARE(other.isValid(), frame.isValid());
        QCOMPARE(other.mask(), frame.mask());
        QCOMPARE(other.opCode(), frame.opCode());
        QCOMPARE(other.payload(), frame.payload());
        QCOMPARE(other.rsv1(), frame.rsv1());
        QCOMPARE(other.rsv2(), frame.rsv2());
        QCOMPARE(other.rsv3(), frame.rsv3());
    }
}

void tst_WebSocketFrame::tst_goodFrames_data()
{
    QTest::addColumn<bool>("rsv1");
    QTest::addColumn<bool>("rsv2");
    QTest::addColumn<bool>("rsv3");
    QTest::addColumn<quint32>("mask");
    QTest::addColumn<QWebSocketProtocol::OpCode>("opCode");
    QTest::addColumn<bool>("isFinal");
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<bool>("isControlFrame");
    QTest::addColumn<bool>("isDataFrame");
    QTest::addColumn<bool>("isContinuationFrame");

    QTest::newRow("Non masked final text frame with small payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QStringLiteral("Hello world!").toUtf8()
            << false << true << false;
    QTest::newRow("Non masked final binary frame with small payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodeBinary
            << true << QByteArrayLiteral("\x00\x01\x02\x03\x04")
            << false << true << false;
    QTest::newRow("Non masked final text frame with no payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QByteArray()
            << false << true << false;
    QTest::newRow("Non masked final binary frame with no payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodeBinary
            << true << QByteArray()
            << false << true << false;

    QTest::newRow("Non masked final close frame with small payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodeClose
            << true << QStringLiteral("Hello world!").toUtf8()
            << true << false << false;
    QTest::newRow("Non masked final close frame with no payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodeClose
            << true << QByteArray()
            << true << false << false;
    QTest::newRow("Non masked final ping frame with small payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodePing
            << true << QStringLiteral("Hello world!").toUtf8()
            << true << false << false;
    QTest::newRow("Non masked final pong frame with no payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodePong
            << true << QByteArray()
            << true << false << false;

    QTest::newRow("Non masked final continuation frame with small payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodeContinue
            << true << QStringLiteral("Hello world!").toUtf8()
            << false << true << true;
    QTest::newRow("Non masked non-final continuation frame with small payload")
            << false << false << false
            << 0U << QWebSocketProtocol::OpCodeContinue
            << false << QStringLiteral("Hello world!").toUtf8()
            << false << true << true;
}

void tst_WebSocketFrame::tst_goodFrames()
{
    QFETCH(bool, rsv1);
    QFETCH(bool, rsv2);
    QFETCH(bool, rsv3);
    QFETCH(quint32, mask);
    QFETCH(QWebSocketProtocol::OpCode, opCode);
    QFETCH(bool, isFinal);
    QFETCH(QByteArray, payload);
    QFETCH(bool, isControlFrame);
    QFETCH(bool, isDataFrame);
    QFETCH(bool, isContinuationFrame);

    FrameHelper helper;
    helper.setRsv1(rsv1);
    helper.setRsv2(rsv2);
    helper.setRsv3(rsv3);
    helper.setMask(mask);
    helper.setOpCode(opCode);
    helper.setFinalFrame(isFinal);
    helper.setPayload(payload);

    QByteArray wireRepresentation = helper.wireRepresentation();
    QBuffer buffer;
    buffer.setData(wireRepresentation);
    buffer.open(QIODevice::ReadOnly);
    QWebSocketFrame frame = QWebSocketFrame::readFrame(&buffer);
    buffer.close();
    QVERIFY(frame.isValid());
    QCOMPARE(frame.rsv1(), rsv1);
    QCOMPARE(frame.rsv2(), rsv2);
    QCOMPARE(frame.rsv3(), rsv3);
    QCOMPARE(frame.hasMask(), (mask != 0));
    QCOMPARE(frame.opCode(), opCode);
    QCOMPARE(frame.isFinalFrame(), isFinal);
    QCOMPARE(frame.isControlFrame(), isControlFrame);
    QCOMPARE(frame.isDataFrame(), isDataFrame);
    QCOMPARE(frame.isContinuationFrame(), isContinuationFrame);
    QCOMPARE(frame.payload().length(), payload.length());
    QCOMPARE(frame.payload(), payload);
}

void tst_WebSocketFrame::tst_invalidFrames_data()
{
    QTest::addColumn<int>("rsv1");
    QTest::addColumn<int>("rsv2");
    QTest::addColumn<int>("rsv3");
    QTest::addColumn<quint32>("mask");
    QTest::addColumn<QWebSocketProtocol::OpCode>("opCode");
    QTest::addColumn<bool>("isFinal");
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QWebSocketProtocol::CloseCode>("expectedError");

    QTest::newRow("RSV1 != 0")
            << 1 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("RSV2 != 0")
            << 0 << 1 << 0
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("RSV3 != 0")
            << 0 << 0 << 1
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("RSV1 != 0 and RSV2 != 0")
            << 1 << 1 << 0
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("RSV1 != 0 and RSV3 != 0")
            << 1 << 0 << 1
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("RSV2 != 0 and RSV3 != 0")
            << 0 << 1 << 1
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;

    QTest::newRow("Reserved OpCode 3")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReserved3
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode 4")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReserved4
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode 5")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReserved5
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode 6")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReserved6
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode 7")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReserved7
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode B")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReservedB
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode C")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReservedC
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode D")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReservedD
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode E")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReservedE
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Reserved OpCode F")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeReservedF
            << true << QStringLiteral("Hello world!").toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;

    QTest::newRow("Close Frame with payload > 125 bytes")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeClose
            << true << QString(126, 'a').toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Non-final Close Frame")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeClose
            << false << QString(126, 'a').toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Ping Frame with payload > 125 bytes")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodePing
            << true << QString(126, 'a').toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Non-final Ping Frame")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodePing
            << false << QString(126, 'a').toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Pong Frame with payload > 125 bytes")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodePong
            << true << QString(126, 'a').toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
    QTest::newRow("Non-final Pong Frame")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodePong
            << false << QString(126, 'a').toUtf8()
            << QWebSocketProtocol::CloseCodeProtocolError;
}

void tst_WebSocketFrame::tst_invalidFrames()
{
    QFETCH(int, rsv1);
    QFETCH(int, rsv2);
    QFETCH(int, rsv3);
    QFETCH(quint32, mask);
    QFETCH(QWebSocketProtocol::OpCode, opCode);
    QFETCH(bool, isFinal);
    QFETCH(QByteArray, payload);
    QFETCH(QWebSocketProtocol::CloseCode, expectedError);

    FrameHelper helper;
    helper.setRsv1(rsv1);
    helper.setRsv2(rsv2);
    helper.setRsv3(rsv3);
    helper.setMask(mask);
    helper.setOpCode(opCode);
    helper.setFinalFrame(isFinal);
    helper.setPayload(payload);

    QByteArray wireRepresentation = helper.wireRepresentation();
    QBuffer buffer;
    buffer.setData(wireRepresentation);
    buffer.open(QIODevice::ReadOnly);
    QWebSocketFrame frame = QWebSocketFrame::readFrame(&buffer);
    buffer.close();

    QVERIFY(!frame.isValid());
    QCOMPARE(frame.closeCode(), expectedError);
}


/*
 * Incomplete or overly large frames
 * Payload must be crafted manually
 *
    QTest::newRow("Frame Too Big")
            << 0 << 0 << 0
            << 0U << QWebSocketProtocol::OpCodeText
            << true << QString(MAX_FRAME_SIZE_IN_BYTES + 1, 'a').toUtf8()
            << QWebSocketProtocol::CloseCodeTooMuchData;

 */
void tst_WebSocketFrame::tst_malformedFrames_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<QWebSocketProtocol::CloseCode>("expectedError");

    //too little data
    QTest::newRow("No data") << QByteArray() << QWebSocketProtocol::CloseCodeGoingAway;
    FrameHelper helper;
    helper.setRsv1(0);
    helper.setRsv2(0);
    helper.setRsv3(0);
    helper.setMask(0U);
    helper.setOpCode(QWebSocketProtocol::OpCodeText);
    helper.setFinalFrame(true);
    helper.setPayload(QString(10, 'a').toUtf8());
    QByteArray wireRep = helper.wireRepresentation();

    //too little data
    //header + payload should be 12 bytes for non-masked payloads < 126 bytes
    for (int i = 1; i < 12; ++i)
    {
        QTest::newRow(QStringLiteral("Header too small - %1 byte(s)").arg(i).toLatin1().constData())
                << wireRep.left(i)
                << QWebSocketProtocol::CloseCodeGoingAway;
    }
    //too much data
    {
        const char bigpayloadIndicator = char(127);
        const quint64 payloadSize = MAX_FRAME_SIZE_IN_BYTES + 1;
        uchar swapped[8] = {0};
        qToBigEndian<quint64>(payloadSize, swapped);
        QTest::newRow("Frame too big")
                << wireRep.left(1).append(bigpayloadIndicator)
                                  .append(reinterpret_cast<char *>(swapped), 8)
                << QWebSocketProtocol::CloseCodeTooMuchData;
    }
    //invalid size field
    {
        const char bigpayloadIndicator = char(127);
        quint64 payloadSize = quint64(1) << 63;
        uchar swapped[8] = {0};
        qToBigEndian<quint64>(payloadSize, swapped);
        QTest::newRow("Highest bit of payload length is set")
                << wireRep.left(1).append(bigpayloadIndicator)
                                  .append(reinterpret_cast<char *>(swapped), 8)
                << QWebSocketProtocol::CloseCodeProtocolError;

        payloadSize = 256;
        qToBigEndian<quint64>(payloadSize, swapped);
        QTest::newRow("Overlong 64-bit size field; should be 16-bit")
                << wireRep.left(1).append(bigpayloadIndicator)
                                  .append(reinterpret_cast<char *>(swapped), 8)
                << QWebSocketProtocol::CloseCodeProtocolError;
    }
    //overlong size field
    {
        const char largepayloadIndicator = char(126);
        const quint16 payloadSize = 120;
        uchar swapped[2] = {0};
        qToBigEndian<quint16>(payloadSize, swapped);
        QTest::newRow("Overlong 16-bit size field")
                << wireRep.left(1).append(largepayloadIndicator)
                                  .append(reinterpret_cast<char *>(swapped), 2)
                << QWebSocketProtocol::CloseCodeProtocolError;
    }
    {
        const char bigpayloadIndicator = char(127);
        quint64 payloadSize = 120;
        uchar swapped[8] = {0};
        qToBigEndian<quint64>(payloadSize, swapped);
        QTest::newRow("Overlong 64-bit size field; should be 7-bit")
                << wireRep.left(1).append(bigpayloadIndicator)
                                  .append(reinterpret_cast<char *>(swapped), 8)
                << QWebSocketProtocol::CloseCodeProtocolError;

        payloadSize = 256;
        qToBigEndian<quint64>(payloadSize, swapped);
        QTest::newRow("Overlong 64-bit size field; should be 16-bit")
                << wireRep.left(1).append(bigpayloadIndicator)
                                  .append(reinterpret_cast<char *>(swapped), 8)
                << QWebSocketProtocol::CloseCodeProtocolError;
    }
}

void tst_WebSocketFrame::tst_malformedFrames()
{
    QFETCH(QByteArray, payload);
    QFETCH(QWebSocketProtocol::CloseCode, expectedError);

    QBuffer buffer;
    buffer.setData(payload);
    buffer.open(QIODevice::ReadOnly);
    QWebSocketFrame frame = QWebSocketFrame::readFrame(&buffer);
    buffer.close();

    QVERIFY(!frame.isValid());
    QCOMPARE(frame.closeCode(), expectedError);
}

QTEST_MAIN(tst_WebSocketFrame)

#include "tst_websocketframe.moc"

