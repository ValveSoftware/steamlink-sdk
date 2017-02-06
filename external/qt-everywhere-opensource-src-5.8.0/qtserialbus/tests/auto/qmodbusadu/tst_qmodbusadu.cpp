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

#include <private/qmodbusadu_p.h>

#include <QtTest/QtTest>

class tst_QModbusAdu : public QObject
{
    Q_OBJECT

private slots:
    void testQByteArrayConstructor()
    {
        QModbusSerialAdu adu(QModbusSerialAdu::Ascii, ":f0010300120008f2\r\n");
        QCOMPARE(adu.size(), 7);
        QCOMPARE(adu.data(), QByteArray::fromHex("f0010300120008"));

        QCOMPARE(adu.rawSize(), 19);
        QCOMPARE(adu.rawData(), QByteArray(":f0010300120008f2\r\n"));

        adu = QModbusSerialAdu(QModbusSerialAdu::Rtu, QByteArray::fromHex("f00103001200080f1d"));
        QCOMPARE(adu.size(), 7);
        QCOMPARE(adu.data(), QByteArray::fromHex("f0010300120008"));

        QCOMPARE(adu.rawSize(), 9);
        QCOMPARE(adu.rawData(), QByteArray::fromHex("f00103001200080f1d"));
    }

    void testSlaveAddress()
    {
        QModbusSerialAdu adu(QModbusSerialAdu::Ascii, ":f0010300120008f2\r\n");
        QCOMPARE(adu.serverAddress(), 240);

        adu = QModbusSerialAdu(QModbusSerialAdu::Rtu, QByteArray::fromHex("f00103001200080f1d"));
        QCOMPARE(adu.serverAddress(), 240);
    }

    void testPdu()
    {
        QModbusSerialAdu adu(QModbusSerialAdu::Ascii, ":f0010300120008f2\r\n");
        QCOMPARE(adu.pdu().functionCode(), QModbusPdu::ReadCoils);
        QCOMPARE(adu.pdu().data(), QByteArray::fromHex("0300120008"));

        adu = QModbusSerialAdu(QModbusSerialAdu::Rtu, QByteArray::fromHex("f00103001200080f1d"));
        QCOMPARE(adu.pdu().functionCode(), QModbusPdu::ReadCoils);
        QCOMPARE(adu.pdu().data(), QByteArray::fromHex("0300120008"));
    }

    void testChecksum()
    {
        QModbusSerialAdu adu(QModbusSerialAdu::Ascii, ":f0010300120008f2\r\n");
        QCOMPARE(adu.checksum<quint8>(), quint8(0xf2));

        adu = QModbusSerialAdu(QModbusSerialAdu::Rtu, QByteArray::fromHex("f00103001200080f1d"));
        QCOMPARE(adu.checksum<quint16>(), quint16(0x0f1d));
    }

    void testMatchingChecksum()
    {
        QModbusSerialAdu adu(QModbusSerialAdu::Ascii, ":f0010300120008f2\r\n");
        QCOMPARE(adu.matchingChecksum(), true);

        adu = QModbusSerialAdu(QModbusSerialAdu::Rtu, QByteArray::fromHex("f00103001200080f1d"));
        QCOMPARE(adu.matchingChecksum(), true);
    }

    void testCreate()
    {
        const QModbusRequest pdu(QModbusPdu::ReadHoldingRegisters, QByteArray::fromHex("006B0003"));

        QModbusSerialAdu adu(QModbusSerialAdu::Ascii, ":1103006b00037e\r\n");
        QByteArray ba = QModbusSerialAdu::create(QModbusSerialAdu::Ascii, 17, pdu);
        QCOMPARE(adu.rawData(), ba);
        QCOMPARE(adu.data(), QModbusSerialAdu(QModbusSerialAdu::Ascii, ba).data());

        adu = QModbusSerialAdu(QModbusSerialAdu::Rtu, QByteArray::fromHex("1103006b00037687"));
        ba = QModbusSerialAdu::create(QModbusSerialAdu::Rtu, 17, pdu);
        QCOMPARE(ba, adu.rawData());
        QCOMPARE(adu.data(), QModbusSerialAdu(QModbusSerialAdu::Rtu, ba).data());
    }

    void testChecksumLRC_data()
    {
        // Modbus ASCII Messages generated with pymodbus message-generator.py

        QTest::addColumn<QByteArray>("pdu");
        QTest::addColumn<quint8>("lrc");

        QTest::newRow(":0107F8")
            << QByteArray::fromHex("0107")
            << quint8(0xF8);
        QTest::newRow(":010BF4")
            << QByteArray::fromHex("010B")
            << quint8(0xF4);
        QTest::newRow(":010CF3")
            << QByteArray::fromHex("010C")
            << quint8(0xF3);
        QTest::newRow(":0111EE")
            << QByteArray::fromHex("0111")
            << quint8(0xEE);
        QTest::newRow(":011400EB")
            << QByteArray::fromHex("011400")
            << quint8(0xEB);
        QTest::newRow(":011500EA")
            << QByteArray::fromHex("011500")
            << quint8(0xEA);
        QTest::newRow(":1103006B00037E")
            << QByteArray::fromHex("1103006B0003")
            << quint8(0x7E);
        QTest::newRow(":01160012FFFF0000D9")
            << QByteArray::fromHex("01160012FFFF0000")
            << quint8(0xD9);
        QTest::newRow(":0110001200081000010001000100010001000100010001BD")
            << QByteArray::fromHex("0110001200081000010001000100010001000100010001")
            << quint8(0xBD);
        QTest::newRow(":011700120008000000081000010001000100010001000100010001AE")
            << QByteArray::fromHex("011700120008000000081000010001000100010001000100010001")
            << quint8(0xAE);

    }
    void testChecksumLRC()
    {
        QFETCH(QByteArray, pdu);
        QFETCH(quint8, lrc);
        QCOMPARE(QModbusSerialAdu::calculateLRC(pdu.constData(), pdu.size()), lrc);
    }

