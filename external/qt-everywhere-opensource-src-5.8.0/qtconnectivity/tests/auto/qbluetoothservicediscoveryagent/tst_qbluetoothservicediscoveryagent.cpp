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
#include <QLoggingCategory>
#include <QVariant>
#include <QList>

#include <qbluetoothaddress.h>
#include <qbluetoothdevicediscoveryagent.h>
#include <qbluetoothservicediscoveryagent.h>
#include <qbluetoothlocaldevice.h>
#include <qbluetoothserver.h>
#include <qbluetoothserviceinfo.h>

QT_USE_NAMESPACE

// Maximum time to for bluetooth device scan
const int MaxScanTime = 5 * 60 * 1000;  // 5 minutes in ms

class tst_QBluetoothServiceDiscoveryAgent : public QObject
{
    Q_OBJECT

public:
    tst_QBluetoothServiceDiscoveryAgent();
    ~tst_QBluetoothServiceDiscoveryAgent();

public slots:
    void deviceDiscoveryDebug(const QBluetoothDeviceInfo &info);
    void serviceDiscoveryDebug(const QBluetoothServiceInfo &info);
    void serviceError(const QBluetoothServiceDiscoveryAgent::Error err);

private slots:
    void initTestCase();

    void tst_invalidBtAddress();
    void tst_serviceDiscovery_data();
    void tst_serviceDiscovery();
    void tst_serviceDiscoveryAdapters();

private:
    QList<QBluetoothDeviceInfo> devices;
    bool localDeviceAvailable;
};

tst_QBluetoothServiceDiscoveryAgent::tst_QBluetoothServiceDiscoveryAgent()
{
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));

    // start Bluetooth if not started
#ifndef Q_OS_OSX
    QBluetoothLocalDevice *device = new QBluetoothLocalDevice();
    localDeviceAvailable = device->isValid();
    if (localDeviceAvailable) {
        device->powerOn();
        // wait for the device to switch bluetooth mode.
        QTest::qWait(1000);
    }
    delete device;
#else
    QBluetoothLocalDevice device;
    localDeviceAvailable = QBluetoothLocalDevice().hostMode() != QBluetoothLocalDevice::HostPoweredOff;
#endif

    qRegisterMetaType<QBluetoothDeviceInfo>();
    qRegisterMetaType<QBluetoothServiceInfo>();
    qRegisterMetaType<QList<QBluetoothUuid> >();
    qRegisterMetaType<QBluetoothServiceDiscoveryAgent::Error>();
    qRegisterMetaType<QBluetoothDeviceDiscoveryAgent::Error>();

}

tst_QBluetoothServiceDiscoveryAgent::~tst_QBluetoothServiceDiscoveryAgent()
{
}

void tst_QBluetoothServiceDiscoveryAgent::deviceDiscoveryDebug(const QBluetoothDeviceInfo &info)
{
    qDebug() << "Discovered device:" << info.address().toString() << info.name();
}


void tst_QBluetoothServiceDiscoveryAgent::serviceError(const QBluetoothServiceDiscoveryAgent::Error err)
{
    qDebug() << "Service discovery error" << err;
}

void tst_QBluetoothServiceDiscoveryAgent::initTestCase()
{
    if (localDeviceAvailable) {
        QBluetoothDeviceDiscoveryAgent discoveryAgent;

        QSignalSpy finishedSpy(&discoveryAgent, SIGNAL(finished()));
        QSignalSpy errorSpy(&discoveryAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)));
        QSignalSpy discoveredSpy(&discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)));
    //    connect(&discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
    //            this, SLOT(deviceDiscoveryDebug(QBluetoothDeviceInfo)));

        discoveryAgent.start();

        // Wait for up to MaxScanTime for the scan to finish
        int scanTime = MaxScanTime;
        while (finishedSpy.count() == 0 && scanTime > 0) {
            QTest::qWait(1000);
            scanTime -= 1000;
        }
    //    qDebug() << "Scan time left:" << scanTime;

        // Expect finished signal with no error
        QVERIFY(finishedSpy.count() == 1);
        QVERIFY(errorSpy.isEmpty());

        devices = discoveryAgent.discoveredDevices();
    }
}

