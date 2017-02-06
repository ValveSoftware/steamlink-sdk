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

#include <QtSerialBus/qmodbusclient.h>
#include <private/qmodbusclient_p.h>
#include <private/qmodbus_symbols_p.h>

#include <QtTest/QtTest>

class TestClient : public QModbusClient
{
    Q_OBJECT
    class TestClientPrivate : public QModbusClientPrivate
    {
        Q_DECLARE_PUBLIC(TestClient)

    public:
        bool isOpen() const override { return m_open; }

    private:
        bool m_open = false;
    };

public:
    TestClient()
        : QModbusClient(*new TestClientPrivate)
    {}
    bool open() override {
        d_func()->m_open = true;
        setState(QModbusDevice::ConnectedState);
        return true;
    }
    void close() override {
        d_func()->m_open = true;
        setState(QModbusDevice::UnconnectedState);
    }
    bool processResponse(const QModbusResponse &response, QModbusDataUnit *data)
    {
        return QModbusClient::processResponse(response, data);
    }
    Q_DECLARE_PRIVATE(TestClient)
};

class tst_QModbusClient : public QObject
{
    Q_OBJECT

private slots:
    void testTimeout()
    {
        TestClient client;
        QCOMPARE(client.timeout(), 1000); //default value test

        QSignalSpy spy(&client, SIGNAL(timeoutChanged(int)));
        client.setTimeout(50);
        QCOMPARE(client.timeout(), 50);
        QCOMPARE(spy.isEmpty(), false); // we expect the signal

        spy.clear();
        client.setTimeout(49); // everything below 50 fails
        QVERIFY(client.timeout() >= 50);
        QCOMPARE(spy.isEmpty(), true); // and the signal should not fire
    }

    void testNumberOfRetries()
    {
        TestClient client;
        QCOMPARE(client.numberOfRetries(), 3);

        client.setNumberOfRetries(-1);  // ignore everything below 0
        QCOMPARE(client.numberOfRetries(), 3);

        client.setNumberOfRetries(1);
        QCOMPARE(client.numberOfRetries(), 1);
    }