    void testChecksumCRC_data()
    {
        // Modbus RTU Messages generated with pymodbus message-generator.py

        QTest::addColumn<QByteArray>("pdu");
        QTest::addColumn<quint16>("crc");

        QTest::newRow("010300120008e409") << QByteArray::fromHex("010300120008") << quint16(0xe409);
        QTest::newRow("010200120008d9c9") << QByteArray::fromHex("010200120008") << quint16(0xd9c9);
        QTest::newRow("01040012000851c9") << QByteArray::fromHex("010400120008") << quint16(0x51c9);
        QTest::newRow("0101001200089dc9") << QByteArray::fromHex("010100120008") << quint16(0x9dc9);
        QTest::newRow("010f0012000801ff06d6") << QByteArray::fromHex("010f0012000801ff")
            << quint16(0x06d6);
        QTest::newRow("0110001200081000010001000100010001000100010001d551")
            << QByteArray::fromHex("0110001200081000010001000100010001000100010001")
            << quint16(0xd551);
        QTest::newRow("010600120001e80f") << QByteArray::fromHex("010600120001") << quint16(0xe80f);
        QTest::newRow("01050012ff002c3f") << QByteArray::fromHex("01050012ff00") << quint16(0x2c3f);
        QTest::newRow("011700120008000000081000010001000100010001000100010001e6f8")
            << QByteArray::fromHex("011700120008000000081000010001000100010001000100010001")
            << quint16(0xe6f8);
        QTest::newRow("010741e2") << QByteArray::fromHex("0107") << quint16(0x41e2);
        QTest::newRow("010b41e7") << QByteArray::fromHex("010b") << quint16(0x41e7);
        QTest::newRow("010c0025") << QByteArray::fromHex("010c") << quint16(0x0025);
        QTest::newRow("0111c02c") << QByteArray::fromHex("0111") << quint16(0xc02c);
        QTest::newRow("0114002f00") << QByteArray::fromHex("011400") << quint16(0x2f00);
        QTest::newRow("0115002e90") << QByteArray::fromHex("011500") << quint16(0x2e90);
        QTest::newRow("01160012ffff00004e21") << QByteArray::fromHex("01160012ffff0000")
            << quint16(0x4e21);
        QTest::newRow("0118001201d2") << QByteArray::fromHex("01180012") << quint16(0x01d2);
        QTest::newRow("012b0e01007077") << QByteArray::fromHex("012b0e0100") << quint16(0x7077);
        QTest::newRow("010800000000e00b") << QByteArray::fromHex("010800000000") << quint16(0xe00b);
        QTest::newRow("010800010000b1cb") << QByteArray::fromHex("010800010000") << quint16(0xb1cb);
        QTest::newRow("01080002000041cb") << QByteArray::fromHex("010800020000") << quint16(0x41cb);
        QTest::newRow("010800030000100b") << QByteArray::fromHex("010800030000") << quint16(0x100b);
        QTest::newRow("010800040000a1ca") << QByteArray::fromHex("010800040000") << quint16(0xa1ca);
        QTest::newRow("0108000a0000c009") << QByteArray::fromHex("0108000a0000") << quint16(0xc009);
        QTest::newRow("0108000b000091c9") << QByteArray::fromHex("0108000b0000") << quint16(0x91c9);
        QTest::newRow("0108000c00002008") << QByteArray::fromHex("0108000c0000") << quint16(0x2008);
        QTest::newRow("0108000d000071c8") << QByteArray::fromHex("0108000d0000") << quint16(0x71c8);
        QTest::newRow("0108000e000081c8") << QByteArray::fromHex("0108000e0000") << quint16(0x81c8);
        QTest::newRow("0108000f0000d008") << QByteArray::fromHex("0108000f0000") << quint16(0xd008);
        QTest::newRow("010800100000e1ce") << QByteArray::fromHex("010800100000") << quint16(0xe1ce);
        QTest::newRow("010800110000b00e") << QByteArray::fromHex("010800110000") << quint16(0xb00e);
        QTest::newRow("010800120000400e") << QByteArray::fromHex("010800120000") << quint16(0x400e);
        QTest::newRow("01080013000011ce") << QByteArray::fromHex("010800130000") << quint16(0x11ce);
        QTest::newRow("010800140000a00f") << QByteArray::fromHex("010800140000") << quint16(0xa00f);
        QTest::newRow("010800150000f1cf") << QByteArray::fromHex("010800150000") << quint16(0xf1cf);
    }

    void testChecksumCRC()
    {
        QFETCH(QByteArray, pdu);
        QFETCH(quint16, crc);
        QCOMPARE(QModbusSerialAdu::calculateCRC(pdu.constData(), pdu.size()), crc);
    }
};

QTEST_MAIN(tst_QModbusAdu)

#include "tst_qmodbusadu.moc"