void tst_QBluetoothServiceDiscoveryAgent::tst_invalidBtAddress()
{
#ifdef Q_OS_OSX
    if (!localDeviceAvailable)
        QSKIP("On OS X this test requires Bluetooth adapter in powered ON state");
#endif
    QBluetoothServiceDiscoveryAgent *discoveryAgent = new QBluetoothServiceDiscoveryAgent(QBluetoothAddress("11:11:11:11:11:11"));

    QCOMPARE(discoveryAgent->error(), QBluetoothServiceDiscoveryAgent::InvalidBluetoothAdapterError);
    discoveryAgent->start();
    QCOMPARE(discoveryAgent->isActive(), false);
    delete discoveryAgent;

    discoveryAgent = new QBluetoothServiceDiscoveryAgent(QBluetoothAddress());
    QCOMPARE(discoveryAgent->error(), QBluetoothServiceDiscoveryAgent::NoError);
    if (QBluetoothLocalDevice::allDevices().count() > 0) {
        discoveryAgent->start();
        QCOMPARE(discoveryAgent->isActive(), true);
    }
    delete discoveryAgent;
}

void tst_QBluetoothServiceDiscoveryAgent::serviceDiscoveryDebug(const QBluetoothServiceInfo &info)
{
    qDebug() << "Discovered service on"
             << info.device().name() << info.device().address().toString();
    qDebug() << "\tService name:" << info.serviceName() << "cached" << info.device().isCached();
    qDebug() << "\tDescription:"
             << info.attribute(QBluetoothServiceInfo::ServiceDescription).toString();
    qDebug() << "\tProvider:" << info.attribute(QBluetoothServiceInfo::ServiceProvider).toString();
    qDebug() << "\tL2CAP protocol service multiplexer:" << info.protocolServiceMultiplexer();
    qDebug() << "\tRFCOMM server channel:" << info.serverChannel();
}

static void dumpAttributeVariant(const QVariant &var, const QString indent)
{
    if (!var.isValid()) {
        qDebug("%sEmpty", indent.toLocal8Bit().constData());
        return;
    }

    if (var.userType() == qMetaTypeId<QBluetoothServiceInfo::Sequence>()) {
        qDebug("%sSequence", indent.toLocal8Bit().constData());
        const QBluetoothServiceInfo::Sequence *sequence = static_cast<const QBluetoothServiceInfo::Sequence *>(var.data());
        foreach (const QVariant &v, *sequence)
            dumpAttributeVariant(v, indent + '\t');
    } else if (var.userType() == qMetaTypeId<QBluetoothServiceInfo::Alternative>()) {
        qDebug("%sAlternative", indent.toLocal8Bit().constData());
        const QBluetoothServiceInfo::Alternative *alternative = static_cast<const QBluetoothServiceInfo::Alternative *>(var.data());
        foreach (const QVariant &v, *alternative)
            dumpAttributeVariant(v, indent + '\t');
    } else if (var.userType() == qMetaTypeId<QBluetoothUuid>()) {
        QBluetoothUuid uuid = var.value<QBluetoothUuid>();
        switch (uuid.minimumSize()) {
        case 0:
            qDebug("%suuid NULL", indent.toLocal8Bit().constData());
            break;
        case 2:
            qDebug("%suuid %04x", indent.toLocal8Bit().constData(), uuid.toUInt16());
            break;
        case 4:
            qDebug("%suuid %08x", indent.toLocal8Bit().constData(), uuid.toUInt32());
            break;
        case 16: {
            qDebug("%suuid %s", indent.toLocal8Bit().constData(), QByteArray(reinterpret_cast<const char *>(uuid.toUInt128().data), 16).toHex().constData());
            break;
        }
        default:
            qDebug("%suuid ???", indent.toLocal8Bit().constData());
        }
    } else {
        switch (var.userType()) {
        case QVariant::UInt:
            qDebug("%suint %u", indent.toLocal8Bit().constData(), var.toUInt());
            break;
        case QVariant::Int:
            qDebug("%sint %d", indent.toLocal8Bit().constData(), var.toInt());
            break;
        case QVariant::String:
            qDebug("%sstring %s", indent.toLocal8Bit().constData(), var.toString().toLocal8Bit().constData());
            break;
        case QVariant::Bool:
            qDebug("%sbool %d", indent.toLocal8Bit().constData(), var.toBool());
            break;
        case QVariant::Url:
            qDebug("%surl %s", indent.toLocal8Bit().constData(), var.toUrl().toString().toLocal8Bit().constData());
            break;
        default:
            qDebug("%sunknown", indent.toLocal8Bit().constData());
        }
    }
}

static inline void dumpServiceInfoAttributes(const QBluetoothServiceInfo &info)
{
    foreach (quint16 id, info.attributes()) {
        dumpAttributeVariant(info.attribute(id), QString("\t"));
    }
}


