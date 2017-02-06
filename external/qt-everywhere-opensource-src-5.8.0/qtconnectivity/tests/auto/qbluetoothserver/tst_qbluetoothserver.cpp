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

#include <QDebug>

#include <qbluetoothserver.h>
#include <qbluetoothsocket.h>
#include <qbluetoothlocaldevice.h>

QT_USE_NAMESPACE

//same uuid as tests/bttestui
#define TEST_SERVICE_UUID "e8e10f95-1a70-4b27-9ccf-02010264e9c8"

Q_DECLARE_METATYPE(QBluetooth::SecurityFlags)

// Max time to wait for connection
static const int MaxConnectTime = 60 * 1000;   // 1 minute in ms

class tst_QBluetoothServer : public QObject
{
    Q_OBJECT

public:
    tst_QBluetoothServer();
    ~tst_QBluetoothServer();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void tst_construction();

    void tst_receive_data();
    void tst_receive();

    void setHostMode(const QBluetoothAddress &localAdapter, QBluetoothLocalDevice::HostMode newHostMode);

private:
    QBluetoothLocalDevice localDevice;
    QBluetoothLocalDevice::HostMode initialHostMode;
};

tst_QBluetoothServer::tst_QBluetoothServer()
{
}

tst_QBluetoothServer::~tst_QBluetoothServer()
{
}

void tst_QBluetoothServer::setHostMode(const QBluetoothAddress &localAdapter,
                                    QBluetoothLocalDevice::HostMode newHostMode)
{
    QBluetoothLocalDevice device(localAdapter);
    QSignalSpy hostModeSpy(&device, SIGNAL(hostModeStateChanged(QBluetoothLocalDevice::HostMode)));

    if (!device.isValid())
        return;

    if (device.hostMode() == newHostMode)
        return;

    //We distinguish powerOn() and setHostMode(HostConnectable) because
    //Android uses different permission levels, we want to avoid interaction with user
    //which implies usage of powerOn -> unfortunately powerOn() is not equivalent to HostConnectable
    //Unfortunately the discoverable mode always requires a user confirmation
    switch (newHostMode) {
    case QBluetoothLocalDevice::HostPoweredOff:
    case QBluetoothLocalDevice::HostDiscoverable:
    case QBluetoothLocalDevice::HostDiscoverableLimitedInquiry:
        device.setHostMode(newHostMode);
        break;
    case QBluetoothLocalDevice::HostConnectable:
        device.powerOn();
        break;
    }

    int connectTime = 5000;  // ms
    while (hostModeSpy.count() < 1 && connectTime > 0) {
        QTest::qWait(500);
        connectTime -= 500;
    }
}

void tst_QBluetoothServer::initTestCase()
{
    qRegisterMetaType<QBluetooth::SecurityFlags>();
    qRegisterMetaType<QBluetoothServer::Error>();

    QBluetoothLocalDevice device;
    if (!device.isValid())
        return;

    initialHostMode = device.hostMode();
#ifdef Q_OS_OSX
    if (initialHostMode == QBluetoothLocalDevice::HostPoweredOff)
        return;
#endif

    setHostMode(device.address(), QBluetoothLocalDevice::HostConnectable);

    QBluetoothLocalDevice::HostMode hostMode= localDevice.hostMode();

    QVERIFY(hostMode != QBluetoothLocalDevice::HostPoweredOff);
}

void tst_QBluetoothServer::cleanupTestCase()
{
    QBluetoothLocalDevice device;
    setHostMode(device.address(), initialHostMode);
}

void tst_QBluetoothServer::tst_construction()
{
    {
        QBluetoothServer server(QBluetoothServiceInfo::RfcommProtocol);

        QVERIFY(!server.isListening());
        QCOMPARE(server.maxPendingConnections(), 1);
        QVERIFY(!server.hasPendingConnections());
        QVERIFY(server.nextPendingConnection() == 0);
        QCOMPARE(server.error(), QBluetoothServer::NoError);
        QCOMPARE(server.serverType(), QBluetoothServiceInfo::RfcommProtocol);
    }

    {
        QBluetoothServer server(QBluetoothServiceInfo::L2capProtocol);

        QVERIFY(!server.isListening());
        QCOMPARE(server.maxPendingConnections(), 1);
        QVERIFY(!server.hasPendingConnections());
        QVERIFY(server.nextPendingConnection() == 0);
        QCOMPARE(server.error(), QBluetoothServer::NoError);
        QCOMPARE(server.serverType(), QBluetoothServiceInfo::L2capProtocol);
    }
}

void tst_QBluetoothServer::tst_receive_data()
{
    QTest::addColumn<QBluetoothLocalDevice::HostMode>("hostmode");
    QTest::newRow("offline mode") << QBluetoothLocalDevice::HostPoweredOff;
    QTest::newRow("online mode") << QBluetoothLocalDevice::HostConnectable;
}

void tst_QBluetoothServer::tst_receive()
{
    QFETCH(QBluetoothLocalDevice::HostMode, hostmode);

    QBluetoothLocalDevice localDev;
#ifdef Q_OS_OSX
    if (localDev.hostMode() == QBluetoothLocalDevice::HostPoweredOff)
        QSKIP("On OS X this test requires Bluetooth adapter ON");
#endif
    const QBluetoothAddress address = localDev.address();

    bool localDeviceAvailable = localDev.isValid();

    if (localDeviceAvailable) {
        // setHostMode is noop on OS X.
        setHostMode(address, hostmode);

        if (hostmode == QBluetoothLocalDevice::HostPoweredOff) {
#ifndef Q_OS_OSX
            QCOMPARE(localDevice.hostMode(), hostmode);
#endif
        } else {
            QVERIFY(localDevice.hostMode() != QBluetoothLocalDevice::HostPoweredOff);
        }
    }
    QBluetoothServer server(QBluetoothServiceInfo::RfcommProtocol);
    QSignalSpy errorSpy(&server, SIGNAL(error(QBluetoothServer::Error)));

    bool result = server.listen(address, 20);  // port == 20
    QTest::qWait(1000);

    if (!result) {
        QCOMPARE(server.serverAddress(), QBluetoothAddress());
        QCOMPARE(server.serverPort(), quint16(0));
        QVERIFY(errorSpy.count() > 0);
        QVERIFY(!server.isListening());
        if (!localDeviceAvailable) {
            QVERIFY(server.error() != QBluetoothServer::NoError);
        } else {
            //local device but poweredOff
            QCOMPARE(server.error(), QBluetoothServer::PoweredOffError);
        }
        return;
    }

    QVERIFY(result);

    QVERIFY(QBluetoothLocalDevice::allDevices().count());
    QCOMPARE(server.error(), QBluetoothServer::NoError);
    QCOMPARE(server.serverAddress(), address);
    QCOMPARE(server.serverPort(), quint16(20));
    QVERIFY(server.isListening());
    QVERIFY(!server.hasPendingConnections());

    server.close();
    QCOMPARE(server.error(), QBluetoothServer::NoError);
    QVERIFY(server.serverAddress() == address || server.serverAddress() == QBluetoothAddress());
    QVERIFY(server.serverPort() == 0);
    QVERIFY(!server.isListening());
    QVERIFY(!server.hasPendingConnections());
}


QTEST_MAIN(tst_QBluetoothServer)

#include "tst_qbluetoothserver.moc"
