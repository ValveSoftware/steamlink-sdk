/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include <QtCore/qdebug.h>
#include <QtSerialBus/qmodbuspdu.h>

#include <QtTest/QtTest>

static QString s_msg;
static void myMessageHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    s_msg = msg;
}

class DebugHandler
{
public:
    DebugHandler(QtMessageHandler newMessageHandler)
        : oldMessageHandler(qInstallMessageHandler(newMessageHandler)) {}
    ~DebugHandler() {
        qInstallMessageHandler(oldMessageHandler);
    }
private:
    QtMessageHandler oldMessageHandler;
};

static int minimumDataSize(const QModbusPdu &pdu, bool request)
{
    if (pdu.isException())
        return 1;

    switch (pdu.functionCode()) {
    case QModbusPdu::ReadCoils:
    case QModbusPdu::ReadDiscreteInputs:
        return request ? 4 : 2;
    case QModbusPdu::WriteSingleCoil:
    case QModbusPdu::WriteSingleRegister:
        return 4;
    case QModbusPdu::ReadHoldingRegisters:
    case QModbusPdu::ReadInputRegisters:
        return request ? 4 : 3;
    case QModbusPdu::ReadExceptionStatus:
        return request ? 0 : 1;
    case QModbusPdu::Diagnostics:
        return 4;
    case QModbusPdu::GetCommEventCounter:
        return request ? 0 : 4;
    case QModbusPdu::GetCommEventLog:
        return request ? 0 : 8;
    case QModbusPdu::WriteMultipleCoils:
        return request ? 6 : 4;
    case QModbusPdu::WriteMultipleRegisters:
        return request ? 7 : 4;
    case QModbusPdu::ReportServerId:
        return request ? 0 : 3;
    case QModbusPdu::ReadFileRecord:
        return request ? 8 : 5;
    case QModbusPdu::WriteFileRecord:
        return 10;
    case QModbusPdu::MaskWriteRegister:
        return 6;
    case QModbusPdu::ReadWriteMultipleRegisters:
        return request ? 11 : 3;
    case QModbusPdu::ReadFifoQueue:
        return request ? 2 : 6;
    case QModbusPdu::EncapsulatedInterfaceTransport:
        return 2;
    case QModbusPdu::Invalid:
    case QModbusPdu::UndefinedFunctionCode:
        return -1;
    }
    return -1;
}