void tst_QBluetoothServiceDiscoveryAgent::tst_serviceDiscovery_data()
{
    if (devices.isEmpty())
        QSKIP("This test requires an in-range bluetooth device");

    QTest::addColumn<QBluetoothDeviceInfo>("deviceInfo");
    QTest::addColumn<QList<QBluetoothUuid> >("uuidFilter");
    QTest::addColumn<QBluetoothServiceDiscoveryAgent::Error>("serviceDiscoveryError");

    // Only need to test the first 5 live devices
    int max = 5;
    foreach (const QBluetoothDeviceInfo &info, devices) {
        if (info.isCached())
            continue;
        QTest::newRow("default filter") << info << QList<QBluetoothUuid>()
            << QBluetoothServiceDiscoveryAgent::NoError;
        if (!--max)
            break;
        //QTest::newRow("public browse group") << info << (QList<QBluetoothUuid>() << QBluetoothUuid::PublicBrowseGroup);
        //QTest::newRow("l2cap") << info << (QList<QBluetoothUuid>() << QBluetoothUuid::L2cap);
    }
    QTest::newRow("all devices") << QBluetoothDeviceInfo() << QList<QBluetoothUuid>()
        << QBluetoothServiceDiscoveryAgent::NoError;
}

void tst_QBluetoothServiceDiscoveryAgent::tst_serviceDiscoveryAdapters()
{
    QBluetoothLocalDevice localDevice;
    int numberOfAdapters = (localDevice.allDevices()).size();
    if (numberOfAdapters>1) {
        if (devices.isEmpty())
            QSKIP("This test requires an in-range bluetooth device");

        QList<QBluetoothAddress> addresses;

        for (int i=0; i<numberOfAdapters; i++) {
            addresses.append(((QBluetoothHostInfo)localDevice.allDevices().at(i)).address());
        }
        QBluetoothServer server(QBluetoothServiceInfo::RfcommProtocol);
        QBluetoothUuid uuid(QBluetoothUuid::Ftp);
        server.listen(addresses[0]);
        QBluetoothServiceInfo serviceInfo;
        serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, "serviceName");
        serviceInfo.setAttribute(QBluetoothServiceInfo::BrowseGroupList,
                                 QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));

        QBluetoothServiceInfo::Sequence classId;
        classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
        serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);
        serviceInfo.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList,
                                 classId);

        serviceInfo.setServiceUuid(uuid);

        QBluetoothServiceInfo::Sequence protocolDescriptorList;
        QBluetoothServiceInfo::Sequence protocol;
        protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
        protocolDescriptorList.append(QVariant::fromValue(protocol));
        protocol.clear();

        protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
                 << QVariant::fromValue(quint8(server.serverPort()));

        protocolDescriptorList.append(QVariant::fromValue(protocol));
        serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,
                                 protocolDescriptorList);
        QVERIFY(serviceInfo.registerService());

        QVERIFY(server.isListening());
        qDebug() << "Scanning address " << addresses[0].toString();
        QBluetoothServiceDiscoveryAgent discoveryAgent(addresses[1]);
        bool setAddress = discoveryAgent.setRemoteAddress(addresses[0]);

        QVERIFY(setAddress);

        QVERIFY(!discoveryAgent.isActive());

        QVERIFY(discoveryAgent.discoveredServices().isEmpty());

        QSignalSpy finishedSpy(&discoveryAgent, SIGNAL(finished()));

        discoveryAgent.start();
        int scanTime = MaxScanTime;
        while (finishedSpy.count() == 0 && scanTime > 0) {
            QTest::qWait(1000);
            scanTime -= 1000;
        }

        QList<QBluetoothServiceInfo> discServices = discoveryAgent.discoveredServices();
        QVERIFY(!discServices.empty());

        int counter = 0;
        for (int i = 0; i<discServices.size(); i++)
        {
            QBluetoothServiceInfo service((QBluetoothServiceInfo)discServices.at(i));
            if (uuid == service.serviceUuid())
                counter++;
        }
        QVERIFY(counter == 1);
    }

}


