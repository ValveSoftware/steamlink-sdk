/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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
#include <QUuid>

#include <QDebug>

#include <qbluetoothdeviceinfo.h>
#include <qbluetoothserviceinfo.h>
#include <qbluetoothaddress.h>
#include <qbluetoothlocaldevice.h>
#include <qbluetoothuuid.h>
#include <QtBluetooth/QBluetoothServer>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QBluetoothUuid::ProtocolUuid)
Q_DECLARE_METATYPE(QUuid)
Q_DECLARE_METATYPE(QBluetoothServiceInfo::Protocol)

class tst_QBluetoothServiceInfo : public QObject
{
    Q_OBJECT

public:
    tst_QBluetoothServiceInfo();
    ~tst_QBluetoothServiceInfo();

private slots:
    void initTestCase();

    void tst_construction();

    void tst_assignment_data();
    void tst_assignment();

    void tst_serviceClassUuids();
};

tst_QBluetoothServiceInfo::tst_QBluetoothServiceInfo()
{
}

tst_QBluetoothServiceInfo::~tst_QBluetoothServiceInfo()
{
}

void tst_QBluetoothServiceInfo::initTestCase()
{
    qRegisterMetaType<QBluetoothUuid::ProtocolUuid>();
    qRegisterMetaType<QUuid>();
    qRegisterMetaType<QBluetoothServiceInfo::Protocol>();
    // start Bluetooth if not started
    QBluetoothLocalDevice *device = new QBluetoothLocalDevice();
    device->powerOn();
    delete device;
}

void tst_QBluetoothServiceInfo::tst_construction()
{
    const QString serviceName("My Service");
    const QString alternateServiceName("Another ServiceName");
    const QBluetoothDeviceInfo deviceInfo(QBluetoothAddress("001122334455"), "Test Device", 0);
    const QBluetoothDeviceInfo alternatedeviceInfo(QBluetoothAddress("554433221100"), "Test Device2", 0);

    QList<QBluetoothUuid::ProtocolUuid> protUuids;
    //list taken from qbluetoothuuid.h
    protUuids << QBluetoothUuid::Sdp;
    protUuids << QBluetoothUuid::Udp;
    protUuids << QBluetoothUuid::Rfcomm;
    protUuids << QBluetoothUuid::Tcp;
    protUuids << QBluetoothUuid::TcsBin;
    protUuids << QBluetoothUuid::TcsAt;
    protUuids << QBluetoothUuid::Att;
    protUuids << QBluetoothUuid::Obex;
    protUuids << QBluetoothUuid::Ip;
    protUuids << QBluetoothUuid::Ftp;
    protUuids << QBluetoothUuid::Http;
    protUuids << QBluetoothUuid::Wsp;
    protUuids << QBluetoothUuid::Bnep;
    protUuids << QBluetoothUuid::Upnp;
    protUuids << QBluetoothUuid::Hidp;
    protUuids << QBluetoothUuid::HardcopyControlChannel;
    protUuids << QBluetoothUuid::HardcopyDataChannel;
    protUuids << QBluetoothUuid::HardcopyNotification;
    protUuids << QBluetoothUuid::Avctp;
    protUuids << QBluetoothUuid::Avdtp;
    protUuids << QBluetoothUuid::Cmtp;
    protUuids << QBluetoothUuid::UdiCPlain;
    protUuids << QBluetoothUuid::McapControlChannel;
    protUuids << QBluetoothUuid::McapDataChannel;
    protUuids << QBluetoothUuid::L2cap;

    {
        QBluetoothServiceInfo serviceInfo;

        QVERIFY(!serviceInfo.isValid());
        QVERIFY(!serviceInfo.isComplete());
        QVERIFY(!serviceInfo.isRegistered());
        QCOMPARE(serviceInfo.serviceName(), QString());
        QCOMPARE(serviceInfo.serviceDescription(), QString());
        QCOMPARE(serviceInfo.serviceProvider(), QString());
        QCOMPARE(serviceInfo.serviceUuid(), QBluetoothUuid());
        QCOMPARE(serviceInfo.serviceClassUuids().count(), 0);
        QCOMPARE(serviceInfo.attributes().count(), 0);
        QCOMPARE(serviceInfo.serverChannel(), -1);
        QCOMPARE(serviceInfo.protocolServiceMultiplexer(), -1);

        foreach (QBluetoothUuid::ProtocolUuid u, protUuids)
            QCOMPARE(serviceInfo.protocolDescriptor(u).count(), 0);
    }

    {
        QBluetoothServiceInfo serviceInfo;
        serviceInfo.setServiceName(serviceName);
        serviceInfo.setDevice(deviceInfo);

        QVERIFY(serviceInfo.isValid());
        QVERIFY(!serviceInfo.isComplete());
        QVERIFY(!serviceInfo.isRegistered());

        QCOMPARE(serviceInfo.serviceName(), serviceName);
        QCOMPARE(serviceInfo.device().address(), deviceInfo.address());

        QBluetoothServiceInfo copyInfo(serviceInfo);

        QVERIFY(copyInfo.isValid());
        QVERIFY(!copyInfo.isComplete());
        QVERIFY(!copyInfo.isRegistered());

        QCOMPARE(copyInfo.serviceName(), serviceName);
        QCOMPARE(copyInfo.device().address(), deviceInfo.address());


        copyInfo.setAttribute(QBluetoothServiceInfo::ServiceName, alternateServiceName);
        copyInfo.setDevice(alternatedeviceInfo);
        QCOMPARE(copyInfo.serviceName(), alternateServiceName);
        QCOMPARE(copyInfo.attribute(QBluetoothServiceInfo::ServiceName).toString(), alternateServiceName);
        QCOMPARE(serviceInfo.serviceName(), alternateServiceName);
        QCOMPARE(copyInfo.device().address(), alternatedeviceInfo.address());
        QCOMPARE(serviceInfo.device().address(), alternatedeviceInfo.address());

        foreach (QBluetoothUuid::ProtocolUuid u, protUuids)
            QCOMPARE(serviceInfo.protocolDescriptor(u).count(), 0);
        foreach (QBluetoothUuid::ProtocolUuid u, protUuids)
            QCOMPARE(copyInfo.protocolDescriptor(u).count(), 0);
    }
}