class tst_QModbusPdu : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor()
    {
        QModbusRequest request;
        QCOMPARE(request.isValid(), false);
        QCOMPARE(request.isException(), false);
        QCOMPARE(request.functionCode(), QModbusRequest::FunctionCode(0x00));
        QCOMPARE(request.exceptionCode(), QModbusPdu::ExtendedException);
        QCOMPARE(request.data().toHex(), QByteArray());

        QModbusResponse response;
        QCOMPARE(response.isValid(), false);
        QCOMPARE(response.isException(), false);
        QCOMPARE(response.functionCode(), QModbusResponse::FunctionCode(0x00));
        QCOMPARE(response.exceptionCode(), QModbusPdu::ExtendedException);
        QCOMPARE(response.data().toHex(), QByteArray());

        QModbusExceptionResponse exception;
        QCOMPARE(exception.isValid(), false);
        QCOMPARE(exception.isException(), false);
        QCOMPARE(exception.exceptionCode(), QModbusPdu::ExtendedException);
        QCOMPARE(exception.functionCode(), QModbusExceptionResponse::FunctionCode(0x00));
        QCOMPARE(exception.data().toHex(), QByteArray());
    }

    void testVaridicConstructor()
    {
        QModbusRequest request(QModbusRequest::ReportServerId);
        QCOMPARE(request.isValid(), true);
        QCOMPARE(request.isException(), false);
        QCOMPARE(request.functionCode(), QModbusRequest::ReportServerId);
        QCOMPARE(request.exceptionCode(), QModbusPdu::ExtendedException);
        QCOMPARE(request.data().toHex(), QByteArray());

        // Byte Count, Server ID, Run Indicator Status
        QModbusResponse response(QModbusResponse::ReportServerId, quint8(0x02), quint8(0x01),
            quint8(0xff));
        QCOMPARE(response.isValid(), true);
        QCOMPARE(response.isException(), false);
        QCOMPARE(response.functionCode(), QModbusResponse::ReportServerId);
        QCOMPARE(response.exceptionCode(), QModbusPdu::ExtendedException);
        QCOMPARE(response.data().toHex(), QByteArray("0201ff"));
    }

    void testQByteArrayConstructor()
    {
        QModbusRequest request(QModbusRequest::ReportServerId, QByteArray());
        QCOMPARE(request.isValid(), true);
        QCOMPARE(request.isException(), false);
        QCOMPARE(request.functionCode(), QModbusRequest::ReportServerId);
        QCOMPARE(request.data().toHex(), QByteArray());

        // Byte Count, Server ID, Run Indicator Status
        QModbusResponse response(QModbusResponse::ReportServerId, QByteArray::fromHex("ff0102"));
        QCOMPARE(response.isValid(), true);
        QCOMPARE(response.isException(), false);
        QCOMPARE(response.functionCode(), QModbusResponse::ReportServerId);
        QCOMPARE(response.data().toHex(), QByteArray("ff0102"));
    }

    void testCopyConstructorAndAssignment()
    {
        QModbusPdu pdu;
        pdu.setFunctionCode(QModbusPdu::ReadCoils);

        QModbusRequest request(pdu);
        QCOMPARE(request.functionCode(), QModbusResponse::ReadCoils);

        QModbusResponse response(pdu);
        QCOMPARE(response.functionCode(), QModbusResponse::ReadCoils);

        pdu.setFunctionCode(QModbusPdu::WriteMultipleCoils);

        request = pdu;
        QCOMPARE(request.functionCode(), QModbusResponse::WriteMultipleCoils);

        response = pdu;
        QCOMPARE(response.functionCode(), QModbusResponse::WriteMultipleCoils);
    }

    void testEncodeDecode()
    {
        QModbusRequest invalid;
        quint8 data = 0xff;
        invalid.decodeData(&data);
        QCOMPARE(data, quint8(0xff));
        invalid.encodeData(data);
        QCOMPARE(invalid.data().toHex(), QByteArray("ff"));

        data = 0x00;
        QModbusRequest request(QModbusRequest::ReportServerId);
        request.decodeData(&data);
        QCOMPARE(data, quint8(0x00));
        QCOMPARE(request.data().toHex(), QByteArray());

        QModbusResponse response(QModbusResponse::ReportServerId);
        response.encodeData(quint8(0x02), quint8(0x01), quint8(0xff));
        quint8 count = 0, id = 0, run = 0;
        response.decodeData(&count, &id, &run);
        QCOMPARE(count, quint8(0x02));
        QCOMPARE(id, quint8(0x01));
        QCOMPARE(run, quint8(0xff));
        QCOMPARE(response.data().toHex(), QByteArray("0201ff"));

        count = 0, id = 0, run = 0;
        response = QModbusResponse(QModbusResponse::ReportServerId, QByteArray::fromHex("0201ff"));
        QCOMPARE(response.isValid(), true);
        QCOMPARE(response.isException(), false);
        QCOMPARE(response.functionCode(), QModbusResponse::ReportServerId);
        QCOMPARE(response.data().toHex(), QByteArray("0201ff"));
        response.decodeData(&count, &id, &run);
        QCOMPARE(count, quint8(0x02));
        QCOMPARE(id, quint8(0x01));
        QCOMPARE(run, quint8(0xff));

        QModbusRequest readCoils(QModbusRequest::ReadCoils);
        // starting address and quantity of coils
        readCoils.encodeData(quint16(12), quint16(10));
        quint16 address, quantity;
        readCoils.decodeData(&address, &quantity);
        QCOMPARE(address, quint16(0x0c));
        QCOMPARE(quantity, quint16(0x0a));

        request = QModbusRequest(QModbusRequest::WriteMultipleCoils,
            QByteArray::fromHex("0013000a02cd01"));
        QCOMPARE(request.data().toHex(), QByteArray("0013000a02cd01"));
        address = 0xffff, quantity = 0xffff;
        quint8 bytes = 0xff, firstByte = 0xff, secondByte = 0xff;
        request.decodeData(&address, &quantity, &bytes, &firstByte, &secondByte);
        QCOMPARE(address, quint16(19));
        QCOMPARE(quantity, quint16(10));
        QCOMPARE(bytes, quint8(2));
        QCOMPARE(firstByte, quint8(0xcd));
        QCOMPARE(secondByte, quint8(0x01));
    }

    void testQModbusExceptionResponsePdu()
    {
        QModbusExceptionResponse exception(QModbusExceptionResponse::ReportServerId,
            QModbusExceptionResponse::ServerDeviceFailure);
        QCOMPARE(exception.isValid(), true);
        QCOMPARE(exception.isException(), true);
        QCOMPARE(exception.functionCode(), QModbusExceptionResponse::ReportServerId);
        QCOMPARE(exception.data().toHex(), QByteArray("04"));
        QCOMPARE(exception.exceptionCode(), QModbusExceptionResponse::ServerDeviceFailure);

        QModbusExceptionResponse response;
        QCOMPARE(response.isValid(), false);
        QCOMPARE(response.isException(), false);
        QCOMPARE(response.functionCode(), QModbusExceptionResponse::Invalid);

        response.setFunctionCode(QModbusExceptionResponse::ReportServerId);
        response.setExceptionCode(QModbusExceptionResponse::ServerDeviceFailure);
        QCOMPARE(response.isValid(), true);
        QCOMPARE(response.isException(), true);
        QCOMPARE(response.functionCode(), QModbusExceptionResponse::ReportServerId);
        QCOMPARE(response.data().toHex(), QByteArray("04"));
        QCOMPARE(exception.exceptionCode(), QModbusExceptionResponse::ServerDeviceFailure);

        QModbusPdu pdu;
        pdu.setFunctionCode(QModbusExceptionResponse::FunctionCode(QModbusPdu::ReadCoils
            | QModbusPdu::ExceptionByte));
        QCOMPARE(pdu.isException(), true);
        QCOMPARE(pdu.functionCode(), QModbusPdu::ReadCoils);

        QModbusExceptionResponse exception2(pdu);
        QCOMPARE(exception2.isException(), true);
        QCOMPARE(exception2.functionCode(), QModbusPdu::ReadCoils);
        QCOMPARE(exception2.exceptionCode(), QModbusExceptionResponse::ExtendedException);
        exception2.setExceptionCode(QModbusExceptionResponse::IllegalFunction);
        QCOMPARE(exception2.exceptionCode(), QModbusExceptionResponse::IllegalFunction);

        QModbusExceptionResponse exception3 = pdu;
        QCOMPARE(exception3.isException(), true);
        QCOMPARE(exception3.functionCode(), QModbusPdu::ReadCoils);
        QCOMPARE(exception3.exceptionCode(), QModbusExceptionResponse::ExtendedException);
    }

    void testQDebugStreamOperator()
    {
        DebugHandler mhs(myMessageHandler);
        {
            QDebug d = qDebug();
            d << QModbusPdu();
            d << QModbusRequest();
            d << QModbusResponse();
            d << QModbusExceptionResponse();
        }
        QCOMPARE(s_msg, QString::fromLatin1("0x00 0x00 0x00 0x00"));

        {
            QDebug d = qDebug();
            d << QModbusRequest(QModbusRequest::ReadCoils, quint16(19), quint16(19));
        }
        QCOMPARE(s_msg, QString::fromLatin1("0x0100130013"));

        {
            QDebug d = qDebug();
            d << QModbusResponse(QModbusResponse::ReadCoils, QByteArray::fromHex("03cd6b05"));
        }
        QCOMPARE(s_msg, QString::fromLatin1("0x0103cd6b05"));

        {
            QDebug d = qDebug();
            d << QModbusExceptionResponse(QModbusExceptionResponse::ReadCoils,
                QModbusExceptionResponse::IllegalDataAddress);
        }
        QCOMPARE(s_msg, QString::fromLatin1("0x8102"));
    }

    void testMinimumDataSize()
    {
        bool request = true;
       QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadCoils), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReadCoils)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadDiscreteInputs), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReadDiscreteInputs)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteSingleCoil), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::WriteSingleCoil)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadHoldingRegisters), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReadHoldingRegisters)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadInputRegisters), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReadInputRegisters)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteSingleRegister), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::WriteSingleRegister)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadExceptionStatus), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReadExceptionStatus)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::Diagnostics), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::Diagnostics)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::GetCommEventCounter), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::GetCommEventCounter)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::GetCommEventLog), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::GetCommEventLog)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteMultipleCoils), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::WriteMultipleCoils)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteMultipleRegisters), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::WriteMultipleRegisters)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReportServerId), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReportServerId)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadFileRecord), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReadFileRecord)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteFileRecord), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::WriteFileRecord)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::MaskWriteRegister), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::MaskWriteRegister)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadWriteMultipleRegisters), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReadWriteMultipleRegisters)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadFifoQueue), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::ReadFifoQueue)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::EncapsulatedInterfaceTransport), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::EncapsulatedInterfaceTransport)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::Invalid), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::Invalid)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::UndefinedFunctionCode), request),
            QModbusRequest::minimumDataSize(QModbusRequest(QModbusRequest::Invalid)));

        request = false;
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadCoils), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReadCoils)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadDiscreteInputs), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReadDiscreteInputs)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteSingleCoil), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::WriteSingleCoil)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadHoldingRegisters), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReadHoldingRegisters)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadInputRegisters), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReadInputRegisters)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteSingleRegister), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::WriteSingleRegister)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadExceptionStatus), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReadExceptionStatus)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::Diagnostics), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::Diagnostics)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::GetCommEventCounter), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::GetCommEventCounter)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::GetCommEventLog), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::GetCommEventLog)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteMultipleCoils), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::WriteMultipleCoils)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteMultipleRegisters), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::WriteMultipleRegisters)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReportServerId), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReportServerId)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadFileRecord), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReadFileRecord)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::WriteFileRecord), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::WriteFileRecord)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::MaskWriteRegister), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::MaskWriteRegister)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadWriteMultipleRegisters), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReadWriteMultipleRegisters)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::ReadFifoQueue), request),
            QModbusResponse::minimumDataSize(QModbusResponse(QModbusResponse::ReadFifoQueue)));
        QCOMPARE(minimumDataSize(QModbusRequest(QModbusPdu::EncapsulatedInterfaceTransport), request),
            QModbusResponse::minimumDataSize(QModbusRequest(QModbusPdu::EncapsulatedInterfaceTransport)));
        QCOMPARE(minimumDataSize(QModbusResponse(QModbusPdu::Invalid), request),
            QModbusRequest::minimumDataSize(QModbusResponse(QModbusPdu::Invalid)));
        QCOMPARE(minimumDataSize(QModbusResponse(QModbusPdu::UndefinedFunctionCode), request),
            QModbusRequest::minimumDataSize(QModbusResponse(QModbusPdu::Invalid)));

        QModbusExceptionResponse exception(QModbusResponse::ReadCoils, QModbusPdu::IllegalFunction);
        QCOMPARE(minimumDataSize(exception, request), 1);
        QCOMPARE(QModbusResponse::minimumDataSize(exception), 1);
    }

    void testQModbusRequestStreamOperator_data()
    {
        QTest::addColumn<QModbusPdu::FunctionCode>("fc");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QByteArray>("pdu");

        QTest::newRow("ReadCoils") << QModbusRequest::ReadCoils << QByteArray::fromHex("00130013")
            << QByteArray::fromHex("0100130013");
        QTest::newRow("ReadDiscreteInputs") << QModbusRequest::ReadDiscreteInputs
            << QByteArray::fromHex("00c40016") << QByteArray::fromHex("0200c40016");
        QTest::newRow("ReadHoldingRegisters") << QModbusRequest::ReadHoldingRegisters
            << QByteArray::fromHex("006b0003") << QByteArray::fromHex("03006b0003");
        QTest::newRow("ReadInputRegisters") << QModbusRequest::ReadInputRegisters
            << QByteArray::fromHex("00080001") << QByteArray::fromHex("0400080001");
        QTest::newRow("WriteSingleCoil") << QModbusRequest::WriteSingleCoil
            << QByteArray::fromHex("00acff00") << QByteArray::fromHex("0500acff00");
        QTest::newRow("WriteSingleRegister") << QModbusRequest::WriteSingleRegister
            << QByteArray::fromHex("00010003") << QByteArray::fromHex("0600010003");
        QTest::newRow("ReadExceptionStatus") << QModbusRequest::ReadExceptionStatus
            << QByteArray() << QByteArray::fromHex("07");
        QTest::newRow("Diagnostics") << QModbusRequest::Diagnostics
            << QByteArray::fromHex("0000a537") << QByteArray::fromHex("080000a537");
        QTest::newRow("GetCommEventCounter") << QModbusRequest::GetCommEventCounter
            << QByteArray() << QByteArray::fromHex("0b");
        QTest::newRow("GetCommEventLog") << QModbusRequest::GetCommEventLog
            << QByteArray() << QByteArray::fromHex("0c");
        QTest::newRow("WriteMultipleCoils") << QModbusRequest::WriteMultipleCoils
            << QByteArray::fromHex("0013000a02cd01") << QByteArray::fromHex("0f0013000a02cd01");
        QTest::newRow("WriteMultipleRegisters") << QModbusRequest::WriteMultipleRegisters
            << QByteArray::fromHex("0001000204000a0102") << QByteArray::fromHex("100001000204000a0102");
        QTest::newRow("ReportServerId") << QModbusRequest::ReportServerId
            << QByteArray() << QByteArray::fromHex("11");
        QTest::newRow("ReadFileRecord") << QModbusRequest::ReadFileRecord
            << QByteArray::fromHex("0e0600040001000206000300090002")
            << QByteArray::fromHex("140e0600040001000206000300090002");
        QTest::newRow("WriteFileRecord") << QModbusRequest::WriteFileRecord
            << QByteArray::fromHex("0d0600040007000306af04be100d")
            << QByteArray::fromHex("150d0600040007000306af04be100d");
        QTest::newRow("MaskWriteRegister") << QModbusRequest::MaskWriteRegister
            << QByteArray::fromHex("000400f20025") << QByteArray::fromHex("16000400f20025");
        QTest::newRow("ReadWriteMultipleRegisters") << QModbusRequest::ReadWriteMultipleRegisters
            << QByteArray::fromHex("00030006000e00030600ff00ff00ff")
            << QByteArray::fromHex("1700030006000e00030600ff00ff00ff");
        QTest::newRow("ReadFifoQueue") << QModbusRequest::ReadFifoQueue
            << QByteArray::fromHex("04de") << QByteArray::fromHex("1804de");
        QTest::newRow("EncapsulatedInterfaceTransport")
            << QModbusRequest::EncapsulatedInterfaceTransport
            << QByteArray::fromHex("0e0100") << QByteArray::fromHex("2b0e0100");
    }

    void testQModbusRequestStreamOperator()
    {
        QFETCH(QModbusPdu::FunctionCode, fc);
        QFETCH(QByteArray, data);
        QFETCH(QByteArray, pdu);

        QByteArray ba;
        QDataStream output(&ba, QIODevice::WriteOnly);
        output << QModbusRequest(fc, data);
        QCOMPARE(ba, pdu);

        QModbusRequest request;
        QDataStream input(pdu);
        input >> request;
        QCOMPARE(request.functionCode(), fc);
        QCOMPARE(request.data(), data);
    }

    void testQModbusResponseStreamOperator_data()
    {
        QTest::addColumn<QModbusPdu::FunctionCode>("fc");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QByteArray>("pdu");

        QTest::newRow("ReadCoils") << QModbusResponse::ReadCoils << QByteArray::fromHex("03cd6b05")
            << QByteArray::fromHex("0103cd6b05");
        QTest::newRow("ReadDiscreteInputs") << QModbusResponse::ReadDiscreteInputs
            << QByteArray::fromHex("03acdb35") << QByteArray::fromHex("0203acdb35");
        QTest::newRow("ReadHoldingRegisters") << QModbusResponse::ReadHoldingRegisters
            << QByteArray::fromHex("06022b00000064") << QByteArray::fromHex("0306022b00000064");
        QTest::newRow("ReadInputRegisters") << QModbusResponse::ReadInputRegisters
            << QByteArray::fromHex("02000a") << QByteArray::fromHex("0402000a");
        QTest::newRow("WriteSingleCoil") << QModbusResponse::WriteSingleCoil
            << QByteArray::fromHex("00acff00") << QByteArray::fromHex("0500acff00");
        QTest::newRow("WriteSingleRegister") << QModbusResponse::WriteSingleRegister
            << QByteArray::fromHex("00010003") << QByteArray::fromHex("0600010003");
        QTest::newRow("ReadExceptionStatus") << QModbusResponse::ReadExceptionStatus
            << QByteArray::fromHex("6d") << QByteArray::fromHex("076d");
        QTest::newRow("Diagnostics") << QModbusResponse::Diagnostics
            << QByteArray::fromHex("ffff0108") << QByteArray::fromHex("08ffff0108");
        QTest::newRow("GetCommEventCounter") << QModbusResponse::GetCommEventCounter
            << QByteArray::fromHex("ffff0108") << QByteArray::fromHex("0bffff0108");
        QTest::newRow("GetCommEventLog") << QModbusResponse::GetCommEventLog
            << QByteArray::fromHex("080000010801212000") << QByteArray::fromHex("0c080000010801212000");
        QTest::newRow("WriteMultipleCoils") << QModbusResponse::WriteMultipleCoils
            << QByteArray::fromHex("0013000a") << QByteArray::fromHex("0f0013000a");
        QTest::newRow("WriteMultipleRegisters") << QModbusResponse::WriteMultipleRegisters
            << QByteArray::fromHex("00010002") << QByteArray::fromHex("1000010002");
        QTest::newRow("ReportServerId") << QModbusResponse::ReportServerId
            << QByteArray::fromHex("030aff12") << QByteArray::fromHex("11030aff12");
        QTest::newRow("ReadFileRecord") << QModbusResponse::ReadFileRecord
            << QByteArray::fromHex("0c05060dfe0020050633cd0040")
            << QByteArray::fromHex("140c05060dfe0020050633cd0040");
        QTest::newRow("WriteFileRecord") << QModbusResponse::WriteFileRecord
            << QByteArray::fromHex("0d0600040007000306af04be100d")
            << QByteArray::fromHex("150d0600040007000306af04be100d");
        QTest::newRow("MaskWriteRegister") << QModbusResponse::MaskWriteRegister
            << QByteArray::fromHex("000400f20025") << QByteArray::fromHex("16000400f20025");
        QTest::newRow("ReadWriteMultipleRegisters") << QModbusResponse::ReadWriteMultipleRegisters
            << QByteArray::fromHex("0c00fe0acd00010003000d00ff")
            << QByteArray::fromHex("170c00fe0acd00010003000d00ff");
        QTest::newRow("ReadFifoQueue") << QModbusResponse::ReadFifoQueue
            << QByteArray::fromHex("0006000201b81284") << QByteArray::fromHex("180006000201b81284");
        QTest::newRow("EncapsulatedInterfaceTransport")
            << QModbusResponse::EncapsulatedInterfaceTransport
            << QByteArray::fromHex("0e01010000030016") + "Company identification" +
                QByteArray::fromHex("010c") + "Product code" + QByteArray::fromHex("0205") +
                "V2.11"
            << QByteArray::fromHex("2b0e01010000030016") + "Company identification" +
                QByteArray::fromHex("010c") + "Product code" + QByteArray::fromHex("0205") +
                "V2.11";

        QModbusExceptionResponse ex(QModbusPdu::ReadCoils, QModbusPdu::IllegalDataAddress);
        QTest::newRow("StreamExceptionResponse")
            << QModbusPdu::FunctionCode(ex.functionCode() | QModbusPdu::ExceptionByte)
            << QByteArray::fromHex("02") << QByteArray::fromHex("8102");
    }

    void testQModbusResponseStreamOperator()
    {
        QFETCH(QModbusPdu::FunctionCode, fc);
        QFETCH(QByteArray, data);
        QFETCH(QByteArray, pdu);

        QByteArray ba;
        QDataStream output(&ba, QIODevice::WriteOnly);
        output << QModbusResponse(fc, data);
        QCOMPARE(ba, pdu);

        QModbusResponse response;
        QDataStream input(pdu);
        input >> response;
        if (response.isException())
            QCOMPARE(quint8(response.functionCode()), quint8(fc &~ QModbusPdu::ExceptionByte));
        else
            QCOMPARE(response.functionCode(), fc);
        QCOMPARE(response.data(), data);
    }

    void testEncapsulatedInterfaceTransportStreamOperator()
    {
        const QByteArray valid = QByteArray::fromHex("0e01010000030016") + "Company identification"
            + QByteArray::fromHex("010c") + "Product code" + QByteArray::fromHex("0205")
            + "V2.11";
        {
            QByteArray ba;
            QDataStream output(&ba, QIODevice::ReadWrite);
            output << QModbusResponse(QModbusResponse::EncapsulatedInterfaceTransport, valid);

            QModbusResponse response;
            QDataStream input(ba);
            input >> response;
            QCOMPARE(response.functionCode(), QModbusResponse::EncapsulatedInterfaceTransport);
            QCOMPARE(response.data(), valid);
        }

        // valid embedded
        const QByteArray suffix = QByteArray::fromHex("ffffffffff");
        {
            QByteArray ba;
            QDataStream output(&ba, QIODevice::ReadWrite);
            output << QModbusResponse(QModbusResponse::EncapsulatedInterfaceTransport, valid
                + suffix);

            QModbusResponse response;
            QDataStream input(ba);
            input >> response;
            QCOMPARE(response.functionCode(), QModbusResponse::EncapsulatedInterfaceTransport);
            QCOMPARE(response.data(), valid);
        }

        const QByteArray invalid = QByteArray::fromHex("0e01010000030016") + "Company identification"
            + QByteArray::fromHex("01ff") + "Product code" + QByteArray::fromHex("0205")
            + "V2.11";
        {
            QByteArray ba;
            QDataStream output(&ba, QIODevice::ReadWrite);
            output << QModbusResponse(QModbusResponse::EncapsulatedInterfaceTransport, invalid);

            QModbusResponse response;
            QDataStream input(ba);
            input >> response;
            QCOMPARE(response.functionCode(), QModbusResponse::Invalid);
            QCOMPARE(response.data(), QByteArray());
        }

        const QByteArray invalidShort = QByteArray::fromHex("0e01010000030016")
            + "Company identification" + QByteArray::fromHex("01");
        {
            QByteArray ba;
            QDataStream output(&ba, QIODevice::ReadWrite);
            output << QModbusResponse(QModbusResponse::EncapsulatedInterfaceTransport, invalidShort);

            QModbusResponse response;
            QDataStream input(ba);
            input >> response;
            QCOMPARE(response.functionCode(), QModbusResponse::Invalid);
            QCOMPARE(response.data(), QByteArray());
        }
    }

    void testCalculateDataSize()
    {
        // CanOpenGeneralReference
        QModbusResponse cogr(QModbusPdu::EncapsulatedInterfaceTransport, quint8(0x0d));
        QCOMPARE(QModbusResponse::calculateDataSize(cogr), -1);
        cogr.encodeData(quint8(0x0d), quint8(0x00));
        QCOMPARE(QModbusResponse::calculateDataSize(cogr), 2);
        // TODO: fix the following once we can calculate the size for CanOpenGeneralReference
        cogr.encodeData(quint8(0x0d), quint8(0x00), quint8(0x00));
        QCOMPARE(QModbusResponse::calculateDataSize(cogr), 2);

        // ReadDeviceIdentification
        QModbusResponse rdi(QModbusPdu::EncapsulatedInterfaceTransport, quint8(0x0e));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), -1);
        rdi.encodeData(quint8(0x0e), quint8(0x01));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);

        // single object
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x01)); // object count
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x01),
            quint8(0x00)); // object id
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x01),
            quint8(0x00), quint8(0x01)); // object size
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 9);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x01),
            quint8(0x00), quint8(0x01), quint8(0xff)); // object data
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 9);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x01),
            quint8(0x00), quint8(0x01), quint8(0xff),
            quint8(0xff)); // dummy additional data
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 9);

        // multiple objects equal size
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0xff), quint8(0x02),
            quint8(0x02)); // object count
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0xff), quint8(0x02),
            quint8(0x02),
            quint8(0x00)); // object id
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x02),
            quint8(0x00), quint8(0x01)); // object size
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 11);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x02),
            quint8(0x00), quint8(0x01), quint8(0xff)); // object data
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 11);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x02),
            quint8(0x00), quint8(0x01), quint8(0xff),
            quint8(0x01)); // second object id
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 11);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x02),
            quint8(0x00), quint8(0x01), quint8(0xff),
            quint8(0x01), quint8(0x01)); // second object size
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 12);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x02),
            quint8(0x00), quint8(0x01), quint8(0xff),
            quint8(0x01), quint8(0x01), quint8(0xff)); // second object data
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 12);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x02),
            quint8(0x00), quint8(0x01), quint8(0xff),
            quint8(0x01), quint8(0x01), quint8(0xff),
            quint8(0xff)); // dummy additional data
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 12);

        // multiple objects different size
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0xff), quint8(0x02),
            quint8(0x03));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0xff), quint8(0x02),
            quint8(0x03),
            quint8(0x00));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 8);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x01));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 13);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 14);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 14);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 14);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 14);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 14);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 17);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 17);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0x02));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 17);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0x02), quint8(0x05));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 22);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0x02), quint8(0x05), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 22);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0x02), quint8(0x05), quint8(0xff), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 22);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0x02), quint8(0x05), quint8(0xff), quint8(0xff), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 22);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0x02), quint8(0x05), quint8(0xff), quint8(0xff), quint8(0xff), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 22);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0x02), quint8(0x05), quint8(0xff), quint8(0xff), quint8(0xff), quint8(0xff), quint8(0xff));
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 22);
        rdi.encodeData(quint8(0x0e), quint8(0x01), quint8(0x01), quint8(0x00), quint8(0x00),
            quint8(0x03),
            quint8(0x00), quint8(0x02), quint8(0xff), quint8(0xff),
            quint8(0x01), quint8(0x03), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0x02), quint8(0x05), quint8(0xff), quint8(0xff), quint8(0xff), quint8(0xff), quint8(0xff),
            quint8(0xff)); // dummy additional data
        QCOMPARE(QModbusResponse::calculateDataSize(rdi), 22);
    }

    void testCustomCalculator()
    {
        // request
        QModbusRequest request(QModbusRequest::WriteSingleCoil);
        request.encodeData(quint8(0x00), quint8(0xac), quint8(0xff), quint8(0x00));
        QCOMPARE(QModbusRequest::calculateDataSize(request), 4);

        QModbusRequest::registerDataSizeCalculator(QModbusRequest::WriteSingleCoil,
            [](const QModbusRequest &)->int {
            return 15;
        });
        QCOMPARE(QModbusRequest::calculateDataSize(request), 15);

        QModbusRequest::registerDataSizeCalculator(QModbusRequest::WriteSingleCoil, nullptr);
        QCOMPARE(QModbusRequest::calculateDataSize(request), 4);

        // response
        QModbusResponse response(QModbusRequest::WriteSingleCoil);
        response.encodeData(quint8(0x00), quint8(0xac), quint8(0xff), quint8(0x00));
        QCOMPARE(QModbusResponse::calculateDataSize(request), 4);

        QModbusResponse::registerDataSizeCalculator(QModbusResponse::WriteSingleCoil,
            [](const QModbusResponse &)->int {
            return 15;
        });
        QCOMPARE(QModbusResponse::calculateDataSize(response), 15);

        QModbusResponse::registerDataSizeCalculator(QModbusResponse::WriteSingleCoil, nullptr);
        QCOMPARE(QModbusResponse::calculateDataSize(response), 4);

        // custom
        QModbusPdu::FunctionCode custom = QModbusPdu::FunctionCode(0x30);
        request.setFunctionCode(custom);
        QCOMPARE(QModbusRequest::calculateDataSize(request), -1);

        QModbusRequest::registerDataSizeCalculator(custom, [](const QModbusRequest &)->int {
            return 27;
        });
        QCOMPARE(QModbusRequest::calculateDataSize(request), 27);
    }

    void testCalculateLongDataSize()
    {
        QByteArray longData = QByteArray(128, ' ');
        longData[0] = longData.size();
        const QModbusResponse response(QModbusResponse::ReadCoils, longData);
        QCOMPARE(QModbusResponse::calculateDataSize(response), 1 + longData.size());

        const QModbusRequest wfrRequest(QModbusPdu::WriteFileRecord, longData);
        QCOMPARE(QModbusRequest::calculateDataSize(wfrRequest), 1 + longData.size());

        longData = QByteArray(4 + 128, ' ');
        longData[4] = char(128);
        const QModbusRequest wmcRequest(QModbusPdu::WriteMultipleCoils, longData);
        QCOMPARE(QModbusRequest::calculateDataSize(wmcRequest), 1 + longData.size());

        const QModbusRequest wmrRequest(QModbusPdu::WriteMultipleRegisters, longData);
        QCOMPARE(QModbusRequest::calculateDataSize(wmrRequest), 1 + longData.size());
    }
};

QTEST_MAIN(tst_QModbusPdu)

#include "tst_qmodbuspdu.moc"