void tst_QBluetoothServiceDiscoveryAgent::tst_serviceDiscovery()
{
    // Not all devices respond to SDP, so allow for a failure
    static int expected_failures = 0;

    if (devices.isEmpty())
        QSKIP("This test requires an in-range bluetooth device");

    QFETCH(QBluetoothDeviceInfo, deviceInfo);
    QFETCH(QList<QBluetoothUuid>, uuidFilter);
    QFETCH(QBluetoothServiceDiscoveryAgent::Error, serviceDiscoveryError);

    QBluetoothLocalDevice localDevice;
    qDebug() << "Scanning address" << deviceInfo.address().toString();
    QBluetoothServiceDiscoveryAgent discoveryAgent(localDevice.address());
    bool setAddress = discoveryAgent.setRemoteAddress(deviceInfo.address());

    QVERIFY(setAddress);

    QVERIFY(!discoveryAgent.isActive());

    QVERIFY(discoveryAgent.discoveredServices().isEmpty());

    QVERIFY(discoveryAgent.uuidFilter().isEmpty());

    discoveryAgent.setUuidFilter(uuidFilter);

    QVERIFY(discoveryAgent.uuidFilter() == uuidFilter);

    QSignalSpy finishedSpy(&discoveryAgent, SIGNAL(finished()));
    QSignalSpy errorSpy(&discoveryAgent, SIGNAL(error(QBluetoothServiceDiscoveryAgent::Error)));
    QSignalSpy discoveredSpy(&discoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)));
//    connect(&discoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
//            this, SLOT(serviceDiscoveryDebug(QBluetoothServiceInfo)));
    connect(&discoveryAgent, SIGNAL(error(QBluetoothServiceDiscoveryAgent::Error)),
            this, SLOT(serviceError(QBluetoothServiceDiscoveryAgent::Error)));

    discoveryAgent.start(QBluetoothServiceDiscoveryAgent::FullDiscovery);

    /*
     * Either we wait for discovery agent to run its course (e.g. Bluez 4) or
     * we have an immediate result (e.g. Bluez 5)
     */
    QVERIFY(discoveryAgent.isActive() || !finishedSpy.isEmpty());

    // Wait for up to MaxScanTime for the scan to finish
    int scanTime = MaxScanTime;
    while (finishedSpy.count() == 0 && scanTime > 0) {
        QTest::qWait(1000);
        scanTime -= 1000;
    }

    if (discoveryAgent.error() && expected_failures++ < 2){
        qDebug() << "Device failed to respond to SDP, skipping device" << discoveryAgent.error() << discoveryAgent.errorString();
        return;
    }

    QVERIFY(discoveryAgent.error() == serviceDiscoveryError);
    QVERIFY(discoveryAgent.errorString() == QString());

    // Expect finished signal with no error
    QVERIFY(finishedSpy.count() == 1);
    QVERIFY(errorSpy.isEmpty());

    //if (discoveryAgent.discoveredServices().count() && expected_failures++ <2){
    if (discoveredSpy.isEmpty() && expected_failures++ < 2){
        qDebug() << "Device failed to return any results, skipping device" << discoveryAgent.discoveredServices().count();
        return;
    }

    // All returned QBluetoothServiceInfo should be valid.
    bool servicesFound = !discoveredSpy.isEmpty();
    while (!discoveredSpy.isEmpty()) {
        const QVariant v = discoveredSpy.takeFirst().at(0);
        // Work around limitation in QMetaType and moc.
        // QBluetoothServiceInfo is registered with metatype as QBluetoothServiceInfo
        // moc sees it as the unqualified QBluetoothServiceInfo.
        if (v.userType() == qMetaTypeId<QBluetoothServiceInfo>())
        {
            const QBluetoothServiceInfo info =
                *reinterpret_cast<const QBluetoothServiceInfo*>(v.constData());

            QVERIFY(info.isValid());
            QVERIFY(!info.isRegistered());

#if 0
            qDebug() << info.device().name() << info.device().address().toString();
            qDebug() << "\tService name:" << info.serviceName();
            if (info.protocolServiceMultiplexer() >= 0)
                qDebug() << "\tL2CAP protocol service multiplexer:" << info.protocolServiceMultiplexer();
            if (info.serverChannel() >= 0)
                qDebug() << "\tRFCOMM server channel:" << info.serverChannel();
            //dumpServiceInfoAttributes(info);
#endif
        } else {
            QFAIL("Unknown type returned by service discovery");
        }

    }

    if (servicesFound)
        QVERIFY(discoveryAgent.discoveredServices().count() != 0);
    discoveryAgent.clear();
    QVERIFY(discoveryAgent.discoveredServices().count() == 0);

    discoveryAgent.stop();
    QVERIFY(!discoveryAgent.isActive());
}

QTEST_MAIN(tst_QBluetoothServiceDiscoveryAgent)

#include "tst_qbluetoothservicediscoveryagent.moc"