    void testProcessReadWriteSingleMultipleCoilsResponse()
    {
        TestClient client;

        QModbusDataUnit unit(QModbusDataUnit::Coils, 100, 24);
        QModbusResponse response = QModbusResponse(QModbusResponse::ReadCoils,
            QByteArray::fromHex("03cd6b05"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 24u);
        QCOMPARE(unit.startAddress(), 100);
        QCOMPARE(unit.values(),
            QVector<quint16>({ 1,0,1,1,0,0,1,1, 1,1,0,1,0,1,1,0, 1,0,1,0,0,0,0,0 }));
        QCOMPARE(unit.registerType(), QModbusDataUnit::Coils);

        unit = QModbusDataUnit();
        response = QModbusResponse(QModbusResponse::WriteSingleCoil,
            QByteArray::fromHex("00acff00"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 1u);
        QCOMPARE(unit.startAddress(), 172);
        QCOMPARE(unit.values(), QVector<quint16>() << Coil::On);
        QCOMPARE(unit.registerType(), QModbusDataUnit::Coils);

        unit = QModbusDataUnit();
        response = QModbusResponse(QModbusResponse::WriteMultipleCoils,
            QByteArray::fromHex("0013000a"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 10u);
        QCOMPARE(unit.startAddress(), 19);
        QCOMPARE(unit.values(), QVector<quint16>());
        QCOMPARE(unit.registerType(), QModbusDataUnit::Coils);
    }

    void testProcessReadDiscreteInputsResponse()
    {
        TestClient client;

        QModbusDataUnit unit(QModbusDataUnit::DiscreteInputs, 100, 24);
        QModbusResponse response = QModbusResponse(QModbusResponse::ReadDiscreteInputs,
            QByteArray::fromHex("03cd6b05"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 24u);
        QCOMPARE(unit.startAddress(), 100);
        QCOMPARE(unit.values(),
            QVector<quint16>({ 1,0,1,1,0,0,1,1, 1,1,0,1,0,1,1,0, 1,0,1,0,0,0,0,0 }));
        QCOMPARE(unit.registerType(), QModbusDataUnit::DiscreteInputs);

        response.setFunctionCode(QModbusPdu::FunctionCode(0x82));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setFunctionCode(QModbusResponse::ReadDiscreteInputs);
        response.setData(QByteArray::fromHex("05"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("03cd6b"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("03cd6b0517"));
        QCOMPARE(client.processResponse(response, &unit), false);
    }

    void testProcessReadHoldingRegistersResponse()
    {
        TestClient client;

        QModbusDataUnit unit;
        unit.setStartAddress(100);
        QModbusResponse response = QModbusResponse(QModbusResponse::ReadHoldingRegisters,
                                                   QByteArray::fromHex("04cd6b057f"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 2u);
        QCOMPARE(unit.startAddress(), 100);
        QCOMPARE(unit.values(), QVector<quint16>({ 52587, 1407 }));
        QCOMPARE(unit.registerType(), QModbusDataUnit::HoldingRegisters);

        response.setFunctionCode(QModbusPdu::FunctionCode(0x83));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setFunctionCode(QModbusResponse::ReadHoldingRegisters);
        response.setData(QByteArray::fromHex("05"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("04cd6b"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("04cd6b051755"));
        QCOMPARE(client.processResponse(response, &unit), false);
    }

    void testProcessReadInputRegistersResponse()
    {
        TestClient client;

        QModbusDataUnit unit;
        unit.setStartAddress(100);
        QModbusResponse response = QModbusResponse(QModbusResponse::ReadInputRegisters,
                                                   QByteArray::fromHex("04cd6b057f"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 2u);
        QCOMPARE(unit.startAddress(), 100);
        QCOMPARE(unit.values(), QVector<quint16>({ 52587, 1407 }));
        QCOMPARE(unit.registerType(), QModbusDataUnit::InputRegisters);

        response.setFunctionCode(QModbusPdu::FunctionCode(0x84));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setFunctionCode(QModbusResponse::ReadInputRegisters);
        response.setData(QByteArray::fromHex("05"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("04cd6b"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("04cd6b051755"));
        QCOMPARE(client.processResponse(response, &unit), false);
    }

    void testProcessWriteSingleRegisterResponse()
    {
        TestClient client;

        QModbusDataUnit unit;
        QModbusResponse response = QModbusResponse(QModbusResponse::WriteSingleRegister,
            QByteArray::fromHex("04cd6b05"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 1u);
        QCOMPARE(unit.startAddress(), 1229);
        QCOMPARE(unit.values(), QVector<quint16>(1, 27397u));
        QCOMPARE(unit.registerType(), QModbusDataUnit::HoldingRegisters);

        response.setFunctionCode(QModbusPdu::FunctionCode(0x86));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setFunctionCode(QModbusResponse::WriteSingleRegister);
        response.setData(QByteArray::fromHex("05"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("04cd6b051755"));
        QCOMPARE(client.processResponse(response, &unit), false);
    }

    void testProcessWriteMultipleRegistersResponse()
    {
        TestClient client;

        QModbusDataUnit unit;
        QModbusResponse response = QModbusResponse(QModbusResponse::WriteMultipleRegisters,
            QByteArray::fromHex("03cd007b"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 123u);
        QCOMPARE(unit.startAddress(), 973);
        QCOMPARE(unit.registerType(), QModbusDataUnit::HoldingRegisters);

        response.setFunctionCode(QModbusPdu::FunctionCode(0x90));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setFunctionCode(QModbusResponse::WriteMultipleRegisters);
        response.setData(QByteArray::fromHex("05"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("03cd6b"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("03cd6b0517"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("03cd0000"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("03cd007c"));
        QCOMPARE(client.processResponse(response, &unit), false);
    }

    void testProcessReadWriteMultipleRegistersResponse()
    {
        TestClient client;

        QModbusDataUnit unit;
        unit.setStartAddress(100);
        QModbusResponse response = QModbusResponse(QModbusResponse::ReadWriteMultipleRegisters,
            QByteArray::fromHex("04cd6b057f"));
        QCOMPARE(client.processResponse(response, &unit), true);

        QCOMPARE(unit.isValid(), true);
        QCOMPARE(unit.valueCount(), 2u);
        QCOMPARE(unit.values(), QVector<quint16>({ 52587, 1407 }));
        QCOMPARE(unit.registerType(), QModbusDataUnit::HoldingRegisters);

        response.setFunctionCode(QModbusPdu::FunctionCode(0x97));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setFunctionCode(QModbusResponse::ReadWriteMultipleRegisters);
        response.setData(QByteArray::fromHex("05"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("04cd6b"));
        QCOMPARE(client.processResponse(response, &unit), false);

        response.setData(QByteArray::fromHex("04cd6b051755"));
        QCOMPARE(client.processResponse(response, &unit), false);
    }

    void testPrivateCreateReadRequest_data()
    {
        QTest::addColumn<QModbusDataUnit::RegisterType>("rc");
        QTest::addColumn<int>("address");
        QTest::addColumn<int>("count");
        QTest::addColumn<QModbusPdu::FunctionCode>("fc");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<bool>("isValid");

        QTest::newRow("QModbusDataUnit::Invalid") << QModbusDataUnit::Invalid << 19 << 19
            << QModbusRequest::Invalid << QByteArray() << false;
        QTest::newRow("QModbusDataUnit::Coils") << QModbusDataUnit::Coils << 19 << 19
            << QModbusRequest::ReadCoils << QByteArray::fromHex("00130013") << true;
        QTest::newRow("QModbusDataUnit::DiscreteInputs") << QModbusDataUnit::DiscreteInputs << 19
            << 19 << QModbusRequest::ReadDiscreteInputs << QByteArray::fromHex("00130013") << true;
        QTest::newRow("QModbusDataUnit::InputRegisters") << QModbusDataUnit::InputRegisters << 19
            << 19 << QModbusRequest::ReadInputRegisters << QByteArray::fromHex("00130013") << true;
        QTest::newRow("QModbusDataUnit::HoldingRegisters") << QModbusDataUnit::HoldingRegisters
            << 19 << 19 << QModbusRequest::ReadHoldingRegisters << QByteArray::fromHex("00130013")
            << true;
    }

    void testPrivateCreateReadRequest()
    {
        QFETCH(QModbusDataUnit::RegisterType, rc);
        QFETCH(int, address);
        QFETCH(int, count);

        TestClient client;
        QModbusDataUnit read(rc, address, count);
        QModbusRequest request = client.d_func()->createReadRequest(read);
        QTEST(request.functionCode(), "fc");
        QTEST(request.data(), "data");
        QTEST(request.isValid(), "isValid");
    }

    void testPrivateCreateWriteRequest_data()
    {
        QTest::addColumn<QModbusDataUnit::RegisterType>("rc");
        QTest::addColumn<int>("address");
        QTest::addColumn<QVector<quint16>>("values");
        QTest::addColumn<QModbusPdu::FunctionCode>("fc");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<bool>("isValid");

        QTest::newRow("QModbusDataUnit::Invalid") << QModbusDataUnit::Invalid << 19
            << (QVector<quint16>() << 1) << QModbusRequest::Invalid << QByteArray() << false;
        QTest::newRow("QModbusDataUnit::DiscreteInputs") << QModbusDataUnit::DiscreteInputs << 19
            << (QVector<quint16>() << 1) << QModbusRequest::Invalid << QByteArray() << false;
        QTest::newRow("QModbusDataUnit::InputRegisters") << QModbusDataUnit::InputRegisters << 19
            << (QVector<quint16>() << 1) << QModbusRequest::Invalid << QByteArray() << false;

        QTest::newRow("QModbusDataUnit::Coils{Single}") << QModbusDataUnit::Coils << 172
            << (QVector<quint16>() << 1) << QModbusRequest::WriteSingleCoil
            << QByteArray::fromHex("00acff00") << true;
        QTest::newRow("QModbusDataUnit::Coils{Multiple}") << QModbusDataUnit::Coils << 19
            << (QVector<quint16>({ 1,0,1,1,0,0,1,1, 1,0 /* 6 times padding */ }))
            << QModbusRequest::WriteMultipleCoils << QByteArray::fromHex("0013000a02cd01")
            << true;
        QTest::newRow("QModbusDataUnit::HoldingRegisters{Single}")
            << QModbusDataUnit::HoldingRegisters << 1229 << (QVector<quint16>() << 27397u)
            << QModbusRequest::WriteSingleRegister << QByteArray::fromHex("04cd6b05") << true;
        QTest::newRow("QModbusDataUnit::HoldingRegisters{Multiple}")
            << QModbusDataUnit::HoldingRegisters << 1 << (QVector<quint16>() << 27397u << 27397u)
            << QModbusRequest::WriteMultipleRegisters << QByteArray::fromHex("00010002046b056b05")
            << true;
    }

    void testPrivateCreateWriteRequest()
    {
        QFETCH(QModbusDataUnit::RegisterType, rc);
        QFETCH(int, address);
        QFETCH(QVector<quint16>, values);

        TestClient client;
        QModbusDataUnit write(rc, address, values);
        QModbusRequest request = client.d_func()->createWriteRequest(write);
        QTEST(request.functionCode(), "fc");
        QTEST(request.data(), "data");
        QTEST(request.isValid(), "isValid");
    }

    void testPrivateCreateRWRequest_data()
    {
        QTest::addColumn<QModbusDataUnit::RegisterType>("rc");
        QTest::addColumn<int>("address");
        QTest::addColumn<QVector<quint16>>("values");
        QTest::addColumn<QModbusPdu::FunctionCode>("fc");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<bool>("isValid");

        QTest::newRow("QModbusDataUnit::Invalid") << QModbusDataUnit::Invalid << 19
            << (QVector<quint16>() << 1) << QModbusRequest::Invalid << QByteArray() << false;
        QTest::newRow("QModbusDataUnit::Coils") << QModbusDataUnit::Invalid << 172
            << (QVector<quint16>() << 1) << QModbusRequest::Invalid << QByteArray() << false;
        QTest::newRow("QModbusDataUnit::DiscreteInputs") << QModbusDataUnit::DiscreteInputs << 19
            << (QVector<quint16>() << 1) << QModbusRequest::Invalid << QByteArray() << false;
        QTest::newRow("QModbusDataUnit::InputRegisters") << QModbusDataUnit::InputRegisters << 19
            << (QVector<quint16>() << 1) << QModbusRequest::Invalid << QByteArray() << false;
        QTest::newRow("QModbusDataUnit::HoldingRegisters{Read|Write}")
            << QModbusDataUnit::HoldingRegisters << 1 << (QVector<quint16>() << 27397u << 27397u)
            << QModbusRequest::ReadWriteMultipleRegisters
            << QByteArray::fromHex("0001000200010002046b056b05") << true;
    }

    void testPrivateCreateRWRequest()
    {
        QFETCH(QModbusDataUnit::RegisterType, rc);
        QFETCH(int, address);
        QFETCH(QVector<quint16>, values);

        TestClient client;
        QModbusDataUnit read(rc, address, values.count());
        QModbusDataUnit write(rc, address, values);
        QModbusRequest request = client.d_func()->createRWRequest(read, write);
        QTEST(request.functionCode(), "fc");
        QTEST(request.data(), "data");
        QTEST(request.isValid(), "isValid");
    }

    void testPrivateSendRequest()
    {
        TestClient client;
        QModbusDataUnit unit;
        QModbusReply *reply = nullptr;

        QTest::ignoreMessage(QtWarningMsg, "(Client) Device is not connected");
        QCOMPARE(client.d_func()->sendRequest(QModbusRequest(), 1, &unit), reply);
        QCOMPARE(client.error(), QModbusDevice::ConnectionError);
        QCOMPARE(client.errorString(), QString("Device not connected."));

        QTest::ignoreMessage(QtWarningMsg, "(Client) Device is not connected");
        QCOMPARE(client.d_func()->sendRequest(QModbusRequest(), 1, nullptr), reply);
        QCOMPARE(client.error(), QModbusDevice::ConnectionError);
        QCOMPARE(client.errorString(), QString("Device not connected."));

        QCOMPARE(client.connectDevice(), true);

        QTest::ignoreMessage(QtWarningMsg, "(Client) Refuse to send invalid request.");
        QCOMPARE(client.d_func()->sendRequest(QModbusRequest(), 1, &unit), reply);
        QCOMPARE(client.error(), QModbusDevice::ProtocolError);
        QCOMPARE(client.errorString(), QString("Invalid Modbus request."));

        QTest::ignoreMessage(QtWarningMsg, "(Client) Refuse to send invalid request.");
        QCOMPARE(client.d_func()->sendRequest(QModbusRequest(), 1, nullptr), reply);
        QCOMPARE(client.error(), QModbusDevice::ProtocolError);
        QCOMPARE(client.errorString(), QString("Invalid Modbus request."));

        QModbusRequest request(QModbusRequest::Diagnostics);
        // The default implementation is empty and returns a null pointer.
        QCOMPARE(client.d_func()->sendRequest(request, 1, &unit), reply);
        QCOMPARE(client.d_func()->sendRequest(request, 1, nullptr), reply);
    }
};

QTEST_MAIN(tst_QModbusClient)

#include "tst_qmodbusclient.moc"