void tst_QBluetoothServiceInfo::tst_assignment_data()
{
    QTest::addColumn<QUuid>("uuid");
    QTest::addColumn<QBluetoothUuid::ProtocolUuid>("protocolUuid");
    QTest::addColumn<QBluetoothServiceInfo::Protocol>("serviceInfoProtocol");
    QTest::addColumn<bool>("protocolSupported");

    bool l2cpSupported = true;
    //some platforms don't support L2CP
#ifdef QT_ANDROID_BLUETOOTH
    l2cpSupported = false;
#endif
    QTest::newRow("assignment_data_l2cp")
        << QUuid(0x67c8770b, 0x44f1, 0x410a, 0xab, 0x9a, 0xf9, 0xb5, 0x44, 0x6f, 0x13, 0xee)
        << QBluetoothUuid::L2cap << QBluetoothServiceInfo::L2capProtocol << l2cpSupported;
    QTest::newRow("assignment_data_rfcomm")
        << QUuid(0x67c8770b, 0x44f1, 0x410a, 0xab, 0x9a, 0xf9, 0xb5, 0x44, 0x6f, 0x13, 0xee)
        << QBluetoothUuid::Rfcomm << QBluetoothServiceInfo::RfcommProtocol << true;

}

void tst_QBluetoothServiceInfo::tst_assignment()
{
    QFETCH(QUuid, uuid);
    QFETCH(QBluetoothUuid::ProtocolUuid, protocolUuid);
    QFETCH(QBluetoothServiceInfo::Protocol, serviceInfoProtocol);
    QFETCH(bool, protocolSupported);

    const QString serviceName("My Service");
    const QBluetoothDeviceInfo deviceInfo(QBluetoothAddress("001122334455"), "Test Device", 0);

    QBluetoothServiceInfo serviceInfo;
    serviceInfo.setServiceName(serviceName);
    serviceInfo.setDevice(deviceInfo);

    QVERIFY(serviceInfo.isValid());
    QVERIFY(!serviceInfo.isRegistered());
    QVERIFY(!serviceInfo.isComplete());

    {
        QBluetoothServiceInfo copyInfo = serviceInfo;

        QVERIFY(copyInfo.isValid());
        QVERIFY(!copyInfo.isRegistered());
        QVERIFY(!copyInfo.isComplete());

        QCOMPARE(copyInfo.serviceName(), serviceName);
        QCOMPARE(copyInfo.device().address(), deviceInfo.address());
    }

    {
        QBluetoothServiceInfo copyInfo;

        QVERIFY(!copyInfo.isValid());
        QVERIFY(!copyInfo.isRegistered());
        QVERIFY(!copyInfo.isComplete());

        copyInfo = serviceInfo;

        QVERIFY(copyInfo.isValid());
        QVERIFY(!copyInfo.isRegistered());
        QVERIFY(!copyInfo.isComplete());

        QCOMPARE(copyInfo.serviceName(), serviceName);
        QCOMPARE(copyInfo.device().address(), deviceInfo.address());
    }

    {
        QBluetoothServiceInfo copyInfo1;
        QBluetoothServiceInfo copyInfo2;

        QVERIFY(!copyInfo1.isValid());
        QVERIFY(!copyInfo1.isRegistered());
        QVERIFY(!copyInfo1.isComplete());
        QVERIFY(!copyInfo2.isValid());
        QVERIFY(!copyInfo2.isRegistered());
        QVERIFY(!copyInfo2.isComplete());

        copyInfo1 = copyInfo2 = serviceInfo;

        QVERIFY(copyInfo1.isValid());
        QVERIFY(!copyInfo1.isRegistered());
        QVERIFY(!copyInfo1.isComplete());
        QVERIFY(copyInfo2.isValid());
        QVERIFY(!copyInfo2.isRegistered());
        QVERIFY(!copyInfo2.isComplete());

        QCOMPARE(copyInfo1.serviceName(), serviceName);
        QCOMPARE(copyInfo2.serviceName(), serviceName);
        QCOMPARE(copyInfo1.device().address(), deviceInfo.address());
        QCOMPARE(copyInfo2.device().address(), deviceInfo.address());
    }

    {
        QBluetoothServiceInfo copyInfo;
        QVERIFY(!copyInfo.isValid());
        QVERIFY(!copyInfo.isRegistered());
        QVERIFY(!copyInfo.isComplete());
        copyInfo = serviceInfo;
        QVERIFY(copyInfo.contains(QBluetoothServiceInfo::ServiceName));

        copyInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList, QBluetoothUuid(uuid));
        QVERIFY(copyInfo.contains(QBluetoothServiceInfo::ProtocolDescriptorList));
        QVERIFY(copyInfo.isComplete());
        QVERIFY(copyInfo.attributes().count() > 0);

        copyInfo.removeAttribute(QBluetoothServiceInfo::ProtocolDescriptorList);
        QVERIFY(!copyInfo.contains(QBluetoothServiceInfo::ProtocolDescriptorList));
        QVERIFY(!copyInfo.isComplete());
    }

    {
        QBluetoothServiceInfo copyInfo;
        QVERIFY(!copyInfo.isValid());
        copyInfo = serviceInfo;

        QVERIFY(copyInfo.serverChannel() == -1);
        QVERIFY(copyInfo.protocolServiceMultiplexer() == -1);

        QBluetoothServiceInfo::Sequence protocolDescriptorList;
        QBluetoothServiceInfo::Sequence protocol;
        protocol << QVariant::fromValue(QBluetoothUuid(protocolUuid));
        protocolDescriptorList.append(QVariant::fromValue(protocol));
        protocol.clear();

        protocolDescriptorList.append(QVariant::fromValue(protocol));
        copyInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,
                                 protocolDescriptorList);
        if (serviceInfoProtocol == QBluetoothServiceInfo::L2capProtocol) {
            QVERIFY(copyInfo.serverChannel() == -1);
            QVERIFY(copyInfo.protocolServiceMultiplexer() != -1);
        } else if (serviceInfoProtocol == QBluetoothServiceInfo::RfcommProtocol) {
            QVERIFY(copyInfo.serverChannel() != -1);
            QVERIFY(copyInfo.protocolServiceMultiplexer() == -1);
        }

        QVERIFY(copyInfo.socketProtocol() == serviceInfoProtocol);
    }

    {
        QBluetoothServiceInfo copyInfo;

        QVERIFY(!copyInfo.isValid());
        copyInfo = serviceInfo;
        copyInfo.setServiceUuid(QBluetoothUuid::SerialPort);
        QVERIFY(!copyInfo.isRegistered());

        if (!QBluetoothLocalDevice::allDevices().count()) {
            QSKIP("Skipping test due to missing Bluetooth device");
        } else if (protocolSupported) {
            QBluetoothServer server(serviceInfoProtocol);
            QVERIFY(server.listen());
            QTRY_VERIFY_WITH_TIMEOUT(server.isListening(), 5000);
            QVERIFY(server.serverPort() > 0);

            QBluetoothServiceInfo::Sequence protocolDescriptorList;
            QBluetoothServiceInfo::Sequence protocol;
            protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));

            if (serviceInfoProtocol == QBluetoothServiceInfo::L2capProtocol) {
                protocol << QVariant::fromValue(server.serverPort());
                protocolDescriptorList.append(QVariant::fromValue(protocol));
            } else if (serviceInfoProtocol == QBluetoothServiceInfo::RfcommProtocol) {
                protocolDescriptorList.append(QVariant::fromValue(protocol));
                protocol.clear();
                protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
                         << QVariant::fromValue(quint8(server.serverPort()));
                protocolDescriptorList.append(QVariant::fromValue(protocol));
            }

            serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,
                                     protocolDescriptorList);

            QVERIFY(copyInfo.registerService());
            QVERIFY(copyInfo.isRegistered());
            QVERIFY(serviceInfo.isRegistered());
            QBluetoothServiceInfo secondCopy;
            secondCopy = copyInfo;
            QVERIFY(secondCopy.isRegistered());

            QVERIFY(secondCopy.unregisterService());
            QVERIFY(!copyInfo.isRegistered());
            QVERIFY(!secondCopy.isRegistered());
            QVERIFY(!serviceInfo.isRegistered());
            QVERIFY(server.isListening());
            server.close();
            QVERIFY(!server.isListening());
        }
    }
}

void tst_QBluetoothServiceInfo::tst_serviceClassUuids()
{
    QBluetoothServiceInfo info;
    QCOMPARE(info.serviceClassUuids().count(), 0);

    QBluetoothServiceInfo::Sequence classIds;
    classIds << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
    QCOMPARE(classIds.count(), 1);

    QBluetoothUuid uuid(QString("e8e10f95-1a70-4b27-9ccf-02010264e9c8"));
    classIds.prepend(QVariant::fromValue(uuid));
    QCOMPARE(classIds.count(), 2);
    QCOMPARE(classIds.at(0).value<QBluetoothUuid>(), uuid);

    info.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classIds);
    QList<QBluetoothUuid> svclids = info.serviceClassUuids();
    QCOMPARE(svclids.count(), 2);
    QCOMPARE(svclids.at(0), uuid);
    QCOMPARE(svclids.at(1), QBluetoothUuid(QBluetoothUuid::SerialPort));
}

QTEST_MAIN(tst_QBluetoothServiceInfo)

#include "tst_qbluetoothserviceinfo.moc"
