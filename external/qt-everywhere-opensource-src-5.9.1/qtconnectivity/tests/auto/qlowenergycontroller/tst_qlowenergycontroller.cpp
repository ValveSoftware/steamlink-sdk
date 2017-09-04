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
#include <QBluetoothAddress>
#include <QBluetoothLocalDevice>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothUuid>
#include <QLowEnergyController>
#include <QLowEnergyCharacteristic>

#include <QDebug>

/*!
  This test requires a TI sensor tag with Firmware version: 1.5 (Oct 23 2013).
  Since revision updates change user strings and even shift handles around
  other versions than the above are unlikely to succeed. Please update the
  sensor tag before continuing.

  The TI sensor can be updated using the related iOS app. The Android version
  doesn't seem to update at this point in time.
  */

QT_USE_NAMESPACE

// This define must be set if the platform provides access to GATT handles
// otherwise it must not be defined. As of now the two supported platforms
// (Android and Bluez/Linux) provide access or some notion of it.
#ifndef Q_OS_MAC
#define HANDLES_PROVIDED_BY_PLATFORM
#endif

#ifdef HANDLES_PROVIDED_BY_PLATFORM
#define HANDLE_COMPARE(actual,expected) QCOMPARE(actual,expected)
#else
#define HANDLE_COMPARE(actual,expected)
#endif

class tst_QLowEnergyController : public QObject
{
    Q_OBJECT

public:
    tst_QLowEnergyController();
    ~tst_QLowEnergyController();

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();
    void tst_emptyCtor();
    void tst_connect();
    void tst_concurrentDiscovery();
    void tst_defaultBehavior();
    void tst_writeCharacteristic();
    void tst_writeCharacteristicNoResponse();
    void tst_readWriteDescriptor();
    void tst_customProgrammableDevice();
    void tst_errorCases();
private:
    void verifyServiceProperties(const QLowEnergyService *info);

    QBluetoothDeviceDiscoveryAgent *devAgent;
    QBluetoothAddress remoteDevice;
    QBluetoothDeviceInfo remoteDeviceInfo;
    QList<QBluetoothUuid> foundServices;
};

Q_DECLARE_METATYPE(QLowEnergyCharacteristic)
Q_DECLARE_METATYPE(QLowEnergyDescriptor)

tst_QLowEnergyController::tst_QLowEnergyController()
{
    qRegisterMetaType<QLowEnergyCharacteristic>();
    qRegisterMetaType<QLowEnergyDescriptor>();

    //QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
#ifndef Q_OS_MAC
    // Core Bluetooth (OS X and iOS) does not work with addresses,
    // making the code below useless.
    const QString remote = qgetenv("BT_TEST_DEVICE");
    if (!remote.isEmpty()) {
        remoteDevice = QBluetoothAddress(remote);
        qWarning() << "Using remote device " << remote << " for testing. Ensure that the device is discoverable for pairing requests";
    } else {
        qWarning() << "Not using any remote device for testing. Set BT_TEST_DEVICE env to run manual tests involving a remote device";
    }
#endif
}

tst_QLowEnergyController::~tst_QLowEnergyController()
{

}

void tst_QLowEnergyController::initTestCase()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_WINRT)
    if (remoteDevice.isNull()
        || QBluetoothLocalDevice::allDevices().isEmpty()) {
        qWarning("No remote device or local adapter found.");
        return;
    }
#elif defined(Q_OS_OSX)
    // allDevices is always empty on iOS:
    if (QBluetoothLocalDevice::allDevices().isEmpty()) {
        qWarning("No local adapter found.");
        return;
    }
#endif

    devAgent = new QBluetoothDeviceDiscoveryAgent(this);

    QSignalSpy finishedSpy(devAgent, SIGNAL(finished()));
    // there should be no changes yet
    QVERIFY(finishedSpy.isValid());
    QVERIFY(finishedSpy.isEmpty());

    bool deviceFound = false;
    devAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.count() > 0, 30000);
    foreach (const QBluetoothDeviceInfo &info, devAgent->discoveredDevices()) {
#ifndef Q_OS_MAC
        if (info.address() == remoteDevice) {
#else
        // On OS X/iOS the only way to find the device we are
        // interested in - is to use device's name.
        if (info.name().contains("Sensor") && info.name().contains("Tag")) {
#endif
            remoteDeviceInfo = info;
            deviceFound = true;
            break;
        }
    }

    QVERIFY2(deviceFound, "Cannot find remote device.");

    // These are the services exported by the TI SensorTag
#ifndef Q_OS_MAC
    // Core Bluetooth somehow ignores/hides/fails to discover these services.
    foundServices << QBluetoothUuid(QString("00001800-0000-1000-8000-00805f9b34fb"));
    foundServices << QBluetoothUuid(QString("00001801-0000-1000-8000-00805f9b34fb"));
#endif
    foundServices << QBluetoothUuid(QString("0000180a-0000-1000-8000-00805f9b34fb"));
    foundServices << QBluetoothUuid(QString("0000ffe0-0000-1000-8000-00805f9b34fb"));
    foundServices << QBluetoothUuid(QString("f000aa00-0451-4000-b000-000000000000"));
    foundServices << QBluetoothUuid(QString("f000aa10-0451-4000-b000-000000000000"));
    foundServices << QBluetoothUuid(QString("f000aa20-0451-4000-b000-000000000000"));
    foundServices << QBluetoothUuid(QString("f000aa30-0451-4000-b000-000000000000"));
    foundServices << QBluetoothUuid(QString("f000aa40-0451-4000-b000-000000000000"));
    foundServices << QBluetoothUuid(QString("f000aa50-0451-4000-b000-000000000000"));
    foundServices << QBluetoothUuid(QString("f000aa60-0451-4000-b000-000000000000"));
    foundServices << QBluetoothUuid(QString("f000ccc0-0451-4000-b000-000000000000"));
    foundServices << QBluetoothUuid(QString("f000ffc0-0451-4000-b000-000000000000"));
}

/*
 * Executed in between each test function call.
 */
void tst_QLowEnergyController::init()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(Q_OS_TVOS)
    /*
     * Add a delay to give Android/iOS stack time to catch up in between
     * the multiple connect/disconnects within each test function.
     */
    QTest::qWait(2000);
#endif
}

void tst_QLowEnergyController::cleanupTestCase()
{

}

void tst_QLowEnergyController::tst_emptyCtor()
{
    {
        QBluetoothAddress remoteAddress;
        QLowEnergyController control(remoteAddress);
        QSignalSpy connectedSpy(&control, SIGNAL(connected()));
        QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
        QSignalSpy errorSpy(&control, SIGNAL(error(QLowEnergyController::Error)));
        QCOMPARE(control.error(), QLowEnergyController::NoError);
        control.connectToDevice();

        QTRY_VERIFY_WITH_TIMEOUT(!errorSpy.isEmpty(), 10000);

        QVERIFY(connectedSpy.isEmpty());
        QVERIFY(stateSpy.isEmpty());

        QLowEnergyController::Error lastError = errorSpy[0].at(0).value<QLowEnergyController::Error>();
        QVERIFY(lastError == QLowEnergyController::UnknownRemoteDeviceError
                || lastError == QLowEnergyController::InvalidBluetoothAdapterError);
    }

    {
        QBluetoothDeviceInfo deviceInfo;
        QLowEnergyController control(deviceInfo);
        QSignalSpy connectedSpy(&control, SIGNAL(connected()));
        QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
        QSignalSpy errorSpy(&control, SIGNAL(error(QLowEnergyController::Error)));
        QCOMPARE(control.error(), QLowEnergyController::NoError);
        control.connectToDevice();

        QTRY_VERIFY_WITH_TIMEOUT(!errorSpy.isEmpty(), 10000);

        QVERIFY(connectedSpy.isEmpty());
        QVERIFY(stateSpy.isEmpty());

        QLowEnergyController::Error lastError = errorSpy[0].at(0).value<QLowEnergyController::Error>();
        QVERIFY(lastError == QLowEnergyController::UnknownRemoteDeviceError  // if local device on platform found
                || lastError == QLowEnergyController::InvalidBluetoothAdapterError); // otherwise, e.g. fallback backend
    }

}

void tst_QLowEnergyController::tst_connect()
{
    QList<QBluetoothHostInfo> localAdapters = QBluetoothLocalDevice::allDevices();

#if defined(Q_OS_IOS) || defined(Q_OS_TVOS) || defined(Q_OS_WINRT)
    if (!remoteDeviceInfo.isValid())
#else
    if (localAdapters.isEmpty() || !remoteDeviceInfo.isValid())
#endif
        QSKIP("No local Bluetooth or remote BTLE device found. Skipping test.");

    QLowEnergyController control(remoteDeviceInfo);
    QCOMPARE(remoteDeviceInfo.deviceUuid(), control.remoteDeviceUuid());
    QCOMPARE(control.role(), QLowEnergyController::CentralRole);
    QSignalSpy connectedSpy(&control, SIGNAL(connected()));
    QSignalSpy disconnectedSpy(&control, SIGNAL(disconnected()));
    if (remoteDeviceInfo.name().isEmpty())
        QVERIFY(control.remoteName().isEmpty());
    else
        QCOMPARE(control.remoteName(), remoteDeviceInfo.name());

#if !defined(Q_OS_IOS) && !defined(Q_OS_TVOS) && !defined(Q_OS_WINRT)
    const QBluetoothAddress localAdapter = localAdapters.at(0).address();
    QCOMPARE(control.localAddress(), localAdapter);
    QVERIFY(!control.localAddress().isNull());
#endif
#ifndef Q_OS_MAC
    QCOMPARE(control.remoteAddress(), remoteDevice);
#endif
    QCOMPARE(control.state(), QLowEnergyController::UnconnectedState);
    QCOMPARE(control.error(), QLowEnergyController::NoError);
    QVERIFY(control.errorString().isEmpty());
    QCOMPARE(disconnectedSpy.count(), 0);
    QCOMPARE(connectedSpy.count(), 0);
    QVERIFY(control.services().isEmpty());

    bool wasError = false;
    control.connectToDevice();
    QTRY_IMPL(control.state() != QLowEnergyController::ConnectingState,
              10000);

    QCOMPARE(disconnectedSpy.count(), 0);
    if (control.error() != QLowEnergyController::NoError) {
        //error during connect
        QCOMPARE(connectedSpy.count(), 0);
        QCOMPARE(control.state(), QLowEnergyController::UnconnectedState);
        wasError = true;
    } else if (control.state() == QLowEnergyController::ConnectingState) {
        //timeout
        QCOMPARE(connectedSpy.count(), 0);
        QVERIFY(control.errorString().isEmpty());
        QCOMPARE(control.error(), QLowEnergyController::NoError);
        QVERIFY(control.services().isEmpty());
        QSKIP("Connection to LE device cannot be established. Skipping test.");
        return;
    } else {
        QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
        QCOMPARE(connectedSpy.count(), 1);
        QCOMPARE(control.error(), QLowEnergyController::NoError);
        QVERIFY(control.errorString().isEmpty());
    }

    QVERIFY(control.services().isEmpty());

    QList<QLowEnergyService *> savedReferences;

    if (!wasError) {
        QSignalSpy discoveryFinishedSpy(&control, SIGNAL(discoveryFinished()));
        QSignalSpy serviceFoundSpy(&control, SIGNAL(serviceDiscovered(QBluetoothUuid)));
        QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
        control.discoverServices();
        QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 20000);
        QCOMPARE(stateSpy.count(), 2);
        QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
                 QLowEnergyController::DiscoveringState);
        QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
                 QLowEnergyController::DiscoveredState);

        QVERIFY(!serviceFoundSpy.isEmpty());
        QVERIFY(serviceFoundSpy.count() >= foundServices.count());
        QVERIFY(!serviceFoundSpy.isEmpty());
        QList<QBluetoothUuid> listing;
        for (int i = 0; i < serviceFoundSpy.count(); i++) {
            const QVariant v = serviceFoundSpy[i].at(0);
            listing.append(v.value<QBluetoothUuid>());
        }

        foreach (const QBluetoothUuid &uuid, foundServices) {
            QVERIFY2(listing.contains(uuid),
                     uuid.toString().toLatin1());

            QLowEnergyService *service = control.createServiceObject(uuid);
            QVERIFY2(service, uuid.toString().toLatin1());
            savedReferences.append(service);
            QCOMPARE(service->type(), QLowEnergyService::PrimaryService);
            QCOMPARE(service->state(), QLowEnergyService::DiscoveryRequired);
        }

        // unrelated uuids don't return valid service object
        // invalid service uuid
        QVERIFY(!control.createServiceObject(QBluetoothUuid()));
        // some random uuid
        QVERIFY(!control.createServiceObject(QBluetoothUuid(QBluetoothUuid::DeviceName)));

        // initiate characteristic discovery
        foreach (QLowEnergyService *service, savedReferences) {
            qDebug() << "Discovering" << service->serviceUuid();
            QSignalSpy stateSpy(service,
                                SIGNAL(stateChanged(QLowEnergyService::ServiceState)));
            QSignalSpy errorSpy(service, SIGNAL(error(QLowEnergyService::ServiceError)));
            service->discoverDetails();

            QTRY_VERIFY_WITH_TIMEOUT(
                        service->state() == QLowEnergyService::ServiceDiscovered, 10000);

            QCOMPARE(errorSpy.count(), 0); //no error
            QCOMPARE(stateSpy.count(), 2); //

            verifyServiceProperties(service);
        }

        // ensure that related service objects share same state
        foreach (QLowEnergyService* originalService, savedReferences) {
            QLowEnergyService *newService = control.createServiceObject(
                        originalService->serviceUuid());
            QVERIFY(newService);
            QCOMPARE(newService->state(), QLowEnergyService::ServiceDiscovered);
            delete newService;
        }
    }

    // Finish off
    control.disconnectFromDevice();
    QTRY_VERIFY_WITH_TIMEOUT(
                control.state() == QLowEnergyController::UnconnectedState,
                10000);

    if (wasError) {
        QCOMPARE(disconnectedSpy.count(), 0);
    } else {
        QCOMPARE(disconnectedSpy.count(), 1);
        // after disconnect all service references must be invalid
        foreach (const QLowEnergyService *entry, savedReferences) {
            const QBluetoothUuid &uuid = entry->serviceUuid();
            QVERIFY2(entry->state() == QLowEnergyService::InvalidService,
                     uuid.toString().toLatin1());

            //after disconnect all related characteristics and descriptors are invalid
            QList<QLowEnergyCharacteristic> chars = entry->characteristics();
            for (int i = 0; i < chars.count(); i++) {
                QCOMPARE(chars.at(i).isValid(), false);
                QList<QLowEnergyDescriptor> descriptors = chars[i].descriptors();
                for (int j = 0; j < descriptors.count(); j++)
                    QCOMPARE(descriptors[j].isValid(), false);
            }
        }
    }

    qDeleteAll(savedReferences);
    savedReferences.clear();
}

void tst_QLowEnergyController::tst_concurrentDiscovery()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_WINRT)
    QList<QBluetoothHostInfo> localAdapters = QBluetoothLocalDevice::allDevices();
    if (localAdapters.isEmpty())
        QSKIP("No local Bluetooth device found. Skipping test.");
#endif

    if (!remoteDeviceInfo.isValid())
        QSKIP("No remote BTLE device found. Skipping test.");
    QLowEnergyController control(remoteDeviceInfo);


    QCOMPARE(control.state(), QLowEnergyController::UnconnectedState);
    QCOMPARE(control.error(), QLowEnergyController::NoError);

    control.connectToDevice();
    {
        QTRY_IMPL(control.state() != QLowEnergyController::ConnectingState,
              30000);
    }

    if (control.state() == QLowEnergyController::ConnectingState
            || control.error() != QLowEnergyController::NoError) {
        // default BTLE backend forever hangs in ConnectingState
        QSKIP("Cannot connect to remote device");
    }

    QCOMPARE(control.state(), QLowEnergyController::ConnectedState);

    // 2. new controller to same device fails
    {
        QLowEnergyController control2(remoteDevice);
        control2.connectToDevice();
        {
            QTRY_IMPL(control2.state() != QLowEnergyController::ConnectingState,
                      30000);
        }

#if defined(Q_OS_ANDROID) || defined(Q_OS_WINRT)
        QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
        QCOMPARE(control2.state(), QLowEnergyController::ConnectedState);
        control2.disconnectFromDevice();
        QTest::qWait(3000);
        QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
        QCOMPARE(control2.state(), QLowEnergyController::UnconnectedState);
#else
        // see QTBUG-42519
        // Linux cannot maintain two controller connections at the same time
        QVERIFY(control2.error() != QLowEnergyController::NoError);
#endif
    }

    /* We are testing that we can run service discovery on the same device
     * for multiple services at the same time.
     * */

    QSignalSpy discoveryFinishedSpy(&control, SIGNAL(discoveryFinished()));
    QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
    control.discoverServices();
    QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 20000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveringState);
    QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveredState);

    // pick MAX_SERVICES_SAME_TIME_ACCESS services
    // and discover them at the same time
#define MAX_SERVICES_SAME_TIME_ACCESS 3
    QLowEnergyService *services[MAX_SERVICES_SAME_TIME_ACCESS];

    QVERIFY(control.services().count() >= MAX_SERVICES_SAME_TIME_ACCESS);

    QList<QBluetoothUuid> uuids = control.services();

    // initialize services
    for (int i = 0; i<MAX_SERVICES_SAME_TIME_ACCESS; i++) {
        services[i] = control.createServiceObject(uuids.at(i), this);
        QVERIFY(services[i]);
    }

    // start complete discovery
    for (int i = 0; i<MAX_SERVICES_SAME_TIME_ACCESS; i++)
        services[i]->discoverDetails();

    // wait until discovery done
    for (int i = 0; i<MAX_SERVICES_SAME_TIME_ACCESS; i++) {
        qWarning() << "Waiting for" << i << services[i]->serviceUuid();
        QTRY_VERIFY_WITH_TIMEOUT(
            services[i]->state() == QLowEnergyService::ServiceDiscovered,
            30000);
    }

    // verify discovered services
    for (int i = 0; i<MAX_SERVICES_SAME_TIME_ACCESS; i++) {
        verifyServiceProperties(services[i]);

        QVERIFY(!services[i]->contains(QLowEnergyCharacteristic()));
        QVERIFY(!services[i]->contains(QLowEnergyDescriptor()));
    }

    control.disconnectFromDevice();
    QTRY_VERIFY_WITH_TIMEOUT(control.state() == QLowEnergyController::UnconnectedState,
                             30000);
    discoveryFinishedSpy.clear();

    // redo the discovery with same controller
    QLowEnergyService *services_second[MAX_SERVICES_SAME_TIME_ACCESS];
    control.connectToDevice();
    {
        QTRY_IMPL(control.state() != QLowEnergyController::ConnectingState,
              30000);
    }

    QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
    stateSpy.clear();
    control.discoverServices();
    QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 20000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveringState);
    QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveredState);

    // get all details
    for (int i = 0; i<MAX_SERVICES_SAME_TIME_ACCESS; i++) {
        services_second[i] = control.createServiceObject(uuids.at(i), this);
        QVERIFY(services_second[i]->parent() == this);
        QVERIFY(services[i]);
        QVERIFY(services_second[i]->state() == QLowEnergyService::DiscoveryRequired);
        services_second[i]->discoverDetails();
    }

    // wait until discovery done
    for (int i = 0; i<MAX_SERVICES_SAME_TIME_ACCESS; i++) {
        qWarning() << "Waiting for" << i << services_second[i]->serviceUuid();
        QTRY_VERIFY_WITH_TIMEOUT(
            services_second[i]->state() == QLowEnergyService::ServiceDiscovered,
            30000);
        QCOMPARE(services_second[i]->serviceName(), services[i]->serviceName());
        QCOMPARE(services_second[i]->serviceUuid(), services[i]->serviceUuid());
    }

    // verify discovered services (1st and 2nd round)
    for (int i = 0; i<MAX_SERVICES_SAME_TIME_ACCESS; i++) {
        verifyServiceProperties(services_second[i]);
        //after disconnect all related characteristics and descriptors are invalid
        const QList<QLowEnergyCharacteristic> chars = services[i]->characteristics();
        for (int j = 0; j < chars.count(); j++) {
            QCOMPARE(chars.at(j).isValid(), false);
            QVERIFY(services[i]->contains(chars[j]));
            QVERIFY(!services_second[i]->contains(chars[j]));
            const QList<QLowEnergyDescriptor> descriptors = chars[j].descriptors();
            for (int k = 0; k < descriptors.count(); k++) {
                QCOMPARE(descriptors[k].isValid(), false);
                services[i]->contains(descriptors[k]);
                QVERIFY(!services_second[i]->contains(chars[j]));
            }
        }

        QCOMPARE(services[i]->serviceUuid(), services_second[i]->serviceUuid());
        QCOMPARE(services[i]->serviceName(), services_second[i]->serviceName());
        QCOMPARE(services[i]->type(), services_second[i]->type());
        QVERIFY(services[i]->state() == QLowEnergyService::InvalidService);
        QVERIFY(services_second[i]->state() == QLowEnergyService::ServiceDiscovered);
    }

    // cleanup
    for (int i = 0; i<MAX_SERVICES_SAME_TIME_ACCESS; i++) {
        delete services[i];
        delete services_second[i];
    }

    control.disconnectFromDevice();
}

void tst_QLowEnergyController::verifyServiceProperties(
        const QLowEnergyService *info)
{
    if (info->serviceUuid() ==
            QBluetoothUuid(QString("00001800-0000-1000-8000-00805f9b34fb"))) {
        qDebug() << "Verifying GAP Service";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 5);

        // Device Name
        QString temp("00002a00-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x3));
        QCOMPARE(chars[0].properties(), QLowEnergyCharacteristic::Read);
        QCOMPARE(chars[0].value(), QByteArray::fromHex("544920424c452053656e736f7220546167"));
        QVERIFY(chars[0].isValid());
        QCOMPARE(chars[0].descriptors().count(), 0);
        QVERIFY(info->contains(chars[0]));

        // Appearance
        temp = QString("00002a01-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x5));
        QCOMPARE(chars[1].properties(), QLowEnergyCharacteristic::Read);
        QCOMPARE(chars[1].value(), QByteArray::fromHex("0000"));
        QVERIFY(chars[1].isValid());
        QCOMPARE(chars[1].descriptors().count(), 0);
        QVERIFY(info->contains(chars[1]));

        // Peripheral Privacy Flag
        temp = QString("00002a02-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[2].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[2].handle(), QLowEnergyHandle(0x7));
        QVERIFY(chars[2].properties() & QLowEnergyCharacteristic::Read);
        QCOMPARE(chars[2].value(), QByteArray::fromHex("00"));
        QVERIFY(chars[2].isValid());
        QCOMPARE(chars[2].descriptors().count(), 0);
        QVERIFY(info->contains(chars[2]));

        // Reconnection Address
        temp = QString("00002a03-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[3].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[3].handle(), QLowEnergyHandle(0x9));
        //Early firmware version had this characteristic as Read|Write and may fail
        QCOMPARE(chars[3].properties(), QLowEnergyCharacteristic::Write);
        if (chars[3].properties() & QLowEnergyCharacteristic::Read)
            QCOMPARE(chars[3].value(), QByteArray::fromHex("000000000000"));
        else
            QCOMPARE(chars[3].value(), QByteArray());
        QVERIFY(chars[3].isValid());
        QCOMPARE(chars[3].descriptors().count(), 0);
        QVERIFY(info->contains(chars[3]));

        // Peripheral Preferred Connection Parameters
        temp = QString("00002a04-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[4].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[4].handle(), QLowEnergyHandle(0xb));
        QCOMPARE(chars[4].properties(), QLowEnergyCharacteristic::Read);
        QCOMPARE(chars[4].value(), QByteArray::fromHex("5000a0000000e803"));
        QVERIFY(chars[4].isValid());
        QCOMPARE(chars[4].descriptors().count(), 0);
        QVERIFY(info->contains(chars[4]));
    } else if (info->serviceUuid() ==
                QBluetoothUuid(QString("00001801-0000-1000-8000-00805f9b34fb"))) {
        qDebug() << "Verifying GATT Service";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 1);

        // Service Changed
        QString temp("00002a05-0000-1000-8000-00805f9b34fb");
        //this should really be readable according to GATT Service spec
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0xe));
        QCOMPARE(chars[0].properties(), QLowEnergyCharacteristic::Indicate);
        QCOMPARE(chars[0].value(), QByteArray());
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 1);
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0xf));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0000"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));
    } else if (info->serviceUuid() ==
                QBluetoothUuid(QString("0000180a-0000-1000-8000-00805f9b34fb"))) {
        qDebug() << "Verifying Device Information";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 9);

        // System ID
        QString temp("00002a23-0000-1000-8000-00805f9b34fb");
        //this should really be readable according to GATT Service spec
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x12));
        QCOMPARE(chars[0].properties(), QLowEnergyCharacteristic::Read);
//        Do not read the System ID as it is different for every device
//        QEXPECT_FAIL("", "The value is different on different devices", Continue);
//        QCOMPARE(chars[0].value(), QByteArray::fromHex("6e41ab0000296abc"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));
        QCOMPARE(chars[0].descriptors().count(), 0);

        // Model Number
        temp = QString("00002a24-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x14));
        QCOMPARE(chars[1].properties(), QLowEnergyCharacteristic::Read);
        QCOMPARE(chars[1].value(), QByteArray::fromHex("4e2e412e00"));
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));
        QCOMPARE(chars[1].descriptors().count(), 0);

        // Serial Number
        temp = QString("00002a25-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[2].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[2].handle(), QLowEnergyHandle(0x16));
        QCOMPARE(chars[2].properties(),
                 (QLowEnergyCharacteristic::Read));
        QCOMPARE(chars[2].value(), QByteArray::fromHex("4e2e412e00"));
        QVERIFY(chars[2].isValid());
        QVERIFY(info->contains(chars[2]));
        QCOMPARE(chars[2].descriptors().count(), 0);

        // Firmware Revision
        temp = QString("00002a26-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[3].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[3].handle(), QLowEnergyHandle(0x18));
        QCOMPARE(chars[3].properties(),
                 (QLowEnergyCharacteristic::Read));
        //FW rev. : 1.5 (Oct 23 2013)
        // Other revisions will fail here
        QCOMPARE(chars[3].value(), QByteArray::fromHex("312e3520284f637420323320323031332900"));
        QVERIFY(chars[3].isValid());
        QVERIFY(info->contains(chars[3]));
        QCOMPARE(chars[3].descriptors().count(), 0);

        // Hardware Revision
        temp = QString("00002a27-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[4].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[4].handle(), QLowEnergyHandle(0x1a));
        QCOMPARE(chars[4].properties(),
                 (QLowEnergyCharacteristic::Read));
        QCOMPARE(chars[4].value(), QByteArray::fromHex("4e2e412e00"));
        QVERIFY(chars[4].isValid());
        QVERIFY(info->contains(chars[4]));
        QCOMPARE(chars[4].descriptors().count(), 0);

        // Software Revision
        temp = QString("00002a28-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[5].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[5].handle(), QLowEnergyHandle(0x1c));
        QCOMPARE(chars[5].properties(),
                 (QLowEnergyCharacteristic::Read));
        QCOMPARE(chars[5].value(), QByteArray::fromHex("4e2e412e00"));
        QVERIFY(chars[5].isValid());
        QVERIFY(info->contains(chars[5]));
        QCOMPARE(chars[5].descriptors().count(), 0);

        // Manufacturer Name
        temp = QString("00002a29-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[6].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[6].handle(), QLowEnergyHandle(0x1e));
        QCOMPARE(chars[6].properties(),
                 (QLowEnergyCharacteristic::Read));
        QCOMPARE(chars[6].value(), QByteArray::fromHex("546578617320496e737472756d656e747300"));
        QVERIFY(chars[6].isValid());
        QVERIFY(info->contains(chars[6]));
        QCOMPARE(chars[6].descriptors().count(), 0);

        // IEEE
        temp = QString("00002a2a-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[7].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[7].handle(), QLowEnergyHandle(0x20));
        QCOMPARE(chars[7].properties(),
                 (QLowEnergyCharacteristic::Read));
        QCOMPARE(chars[7].value(), QByteArray::fromHex("fe006578706572696d656e74616c"));
        QVERIFY(chars[7].isValid());
        QVERIFY(info->contains(chars[7]));
        QCOMPARE(chars[7].descriptors().count(), 0);

        // PnP ID
        temp = QString("00002a50-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[8].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[8].handle(), QLowEnergyHandle(0x22));
        QCOMPARE(chars[8].properties(),
                 (QLowEnergyCharacteristic::Read));
        QCOMPARE(chars[8].value(), QByteArray::fromHex("010d0000001001"));
        QVERIFY(chars[8].isValid());
        QVERIFY(info->contains(chars[8]));
        QCOMPARE(chars[8].descriptors().count(), 0);
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000aa00-0451-4000-b000-000000000000"))) {
        qDebug() << "Verifying Temperature";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QVERIFY(chars.count() >= 2);

        // Temp Data
        QString temp("f000aa01-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x25));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Notify));
        QCOMPARE(chars[0].value(), QByteArray::fromHex("00000000"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);
        //descriptor checks
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x26));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0000"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x27));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        // value different in other revisions and test may fail
        QCOMPARE(chars[0].descriptors().at(1).value(),
                QByteArray::fromHex("54656d702e2044617461"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

        // Temp Config
        temp = QString("f000aa02-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x29));
        QCOMPARE(chars[1].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[1].value(), QByteArray::fromHex("00"));
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));

        QCOMPARE(chars[1].descriptors().count(), 1);
        //descriptor checks
        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x2a));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        // value different in other revisions and test may fail
        QCOMPARE(chars[1].descriptors().at(0).value(),
                QByteArray::fromHex("54656d702e20436f6e662e"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));


        //Temp Period (introduced by later firmware versions)
        if (chars.count() > 2) {
            temp = QString("f000aa03-0451-4000-b000-000000000000");
            QCOMPARE(chars[2].uuid(), QBluetoothUuid(temp));
            HANDLE_COMPARE(chars[2].handle(), QLowEnergyHandle(0x2c));
            QCOMPARE(chars[2].properties(),
                     (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
            QCOMPARE(chars[2].value(), QByteArray::fromHex("64"));
            QVERIFY(chars[2].isValid());
            QVERIFY(info->contains(chars[2]));

            QCOMPARE(chars[2].descriptors().count(), 1);
            //descriptor checks
            QCOMPARE(chars[2].descriptors().at(0).isValid(), true);
            HANDLE_COMPARE(chars[2].descriptors().at(0).handle(), QLowEnergyHandle(0x2d));
            QCOMPARE(chars[2].descriptors().at(0).uuid(),
                    QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
            QCOMPARE(chars[2].descriptors().at(0).type(),
                    QBluetoothUuid::CharacteristicUserDescription);
            QCOMPARE(chars[2].descriptors().at(0).value(),
                    QByteArray::fromHex("54656d702e20506572696f64"));
            QVERIFY(info->contains(chars[2].descriptors().at(0)));
        }
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("0000ffe0-0000-1000-8000-00805f9b34fb"))) {
        qDebug() << "Verifying Simple Keys";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 1);

        // Temp Data
        QString temp("0000ffe1-0000-1000-8000-00805f9b34fb");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x6b));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Notify));
        QCOMPARE(chars[0].value(), QByteArray());
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);
        //descriptor checks
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x6c));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0000"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x6d));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[0].descriptors().at(1).value(),
                QByteArray::fromHex("4b6579205072657373205374617465"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000aa10-0451-4000-b000-000000000000"))) {
        qDebug() << "Verifying Accelerometer";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 3);

        // Accel Data
        QString temp("f000aa11-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x30));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Notify));
        QCOMPARE(chars[0].value(), QByteArray::fromHex("000000"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);

        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x31));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                 QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                 QBluetoothUuid::ClientCharacteristicConfiguration);
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0000"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x32));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                 QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                 QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[0].descriptors().at(1).value(),
                 QByteArray::fromHex("416363656c2e2044617461"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

        // Accel Config
        temp = QString("f000aa12-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x34));
        QCOMPARE(chars[1].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[1].value(), QByteArray::fromHex("00"));
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));
        QCOMPARE(chars[1].descriptors().count(), 1);

        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x35));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                 QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                 QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[1].descriptors().at(0).value(),
                 QByteArray::fromHex("416363656c2e20436f6e662e"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));

        // Accel Period
        temp = QString("f000aa13-0451-4000-b000-000000000000");
        QCOMPARE(chars[2].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[2].handle(), QLowEnergyHandle(0x37));
        QCOMPARE(chars[2].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[2].value(), QByteArray::fromHex("64"));   // don't change it or set it to 0x64
        QVERIFY(chars[2].isValid());
        QVERIFY(info->contains(chars[2]));

        QCOMPARE(chars[2].descriptors().count(), 1);
        //descriptor checks
        QCOMPARE(chars[2].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[2].descriptors().at(0).handle(), QLowEnergyHandle(0x38));
        QCOMPARE(chars[2].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[2].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        // value different in other revisions and test may fail
        QCOMPARE(chars[2].descriptors().at(0).value(),
                QByteArray::fromHex("416363656c2e20506572696f64"));
        QVERIFY(info->contains(chars[2].descriptors().at(0)));
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000aa20-0451-4000-b000-000000000000"))) {
        qDebug() << "Verifying Humidity";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QVERIFY(chars.count() >= 2); //new firmware has more chars

        // Humidity Data
        QString temp("f000aa21-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x3b));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Notify));
        QCOMPARE(chars[0].value(), QByteArray::fromHex("00000000"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);
        //descriptor checks
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x3c));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0000"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x3d));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[0].descriptors().at(1).value(),
                QByteArray::fromHex("48756d69642e2044617461"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

        // Humidity Config
        temp = QString("f000aa22-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x3f));
        QCOMPARE(chars[1].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[1].value(), QByteArray::fromHex("00"));
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));

        QCOMPARE(chars[1].descriptors().count(), 1);
        //descriptor checks
        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x40));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[1].descriptors().at(0).value(),
                QByteArray::fromHex("48756d69642e20436f6e662e"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));

        if (chars.count() >= 3) {
            // New firmware new characteristic
            // Humidity Period
            temp = QString("f000aa23-0451-4000-b000-000000000000");
            QCOMPARE(chars[2].uuid(), QBluetoothUuid(temp));
            HANDLE_COMPARE(chars[2].handle(), QLowEnergyHandle(0x42));
            QCOMPARE(chars[2].properties(),
                     (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
            QCOMPARE(chars[2].value(), QByteArray::fromHex("64"));
            QVERIFY(chars[2].isValid());
            QVERIFY(info->contains(chars[2]));

            QCOMPARE(chars[2].descriptors().count(), 1);
            //descriptor checks
            QCOMPARE(chars[2].descriptors().at(0).isValid(), true);
            HANDLE_COMPARE(chars[2].descriptors().at(0).handle(), QLowEnergyHandle(0x43));
            QCOMPARE(chars[2].descriptors().at(0).uuid(),
                    QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
            QCOMPARE(chars[2].descriptors().at(0).type(),
                    QBluetoothUuid::CharacteristicUserDescription);
            QCOMPARE(chars[2].descriptors().at(0).value(),
                    QByteArray::fromHex("48756d69642e20506572696f64"));
            QVERIFY(info->contains(chars[2].descriptors().at(0)));
        }
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000aa30-0451-4000-b000-000000000000"))) {
        qDebug() << "Verifying Magnetometer";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 3);

        // Magnetometer Data
        QString temp("f000aa31-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x46));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Notify));
        QCOMPARE(chars[0].value(), QByteArray::fromHex("000000000000"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);

        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x47));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                 QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                 QBluetoothUuid::ClientCharacteristicConfiguration);
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0000"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x48));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                 QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                 QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[0].descriptors().at(1).value(),
                 QByteArray::fromHex("4d61676e2e2044617461"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

        // Magnetometer Config
        temp = QString("f000aa32-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x4a));
        QCOMPARE(chars[1].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[1].value(), QByteArray::fromHex("00"));
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));

        QCOMPARE(chars[1].descriptors().count(), 1);
        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x4b));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                 QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                 QBluetoothUuid::CharacteristicUserDescription);
        // value different in other revisions and test may fail
        QCOMPARE(chars[1].descriptors().at(0).value(),
                 QByteArray::fromHex("4d61676e2e20436f6e662e"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));

        // Magnetometer Period
        temp = QString("f000aa33-0451-4000-b000-000000000000");
        QCOMPARE(chars[2].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[2].handle(), QLowEnergyHandle(0x4d));
        QCOMPARE(chars[2].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[2].value(), QByteArray::fromHex("c8"));   // don't change it or set it to 0xc8
        QVERIFY(chars[2].isValid());
        QVERIFY(info->contains(chars[2]));

        QCOMPARE(chars[2].descriptors().count(), 1);
        QCOMPARE(chars[2].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[2].descriptors().at(0).handle(), QLowEnergyHandle(0x4e));
        QCOMPARE(chars[2].descriptors().at(0).uuid(),
                 QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[2].descriptors().at(0).type(),
                 QBluetoothUuid::CharacteristicUserDescription);
        // value different in other revisions and test may fail
        QCOMPARE(chars[2].descriptors().at(0).value(),
                 QByteArray::fromHex("4d61676e2e20506572696f64"));
        QVERIFY(info->contains(chars[2].descriptors().at(0)));
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000aa40-0451-4000-b000-000000000000"))) {
        qDebug() << "Verifying Pressure";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QVERIFY(chars.count() >= 3);

        // Pressure Data
        QString temp("f000aa41-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x51));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Notify));
        QCOMPARE(chars[0].value(), QByteArray::fromHex("00000000"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);
        //descriptor checks
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x52));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0000"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x53));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        // value different in other revisions and test may fail
        QCOMPARE(chars[0].descriptors().at(1).value(),
                QByteArray::fromHex("4261726f6d2e2044617461"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

        // Pressure Config
        temp = QString("f000aa42-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x55));
        QCOMPARE(chars[1].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[1].value(), QByteArray::fromHex("00"));
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));

        QCOMPARE(chars[1].descriptors().count(), 1);
        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x56));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[1].descriptors().at(0).value(),
                QByteArray::fromHex("4261726f6d2e20436f6e662e"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));

        //calibration and period characteristic are swapped, ensure we don't depend on their order
        QLowEnergyCharacteristic calibration, period;
        foreach (const QLowEnergyCharacteristic &ch, chars) {
            //find calibration characteristic
            if (ch.uuid() == QBluetoothUuid(QString("f000aa43-0451-4000-b000-000000000000")))
                calibration = ch;
            else if (ch.uuid() == QBluetoothUuid(QString("f000aa44-0451-4000-b000-000000000000")))
                period = ch;
        }

        if (calibration.isValid()) {
            // Pressure Calibration
            temp = QString("f000aa43-0451-4000-b000-000000000000");
            QCOMPARE(calibration.uuid(), QBluetoothUuid(temp));
            // value different in other revisions and test may fail
            HANDLE_COMPARE(calibration.handle(), QLowEnergyHandle(0x5b));
            QCOMPARE(calibration.properties(),
                     (QLowEnergyCharacteristic::Read));
            QCOMPARE(calibration.value(), QByteArray::fromHex("00000000000000000000000000000000"));   // don't change it
            QVERIFY(calibration.isValid());
            QVERIFY(info->contains(calibration));

            QCOMPARE(calibration.descriptors().count(), 2);
            //descriptor checks
            QCOMPARE(calibration.descriptors().at(0).isValid(), true);
            // value different in other revisions and test may fail
            HANDLE_COMPARE(calibration.descriptors().at(0).handle(), QLowEnergyHandle(0x5c));
            QCOMPARE(calibration.descriptors().at(0).uuid(),
                    QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
            QCOMPARE(calibration.descriptors().at(0).type(),
                    QBluetoothUuid::ClientCharacteristicConfiguration);
            QCOMPARE(calibration.descriptors().at(0).value(), QByteArray::fromHex("0000"));
            QVERIFY(info->contains(calibration.descriptors().at(0)));

            QCOMPARE(calibration.descriptors().at(1).isValid(), true);
            // value different in other revisions and test may fail
            HANDLE_COMPARE(calibration.descriptors().at(1).handle(), QLowEnergyHandle(0x5d));
            QCOMPARE(calibration.descriptors().at(1).uuid(),
                    QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
            QCOMPARE(calibration.descriptors().at(1).type(),
                    QBluetoothUuid::CharacteristicUserDescription);
            QCOMPARE(calibration.descriptors().at(1).value(),
                     QByteArray::fromHex("4261726f6d2e2043616c6962722e"));
            QVERIFY(info->contains(calibration.descriptors().at(1)));
        }

        if (period.isValid()) {
            // Period Calibration
            temp = QString("f000aa44-0451-4000-b000-000000000000");
            QCOMPARE(period.uuid(), QBluetoothUuid(temp));
            // value different in other revisions and test may fail
            HANDLE_COMPARE(period.handle(), QLowEnergyHandle(0x58));
            QCOMPARE(period.properties(),
                     (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
            QCOMPARE(period.value(), QByteArray::fromHex("64"));
            QVERIFY(period.isValid());
            QVERIFY(info->contains(period));

            QCOMPARE(period.descriptors().count(), 1);
            //descriptor checks
            QCOMPARE(period.descriptors().at(0).isValid(), true);
            // value different in other revisions and test may fail
            HANDLE_COMPARE(period.descriptors().at(0).handle(), QLowEnergyHandle(0x59));
            QCOMPARE(period.descriptors().at(0).uuid(),
                    QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
            QCOMPARE(period.descriptors().at(0).type(),
                    QBluetoothUuid::CharacteristicUserDescription);
            QCOMPARE(period.descriptors().at(0).value(),
                     QByteArray::fromHex("4261726f6d2e20506572696f64"));
            QVERIFY(info->contains(period.descriptors().at(0)));
        }
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000aa50-0451-4000-b000-000000000000"))) {
        qDebug() << "Verifying Gyroscope";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QVERIFY(chars.count() >= 2);

        // Gyroscope Data
        QString temp("f000aa51-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x60));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Notify));
        QCOMPARE(chars[0].value(), QByteArray::fromHex("000000000000"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);
        //descriptor checks
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x61));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0000"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x62));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        // value different in other revisions and test may fail
        QCOMPARE(chars[0].descriptors().at(1).value(),
                QByteArray::fromHex("4779726f2044617461"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

        // Gyroscope Config
        temp = QString("f000aa52-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x64));
        QCOMPARE(chars[1].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[1].value(), QByteArray::fromHex("00"));
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));

        QCOMPARE(chars[1].descriptors().count(), 1);
        //descriptor checks
        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x65));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[1].descriptors().at(0).value(),
                QByteArray::fromHex("4779726f20436f6e662e"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));

        // Gyroscope Period
        temp = QString("f000aa53-0451-4000-b000-000000000000");
        QCOMPARE(chars[2].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[2].handle(), QLowEnergyHandle(0x67));
        QCOMPARE(chars[2].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[2].value(), QByteArray::fromHex("64"));
        QVERIFY(chars[2].isValid());
        QVERIFY(info->contains(chars[2]));

        QCOMPARE(chars[2].descriptors().count(), 1);
        //descriptor checks
        QCOMPARE(chars[2].descriptors().at(0).isValid(), true);
        HANDLE_COMPARE(chars[2].descriptors().at(0).handle(), QLowEnergyHandle(0x68));
        QCOMPARE(chars[2].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[2].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[2].descriptors().at(0).value(),
                QByteArray::fromHex("4779726f20506572696f64"));
        QVERIFY(info->contains(chars[2].descriptors().at(0)));
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000aa60-0451-4000-b000-000000000000"))) {
        qDebug() << "Verifying Test Service";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 2);

        // Test Data
        QString temp("f000aa61-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x70));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Read));
        QCOMPARE(chars[0].value(), QByteArray::fromHex("3f00"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 1);
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x71));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[0].descriptors().at(0).value(),
                QByteArray::fromHex("546573742044617461"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        // Test Config
        temp = QString("f000aa62-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x73));
        QCOMPARE(chars[1].properties(),
                 (QLowEnergyCharacteristic::Read|QLowEnergyCharacteristic::Write));
        QCOMPARE(chars[1].value(), QByteArray::fromHex("00"));
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));

        QCOMPARE(chars[1].descriptors().count(), 1);
        //descriptor checks
        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x74));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[1].descriptors().at(0).value(),
                QByteArray::fromHex("5465737420436f6e666967"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000ccc0-0451-4000-b000-000000000000"))) {
        qDebug() << "Connection Control Service";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 3);

        //first characteristic
        QString temp("f000ccc1-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x77));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Notify|QLowEnergyCharacteristic::Read));
        // the connection control parameter change from platform to platform
        // better not test them here
        //QCOMPARE(chars[0].value(), QByteArray::fromHex("000000000000"));
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);
        //descriptor checks
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x78));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        // value different in other revisions and test may fail
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0100"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x79));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[0].descriptors().at(1).value(),
                QByteArray::fromHex("436f6e6e2e20506172616d73"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

        //second characteristic
        temp = QString("f000ccc2-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x7b));
        QCOMPARE(chars[1].properties(), QLowEnergyCharacteristic::Write);
        QCOMPARE(chars[1].value(), QByteArray());
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));

        QCOMPARE(chars[1].descriptors().count(), 1);
        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x7c));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[1].descriptors().at(0).value(),
                QByteArray::fromHex("436f6e6e2e20506172616d7320526571"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));

        //third characteristic
        temp = QString("f000ccc3-0451-4000-b000-000000000000");
        QCOMPARE(chars[2].uuid(), QBluetoothUuid(temp));
        HANDLE_COMPARE(chars[2].handle(), QLowEnergyHandle(0x7e));
        QCOMPARE(chars[2].properties(), QLowEnergyCharacteristic::Write);
        QCOMPARE(chars[2].value(), QByteArray());
        QVERIFY(chars[2].isValid());
        QVERIFY(info->contains(chars[2]));

        QCOMPARE(chars[2].descriptors().count(), 1);
        QCOMPARE(chars[2].descriptors().at(0).isValid(), true);
        HANDLE_COMPARE(chars[2].descriptors().at(0).handle(), QLowEnergyHandle(0x7f));
        QCOMPARE(chars[2].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[2].descriptors().at(0).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[2].descriptors().at(0).value(),
                QByteArray::fromHex("446973636f6e6e65637420526571"));
        QVERIFY(info->contains(chars[2].descriptors().at(0)));
    } else if (info->serviceUuid() ==
               QBluetoothUuid(QString("f000ffc0-0451-4000-b000-000000000000"))) {
        qDebug() << "Verifying OID Service";
        QList<QLowEnergyCharacteristic> chars = info->characteristics();
        QCOMPARE(chars.count(), 2);

        // first characteristic
        QString temp("f000ffc1-0451-4000-b000-000000000000");
        QCOMPARE(chars[0].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].handle(), QLowEnergyHandle(0x82));
        QCOMPARE(chars[0].properties(),
                (QLowEnergyCharacteristic::Notify|QLowEnergyCharacteristic::Write|QLowEnergyCharacteristic::WriteNoResponse));
        QCOMPARE(chars[0].value(), QByteArray());
        QVERIFY(chars[0].isValid());
        QVERIFY(info->contains(chars[0]));

        QCOMPARE(chars[0].descriptors().count(), 2);
        //descriptor checks
        QCOMPARE(chars[0].descriptors().at(0).isValid(), true);
        HANDLE_COMPARE(chars[0].descriptors().at(0).handle(), QLowEnergyHandle(0x83));
        QCOMPARE(chars[0].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[0].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        // value different in other revisions and test may fail
        QCOMPARE(chars[0].descriptors().at(0).value(), QByteArray::fromHex("0100"));
        QVERIFY(info->contains(chars[0].descriptors().at(0)));

        QCOMPARE(chars[0].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[0].descriptors().at(1).handle(), QLowEnergyHandle(0x84));
        QCOMPARE(chars[0].descriptors().at(1).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[0].descriptors().at(1).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[0].descriptors().at(1).value(),
                QByteArray::fromHex("496d67204964656e74696679"));
        QVERIFY(info->contains(chars[0].descriptors().at(1)));

        // second characteristic
        temp = QString("f000ffc2-0451-4000-b000-000000000000");
        QCOMPARE(chars[1].uuid(), QBluetoothUuid(temp));
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].handle(), QLowEnergyHandle(0x86));
        QCOMPARE(chars[1].properties(),
                 (QLowEnergyCharacteristic::Notify|QLowEnergyCharacteristic::Write|QLowEnergyCharacteristic::WriteNoResponse));
        QCOMPARE(chars[1].value(), QByteArray());
        QVERIFY(chars[1].isValid());
        QVERIFY(info->contains(chars[1]));

        QCOMPARE(chars[1].descriptors().count(), 2);
        //descriptor checks
        QCOMPARE(chars[1].descriptors().at(0).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].descriptors().at(0).handle(), QLowEnergyHandle(0x87));
        QCOMPARE(chars[1].descriptors().at(0).uuid(),
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));
        QCOMPARE(chars[1].descriptors().at(0).type(),
                QBluetoothUuid::ClientCharacteristicConfiguration);
        // value different in other revisions and test may fail
        QCOMPARE(chars[1].descriptors().at(0).value(), QByteArray::fromHex("0100"));
        QVERIFY(info->contains(chars[1].descriptors().at(0)));

        QCOMPARE(chars[1].descriptors().at(1).isValid(), true);
        // value different in other revisions and test may fail
        HANDLE_COMPARE(chars[1].descriptors().at(1).handle(), QLowEnergyHandle(0x88));
        QCOMPARE(chars[1].descriptors().at(1).uuid(),
                QBluetoothUuid(QBluetoothUuid::CharacteristicUserDescription));
        QCOMPARE(chars[1].descriptors().at(1).type(),
                QBluetoothUuid::CharacteristicUserDescription);
        QCOMPARE(chars[1].descriptors().at(1).value(),
                QByteArray::fromHex("496d6720426c6f636b"));
        QVERIFY(info->contains(chars[1].descriptors().at(1)));
    } else {
        QFAIL(QString("Service not found" + info->serviceUuid().toString()).toUtf8().constData());
    }
}

void tst_QLowEnergyController::tst_defaultBehavior()
{
    QList<QBluetoothAddress> foundAddresses;
    foreach (const QBluetoothHostInfo &info, QBluetoothLocalDevice::allDevices())
        foundAddresses.append(info.address());
    const QBluetoothAddress randomAddress("11:22:33:44:55:66");

    // Test automatic detection of local adapter
    QLowEnergyController controlDefaultAdapter(randomAddress);
    QCOMPARE(controlDefaultAdapter.remoteAddress(), randomAddress);
    QCOMPARE(controlDefaultAdapter.state(), QLowEnergyController::UnconnectedState);
    if (foundAddresses.isEmpty()) {
        QVERIFY(controlDefaultAdapter.localAddress().isNull());
    } else {
        QCOMPARE(controlDefaultAdapter.error(), QLowEnergyController::NoError);
        QVERIFY(controlDefaultAdapter.errorString().isEmpty());
        QVERIFY(foundAddresses.contains(controlDefaultAdapter.localAddress()));

        // unrelated uuids don't return valid service object
        // invalid service uuid
        QVERIFY(!controlDefaultAdapter.createServiceObject(
                    QBluetoothUuid()));
        // some random uuid
        QVERIFY(!controlDefaultAdapter.createServiceObject(
                    QBluetoothUuid(QBluetoothUuid::DeviceName)));
    }

    QCOMPARE(controlDefaultAdapter.services().count(), 0);

    // Test explicit local adapter
    if (!foundAddresses.isEmpty()) {
        QLowEnergyController controlExplicitAdapter(randomAddress,
                                                       foundAddresses[0]);
        QCOMPARE(controlExplicitAdapter.remoteAddress(), randomAddress);
        QCOMPARE(controlExplicitAdapter.localAddress(), foundAddresses[0]);
        QCOMPARE(controlExplicitAdapter.state(),
                 QLowEnergyController::UnconnectedState);
        QCOMPARE(controlExplicitAdapter.services().count(), 0);

        // unrelated uuids don't return valid service object
        // invalid service uuid
        QVERIFY(!controlExplicitAdapter.createServiceObject(
                    QBluetoothUuid()));
        // some random uuid
        QVERIFY(!controlExplicitAdapter.createServiceObject(
                    QBluetoothUuid(QBluetoothUuid::DeviceName)));
    }
}

void tst_QLowEnergyController::tst_writeCharacteristic()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_WINRT)
    QList<QBluetoothHostInfo> localAdapters = QBluetoothLocalDevice::allDevices();
    if (localAdapters.isEmpty())
        QSKIP("No local Bluetooth device found. Skipping test.");
#endif

    if (!remoteDeviceInfo.isValid())
        QSKIP("No remote BTLE device found. Skipping test.");
    QLowEnergyController control(remoteDeviceInfo);

    QCOMPARE(control.error(), QLowEnergyController::NoError);

    control.connectToDevice();
    {
        QTRY_IMPL(control.state() != QLowEnergyController::ConnectingState,
              30000);
    }

    if (control.state() == QLowEnergyController::ConnectingState
            || control.error() != QLowEnergyController::NoError) {
        // default BTLE backend forever hangs in ConnectingState
        QSKIP("Cannot connect to remote device");
    }

    QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
    QSignalSpy discoveryFinishedSpy(&control, SIGNAL(discoveryFinished()));
    QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
    control.discoverServices();
    QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 20000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveringState);
    QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveredState);

    const QBluetoothUuid testService(QString("f000aa60-0451-4000-b000-000000000000"));
    QList<QBluetoothUuid> uuids = control.services();
    QVERIFY(uuids.contains(testService));

    QLowEnergyService *service = control.createServiceObject(testService, this);
    QVERIFY(service);
    service->discoverDetails();
    QTRY_VERIFY_WITH_TIMEOUT(
        service->state() == QLowEnergyService::ServiceDiscovered, 30000);

    //test service described by http://processors.wiki.ti.com/index.php/SensorTag_User_Guide
    const QList<QLowEnergyCharacteristic> chars = service->characteristics();

    QLowEnergyCharacteristic dataChar;
    QLowEnergyCharacteristic configChar;
    for (int i = 0; i < chars.count(); i++) {
        if (chars[i].uuid() == QBluetoothUuid(QString("f000aa61-0451-4000-b000-000000000000")))
            dataChar = chars[i];
        else if (chars[i].uuid() == QBluetoothUuid(QString("f000aa62-0451-4000-b000-000000000000")))
            configChar = chars[i];
    }

    QVERIFY(dataChar.isValid());
    QVERIFY(!(dataChar.properties() & ~QLowEnergyCharacteristic::Read)); // only a read char
    QVERIFY(service->contains(dataChar));
    QVERIFY(configChar.isValid());
    QVERIFY(configChar.properties() & QLowEnergyCharacteristic::Write);
    QVERIFY(configChar.properties() & QLowEnergyCharacteristic::Read);
    QVERIFY(service->contains(configChar));

    QCOMPARE(dataChar.value(), QByteArray::fromHex("3f00"));
    QVERIFY(configChar.value() == QByteArray::fromHex("00")
            || configChar.value() == QByteArray::fromHex("81"));

    QSignalSpy writeSpy(service,
                        SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy readSpy(service,
                       SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)));

    // *******************************************
    // test writing of characteristic
    // enable Blinking LED if not already enabled
    if (configChar.value() != QByteArray::fromHex("81")) {
        service->writeCharacteristic(configChar, QByteArray::fromHex("81")); //0x81 blink LED D1
        QTRY_VERIFY_WITH_TIMEOUT(!writeSpy.isEmpty(), 10000);
        QCOMPARE(configChar.value(), QByteArray::fromHex("81"));
        QList<QVariant> firstSignalData = writeSpy.first();
        QLowEnergyCharacteristic signalChar = firstSignalData[0].value<QLowEnergyCharacteristic>();
        QByteArray signalValue = firstSignalData[1].toByteArray();

        QCOMPARE(signalValue, QByteArray::fromHex("81"));
        QVERIFY(signalChar == configChar);

        writeSpy.clear();

    }

    // test direct read of configChar
    QVERIFY(readSpy.isEmpty());
    service->readCharacteristic(configChar);
    QTRY_VERIFY_WITH_TIMEOUT(!readSpy.isEmpty(), 10000);
    QCOMPARE(configChar.value(), QByteArray::fromHex("81"));
    QCOMPARE(readSpy.count(), 1); //expect one characteristicRead signal
    {
        //verify the readCharacteristic()
        QList<QVariant> firstSignalData = readSpy.first();
        QLowEnergyCharacteristic signalChar = firstSignalData[0].value<QLowEnergyCharacteristic>();
        QByteArray signalValue = firstSignalData[1].toByteArray();

        QCOMPARE(signalValue, QByteArray::fromHex("81"));
        QCOMPARE(signalValue, configChar.value());
        QVERIFY(signalChar == configChar);
    }

    service->writeCharacteristic(configChar, QByteArray::fromHex("00")); //turn LED D1 off
    QTRY_VERIFY_WITH_TIMEOUT(!writeSpy.isEmpty(), 10000);
    QCOMPARE(configChar.value(), QByteArray::fromHex("00"));
    QList<QVariant> firstSignalData = writeSpy.first();
    QLowEnergyCharacteristic signalChar = firstSignalData[0].value<QLowEnergyCharacteristic>();
    QByteArray signalValue = firstSignalData[1].toByteArray();

    QCOMPARE(signalValue, QByteArray::fromHex("00"));
    QVERIFY(signalChar == configChar);

    // *******************************************
    // write wrong value -> error response required
    QSignalSpy errorSpy(service, SIGNAL(error(QLowEnergyService::ServiceError)));
    writeSpy.clear();
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(writeSpy.count(), 0);

    // write 2 byte value to 1 byte characteristic
    service->writeCharacteristic(configChar, QByteArray::fromHex("1111"));
    QTRY_VERIFY_WITH_TIMEOUT(!errorSpy.isEmpty(), 10000);
    QCOMPARE(errorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::CharacteristicWriteError);
    QCOMPARE(service->error(), QLowEnergyService::CharacteristicWriteError);
    QCOMPARE(writeSpy.count(), 0);
    QCOMPARE(configChar.value(), QByteArray::fromHex("00"));

    // *******************************************
    // write to read-only characteristic -> error
    errorSpy.clear();
    QCOMPARE(errorSpy.count(), 0);
    service->writeCharacteristic(dataChar, QByteArray::fromHex("ffff"));

    QTRY_VERIFY_WITH_TIMEOUT(!errorSpy.isEmpty(), 10000);
    QCOMPARE(errorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::CharacteristicWriteError);
    QCOMPARE(service->error(), QLowEnergyService::CharacteristicWriteError);
    QCOMPARE(writeSpy.count(), 0);
    QCOMPARE(dataChar.value(), QByteArray::fromHex("3f00"));


    control.disconnectFromDevice();

    // *******************************************
    // write value while disconnected -> error
    errorSpy.clear();
    QCOMPARE(errorSpy.count(), 0);
    service->writeCharacteristic(configChar, QByteArray::fromHex("ffff"));
    QTRY_VERIFY_WITH_TIMEOUT(!errorSpy.isEmpty(), 2000);
    QCOMPARE(errorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    QCOMPARE(service->error(), QLowEnergyService::OperationError);
    QCOMPARE(writeSpy.count(), 0);
    QCOMPARE(configChar.value(), QByteArray::fromHex("00"));

    // invalid characteristics still belong to their respective service
    QVERIFY(service->contains(configChar));
    QVERIFY(service->contains(dataChar));

    QVERIFY(!service->contains(QLowEnergyCharacteristic()));

    delete service;
}

void tst_QLowEnergyController::tst_readWriteDescriptor()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_WINRT)
    QList<QBluetoothHostInfo> localAdapters = QBluetoothLocalDevice::allDevices();
    if (localAdapters.isEmpty())
        QSKIP("No local Bluetooth device found. Skipping test.");
#endif

    if (!remoteDeviceInfo.isValid())
        QSKIP("No remote BTLE device found. Skipping test.");
    QLowEnergyController control(remoteDeviceInfo);

    // quick setup - more elaborate test is done by connect()
    control.connectToDevice();
    {
        QTRY_IMPL(control.state() != QLowEnergyController::ConnectingState,
              30000);
    }

    if (control.state() == QLowEnergyController::ConnectingState
            || control.error() != QLowEnergyController::NoError) {
        // default BTLE backend forever hangs in ConnectingState
        QSKIP("Cannot connect to remote device");
    }

    QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
    QSignalSpy discoveryFinishedSpy(&control, SIGNAL(discoveryFinished()));
    QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
    control.discoverServices();
    QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 20000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveringState);
    QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveredState);

    const QBluetoothUuid testService(QString("f000aa00-0451-4000-b000-000000000000"));
    QList<QBluetoothUuid> uuids = control.services();
    QVERIFY(uuids.contains(testService));

    QLowEnergyService *service = control.createServiceObject(testService, this);
    QVERIFY(service);
    service->discoverDetails();
    QTRY_VERIFY_WITH_TIMEOUT(
        service->state() == QLowEnergyService::ServiceDiscovered, 30000);

    // Temperature service described by
    // http://processors.wiki.ti.com/index.php/SensorTag_User_Guide

    // 1. Find temperature data characteristic
    const QLowEnergyCharacteristic tempData = service->characteristic(
                QBluetoothUuid(QStringLiteral("f000aa01-0451-4000-b000-000000000000")));
    const QLowEnergyCharacteristic tempConfig = service->characteristic(
                QBluetoothUuid(QStringLiteral("f000aa02-0451-4000-b000-000000000000")));

    if (!tempData.isValid()) {
        delete service;
        control.disconnectFromDevice();
        QSKIP("Cannot find temperature data characteristic of TI Sensor");
    }

    // 2. Find temperature data notification descriptor
    const QLowEnergyDescriptor notification = tempData.descriptor(
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));

    if (!notification.isValid()) {
        delete service;
        control.disconnectFromDevice();
        QSKIP("Cannot find temperature data notification of TI Sensor");
    }

    QCOMPARE(notification.value(), QByteArray::fromHex("0000"));
    QVERIFY(service->contains(notification));
    QVERIFY(service->contains(tempData));
    if (tempConfig.isValid()) {
        QVERIFY(service->contains(tempConfig));
        QCOMPARE(tempConfig.value(), QByteArray::fromHex("00"));
    }

    // 3. Test reading and writing to descriptor -> activate notifications
    QSignalSpy descWrittenSpy(service,
                        SIGNAL(descriptorWritten(QLowEnergyDescriptor,QByteArray)));
    QSignalSpy descReadSpy(service,
                        SIGNAL(descriptorRead(QLowEnergyDescriptor,QByteArray)));
    QSignalSpy charWrittenSpy(service,
                        SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy charChangedSpy(service,
                        SIGNAL(characteristicChanged(QLowEnergyCharacteristic,QByteArray)));

    QLowEnergyDescriptor signalDesc;
    QList<QVariant> firstSignalData;
    QByteArray signalValue;
    if (notification.value() != QByteArray::fromHex("0100")) {
        // enable notifications if not already done
        service->writeDescriptor(notification, QByteArray::fromHex("0100"));

        QTRY_VERIFY_WITH_TIMEOUT(!descWrittenSpy.isEmpty(), 3000);
        QCOMPARE(notification.value(), QByteArray::fromHex("0100"));
        firstSignalData = descWrittenSpy.first();
        signalDesc = firstSignalData[0].value<QLowEnergyDescriptor>();
        signalValue = firstSignalData[1].toByteArray();
        QCOMPARE(signalValue, QByteArray::fromHex("0100"));
        QVERIFY(notification == signalDesc);
        descWrittenSpy.clear();
    }

    // 4. Test reception of notifications
    // activate the temperature sensor if available
    if (tempConfig.isValid()) {
        service->writeCharacteristic(tempConfig, QByteArray::fromHex("01"));

        // first signal is confirmation of tempConfig write
        // subsequent signals are temp data updates
        QTRY_VERIFY_WITH_TIMEOUT(charWrittenSpy.count() == 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(charChangedSpy.count() >= 4, 10000);

        QCOMPARE(charWrittenSpy.count(), 1);
        QLowEnergyCharacteristic writtenChar = charWrittenSpy[0].at(0).value<QLowEnergyCharacteristic>();
        QByteArray writtenValue = charWrittenSpy[0].at(1).toByteArray();
        QCOMPARE(tempConfig, writtenChar);
        QCOMPARE(tempConfig.value(), writtenValue);
        QCOMPARE(writtenChar.value(), writtenValue);
        QCOMPARE(writtenValue, QByteArray::fromHex("01"));

        QList<QVariant> entry;
        for (int i = 0; i < charChangedSpy.count(); i++) {
            entry = charChangedSpy[i];
            const QLowEnergyCharacteristic ch = entry[0].value<QLowEnergyCharacteristic>();

            QCOMPARE(tempData, ch);

            //check last characteristic changed value matches the characteristics current value
            if (i == (charChangedSpy.count() - 1)) {
                writtenValue = entry[1].toByteArray();
                QCOMPARE(ch.value(), writtenValue);
                QCOMPARE(tempData.value(), writtenValue);
            }
        }

        service->writeCharacteristic(tempConfig, QByteArray::fromHex("00"));
    }

    // 5. Test reading and writing of/to descriptor -> deactivate notifications

    service->readDescriptor(notification);
    QTRY_VERIFY_WITH_TIMEOUT(!descReadSpy.isEmpty(), 3000);
    QCOMPARE(descReadSpy.count(), 1);
    firstSignalData = descReadSpy.first();
    signalDesc = firstSignalData[0].value<QLowEnergyDescriptor>();
    signalValue = firstSignalData[1].toByteArray();
    QCOMPARE(signalValue, notification.value());
    QCOMPARE(notification.value(), QByteArray::fromHex("0100"));
    descReadSpy.clear();


    service->writeDescriptor(notification, QByteArray::fromHex("0000"));
    // verify
    QTRY_VERIFY_WITH_TIMEOUT(!descWrittenSpy.isEmpty(), 3000);
    QCOMPARE(notification.value(), QByteArray::fromHex("0000"));
    firstSignalData = descWrittenSpy.first();
    signalDesc = firstSignalData[0].value<QLowEnergyDescriptor>();
    signalValue = firstSignalData[1].toByteArray();
    QCOMPARE(signalValue, QByteArray::fromHex("0000"));
    QVERIFY(notification == signalDesc);
    descWrittenSpy.clear();

    // test concurrent writeRequests
    // they need to be queued up
    service->writeDescriptor(notification,QByteArray::fromHex("0100"));
    service->writeDescriptor(notification, QByteArray::fromHex("0000"));
    service->writeDescriptor(notification, QByteArray::fromHex("0100"));
    service->writeDescriptor(notification, QByteArray::fromHex("0000"));
    QTRY_VERIFY_WITH_TIMEOUT(descWrittenSpy.count() == 4, 10000);

    QCOMPARE(notification.value(), QByteArray::fromHex("0000"));
    for (int i = 0; i < descWrittenSpy.count(); i++) {
        firstSignalData = descWrittenSpy.at(i);
        signalDesc = firstSignalData[0].value<QLowEnergyDescriptor>();
        signalValue = firstSignalData[1].toByteArray();
        if (i & 0x1) // odd
            QCOMPARE(signalValue, QByteArray::fromHex("0000"));
        else // even
            QCOMPARE(signalValue, QByteArray::fromHex("0100"));
        QVERIFY(notification == signalDesc);

    }

    // 5. Test reading and writing of/to descriptor -> deactivate notifications

    service->readDescriptor(notification);
    QTRY_VERIFY_WITH_TIMEOUT(!descReadSpy.isEmpty(), 3000);
    QCOMPARE(descReadSpy.count(), 1);
    firstSignalData = descReadSpy.first();
    signalDesc = firstSignalData[0].value<QLowEnergyDescriptor>();
    signalValue = firstSignalData[1].toByteArray();
    QCOMPARE(signalValue, notification.value());
    QCOMPARE(notification.value(), QByteArray::fromHex("0000"));
    descReadSpy.clear();

    descWrittenSpy.clear();

    // *******************************************
    // write wrong value -> error response required
    QSignalSpy errorSpy(service, SIGNAL(error(QLowEnergyService::ServiceError)));
    descWrittenSpy.clear();
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(descWrittenSpy.count(), 0);

    // write 4 byte value to 2 byte characteristic
    service->writeDescriptor(notification, QByteArray::fromHex("11112222"));
#ifdef Q_OS_MAC
    // On OS X/iOS we have a special method to set notify value,
    // it accepts only false/true and not
    // writing descriptors, there is only one way to find this error -
    // immediately intercept in LE controller and set the error.
    QVERIFY(!errorSpy.isEmpty());
#else
    QTRY_VERIFY_WITH_TIMEOUT(!errorSpy.isEmpty(), 30000);
#endif
    QCOMPARE(errorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::DescriptorWriteError);
    QCOMPARE(service->error(), QLowEnergyService::DescriptorWriteError);
    QCOMPARE(descWrittenSpy.count(), 0);
    QCOMPARE(notification.value(), QByteArray::fromHex("0000"));

    control.disconnectFromDevice();

    // *******************************************
    // write value while disconnected -> error
    errorSpy.clear();
    service->writeDescriptor(notification, QByteArray::fromHex("0100"));
    QTRY_VERIFY_WITH_TIMEOUT(!errorSpy.isEmpty(), 2000);
    QCOMPARE(errorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    QCOMPARE(service->error(), QLowEnergyService::OperationError);
    QCOMPARE(descWrittenSpy.count(), 0);
    QCOMPARE(notification.value(), QByteArray::fromHex("0000"));

    delete service;
}

/*
 * By default this test is skipped.
 *
 * Following tests are performed:
 * - encrypted read and discovery
 * - readCharacteristic() of values longer than MTU
 * - readCharacteristic() if values equal to MTU
 *
 * This test is semi manual as the test device environment is very specific.
 * A programmable BTLE device is required. Currently, the test requires
 * the CSR Dev Kit using the hr_sensor example.
 *
  * The following changes must be done to example to be able to fully
 * utilise the test:
 * 1.) gap_service_db.db -> UUID_DEVICE_NAME char - add FLAG_ENCR_R
 *      => tests encrypted read/discovery
 * 2.) dev_info_service_db.db -> UUID_DEVICE_INFO_MANUFACTURER_NAME
 *      =>  The default name "Cambridge Silicon Radio" must be changed
 *          to "Cambridge Silicon Radi" (new length 22)
 * 3.) revert change 1 above and redo test. This attempts to write a
 *     char that is readable w/o encryption but writeable with encryption
 *     => tests encryption code lines in writeCharacteristic()
 *     => otherwise the read encryption would have increased security level already
 *     => programmable CSR device must be reset before each run of this test
 *        (to undo the previous write)
 */
void tst_QLowEnergyController::tst_customProgrammableDevice()
{
    QSKIP("Skipping encryption");

    //Adjust the uuids and device address as see fit to match
    //values that match the current test environment
    //The target characteristic must be readble and writable
    //under encryption to test dynamic switching of security level
    QBluetoothAddress encryptedDevice(QString("00:02:5B:00:15:10"));
    QBluetoothUuid serviceUuid(QBluetoothUuid::GenericAccess);
    QBluetoothUuid characterristicUuid(QBluetoothUuid::DeviceName);

    QLowEnergyController control(encryptedDevice);
    QCOMPARE(control.error(), QLowEnergyController::NoError);

    control.connectToDevice();
    {
        QTRY_IMPL(control.state() != QLowEnergyController::ConnectingState,
              30000);
    }

    if (control.state() == QLowEnergyController::ConnectingState
            || control.error() != QLowEnergyController::NoError) {
        // default BTLE backend forever hangs in ConnectingState
        QSKIP("Cannot connect to remote device");
    }

    QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
    QSignalSpy discoveryFinishedSpy(&control, SIGNAL(discoveryFinished()));
    QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
    control.discoverServices();
    QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 20000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveringState);
    QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveredState);

    QList<QBluetoothUuid> uuids = control.services();
    QVERIFY(uuids.contains(serviceUuid));

    QLowEnergyService *service = control.createServiceObject(serviceUuid, this);
    QVERIFY(service);

    // 1.) discovery triggers read of device name char which is encrypted
    service->discoverDetails();
    QTRY_VERIFY_WITH_TIMEOUT(
        service->state() == QLowEnergyService::ServiceDiscovered, 30000);

    QLowEnergyCharacteristic encryptedChar = service->characteristic(
                                                    characterristicUuid);
    const QByteArray encryptedReference("CSR HR Sensor");
    QVERIFY(encryptedChar.isValid());
    QCOMPARE(encryptedChar.value(), encryptedReference);

    // 2.) read of encrypted characteristic
    //     => the discovery of the encrypted char above will have switched to
    //     encryption already.
    QSignalSpy encryptedReadSpy(service,
                                SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy encryptedErrorSpy(service,
                                 SIGNAL(error(QLowEnergyService::ServiceError)));
    service->readCharacteristic(encryptedChar);
    QTRY_VERIFY_WITH_TIMEOUT(!encryptedReadSpy.isEmpty(), 10000);
    QVERIFY(encryptedErrorSpy.isEmpty());
    QCOMPARE(encryptedReadSpy.count(), 1);
    QList<QVariant> entry = encryptedReadSpy[0];
    QVERIFY(entry[0].value<QLowEnergyCharacteristic>() == encryptedChar);
    QCOMPARE(entry[1].toByteArray(), encryptedReference);
    QCOMPARE(encryptedChar.value(), encryptedReference);

    // 3.) write to encrypted characteristic
    QSignalSpy encryptedWriteSpy(service,
                                 SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)));
    encryptedReadSpy.clear();
    encryptedErrorSpy.clear();
    const QByteArray newValue("ZZZ HR Sensor");
    service->writeCharacteristic(encryptedChar, newValue);
    QTRY_VERIFY_WITH_TIMEOUT(!encryptedWriteSpy.isEmpty(), 10000);
    QVERIFY(encryptedErrorSpy.isEmpty());
    QVERIFY(encryptedReadSpy.isEmpty());
    QCOMPARE(encryptedWriteSpy.count(), 1);
    entry = encryptedWriteSpy[0];
    QVERIFY(entry[0].value<QLowEnergyCharacteristic>() == encryptedChar);
    QCOMPARE(entry[1].toByteArray(), newValue);
    QCOMPARE(encryptedChar.value(), newValue);

    delete service;

    //change to Device Information service
    QVERIFY(uuids.contains(QBluetoothUuid::DeviceInformation));
    service = control.createServiceObject(QBluetoothUuid::DeviceInformation);
    QVERIFY(service);

    service->discoverDetails();
    QTRY_VERIFY_WITH_TIMEOUT(
        service->state() == QLowEnergyService::ServiceDiscovered, 30000);

    // 4.) read of software revision string which is longer than mtu
    //     tests readCharacteristic() including blob reads
    QSignalSpy readSpy(service,
                       SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy errorSpy(service,
                       SIGNAL(error(QLowEnergyService::ServiceError)));

    const QByteArray expectedSoftRev("Application version 2.3.0.0");
    QLowEnergyCharacteristic softwareRevChar
            = service->characteristic(QBluetoothUuid::SoftwareRevisionString);
    QVERIFY(softwareRevChar.isValid());
    QCOMPARE(softwareRevChar.value(), expectedSoftRev);

    service->readCharacteristic(softwareRevChar);
    QTRY_VERIFY_WITH_TIMEOUT(!readSpy.isEmpty(), 10000);
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(readSpy.count(), 1);
    entry = readSpy[0];
    QVERIFY(entry[0].value<QLowEnergyCharacteristic>() == softwareRevChar);
    QCOMPARE(entry[1].toByteArray(), expectedSoftRev);
    QCOMPARE(softwareRevChar.value(), expectedSoftRev);


    // 5.) read of manufacturer string which is exactly as long as single
    //     MTU size (assuming negotiated MTU is 23)
    //     => blob read test without blob being required
    //     => the read blob answer will have zero length

    readSpy.clear();

    // This assumes the manufacturer string was mondified via CSR SDK
    // see function description above
    const QByteArray expectedManufacturer("Cambridge Silicon Radi");
    QLowEnergyCharacteristic manufacturerChar = service->characteristic(
                QBluetoothUuid::ManufacturerNameString);
    QVERIFY(manufacturerChar.isValid());
    QCOMPARE(manufacturerChar.value(), expectedManufacturer);

    service->readCharacteristic(manufacturerChar);
    QTRY_VERIFY_WITH_TIMEOUT(!readSpy.isEmpty(), 10000);
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(readSpy.count(), 1);
    entry = readSpy[0];
    QVERIFY(entry[0].value<QLowEnergyCharacteristic>() == manufacturerChar);
    QCOMPARE(entry[1].toByteArray(), expectedManufacturer);
    QCOMPARE(manufacturerChar.value(), expectedManufacturer);

    delete service;
    control.disconnectFromDevice();
}


/*  1.) Test with undiscovered devices
        - read and write invalid char
    2.) Test with discovered devices
        - read non-readable char
        - write non-writable char
 */
void tst_QLowEnergyController::tst_errorCases()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_WINRT)
    QList<QBluetoothHostInfo> localAdapters = QBluetoothLocalDevice::allDevices();
    if (localAdapters.isEmpty())
        QSKIP("No local Bluetooth device found. Skipping test.");
#endif

    if (!remoteDeviceInfo.isValid())
        QSKIP("No remote BTLE device found. Skipping test.");
    QLowEnergyController control(remoteDeviceInfo);
    QCOMPARE(control.error(), QLowEnergyController::NoError);

    control.connectToDevice();
    {
        QTRY_IMPL(control.state() != QLowEnergyController::ConnectingState,
              30000);
    }

    if (control.state() == QLowEnergyController::ConnectingState
            || control.error() != QLowEnergyController::NoError) {
        // default BTLE backend forever hangs in ConnectingState
        QSKIP("Cannot connect to remote device");
    }

    QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
    QSignalSpy discoveryFinishedSpy(&control, SIGNAL(discoveryFinished()));
    QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
    control.discoverServices();
    QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 20000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveringState);
    QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveredState);


    // Setup required uuids
    const QBluetoothUuid irTemperaturServiceUuid(QStringLiteral("f000aa00-0451-4000-b000-000000000000"));
    const QBluetoothUuid irCharUuid(QString("f000aa01-0451-4000-b000-000000000000"));
    const QBluetoothUuid oadServiceUuid(QStringLiteral("f000ffc0-0451-4000-b000-000000000000"));
    const QBluetoothUuid oadCharUuid(QString("f000ffc1-0451-4000-b000-000000000000"));

    QVERIFY(control.services().contains(irTemperaturServiceUuid));
    QVERIFY(control.services().contains(oadServiceUuid));

    // Create service objects and basic tests
    QLowEnergyService *irService = control.createServiceObject(irTemperaturServiceUuid);
    QVERIFY(irService);
    QCOMPARE(irService->state(), QLowEnergyService::DiscoveryRequired);
    QVERIFY(irService->characteristics().isEmpty());
    QLowEnergyService *oadService = control.createServiceObject(oadServiceUuid);
    QVERIFY(oadService);
    QCOMPARE(oadService->state(), QLowEnergyService::DiscoveryRequired);
    QVERIFY(oadService->characteristics().isEmpty());

    QLowEnergyCharacteristic invalidChar;
    QLowEnergyDescriptor invalidDesc;

    QVERIFY(!irService->contains(invalidChar));
    QVERIFY(!irService->contains(invalidDesc));

    QSignalSpy irErrorSpy(irService, SIGNAL(error(QLowEnergyService::ServiceError)));
    QSignalSpy oadErrorSpy(oadService, SIGNAL(error(QLowEnergyService::ServiceError)));

    QSignalSpy irReadSpy(irService, SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy irWrittenSpy(irService, SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy irDescReadSpy(irService, SIGNAL(descriptorRead(QLowEnergyDescriptor,QByteArray)));
    QSignalSpy irDescWrittenSpy(irService, SIGNAL(descriptorWritten(QLowEnergyDescriptor,QByteArray)));

    QSignalSpy oadCharReadSpy(oadService, SIGNAL(descriptorRead(QLowEnergyDescriptor,QByteArray)));

    // ********************************************************
    // Test read/write to discovered service
    // with invalid characteristic & descriptor

    // discover IR Service
    irService->discoverDetails();
    QTRY_VERIFY_WITH_TIMEOUT(
        irService->state() == QLowEnergyService::ServiceDiscovered, 30000);
    QVERIFY(!irService->contains(invalidChar));
    QVERIFY(!irService->contains(invalidDesc));
    irErrorSpy.clear();

    // read invalid characteristic
    irService->readCharacteristic(invalidChar);
    QTRY_VERIFY_WITH_TIMEOUT(!irErrorSpy.isEmpty(), 5000);
    QCOMPARE(irErrorSpy.count(), 1);
    QVERIFY(irWrittenSpy.isEmpty());
    QVERIFY(irReadSpy.isEmpty());
    QCOMPARE(irErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    irErrorSpy.clear();

    // read invalid descriptor
    irService->readDescriptor(invalidDesc);
    QTRY_VERIFY_WITH_TIMEOUT(!irErrorSpy.isEmpty(), 5000);
    QCOMPARE(irErrorSpy.count(), 1);
    QVERIFY(irDescWrittenSpy.isEmpty());
    QVERIFY(irDescReadSpy.isEmpty());
    QCOMPARE(irErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    irErrorSpy.clear();

    // write invalid characteristic
    irService->writeCharacteristic(invalidChar, QByteArray("foo"));
    QTRY_VERIFY_WITH_TIMEOUT(!irErrorSpy.isEmpty(), 5000);
    QCOMPARE(irErrorSpy.count(), 1);
    QVERIFY(irWrittenSpy.isEmpty());
    QVERIFY(irReadSpy.isEmpty());
    QCOMPARE(irErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    irErrorSpy.clear();

    // write invalid descriptor
    irService->readDescriptor(invalidDesc);
    QTRY_VERIFY_WITH_TIMEOUT(!irErrorSpy.isEmpty(), 5000);
    QCOMPARE(irErrorSpy.count(), 1);
    QVERIFY(irDescWrittenSpy.isEmpty());
    QVERIFY(irDescReadSpy.isEmpty());
    QCOMPARE(irErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    irErrorSpy.clear();

    // ********************************************************
    // Test read/write to undiscovered service
    // with invalid characteristic & descriptor

    // read invalid characteristic
    oadService->readCharacteristic(invalidChar);
    QTRY_VERIFY_WITH_TIMEOUT(!oadErrorSpy.isEmpty(), 5000);
    QCOMPARE(oadErrorSpy.count(), 1);
    QCOMPARE(oadErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    oadErrorSpy.clear();

    // read invalid descriptor
    oadService->readDescriptor(invalidDesc);
    QTRY_VERIFY_WITH_TIMEOUT(!oadErrorSpy.isEmpty(), 5000);
    QCOMPARE(oadErrorSpy.count(), 1);
    QCOMPARE(oadErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    oadErrorSpy.clear();

    // write invalid characteristic
    oadService->writeCharacteristic(invalidChar, QByteArray("foo"));
    QTRY_VERIFY_WITH_TIMEOUT(!oadErrorSpy.isEmpty(), 5000);
    QCOMPARE(oadErrorSpy.count(), 1);
    QCOMPARE(oadErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    oadErrorSpy.clear();

    // write invalid descriptor
    oadService->readDescriptor(invalidDesc);
    QTRY_VERIFY_WITH_TIMEOUT(!oadErrorSpy.isEmpty(), 5000);
    QCOMPARE(oadErrorSpy.count(), 1);
    QCOMPARE(oadErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::OperationError);
    oadErrorSpy.clear();

    // ********************************************************
    // Write to non-writable char

    QLowEnergyCharacteristic nonWritableChar = irService->characteristic(irCharUuid);
    QVERIFY(nonWritableChar.isValid());
    // not writeable in any form
    QVERIFY(!(nonWritableChar.properties()
            & (QLowEnergyCharacteristic::Write|QLowEnergyCharacteristic::WriteNoResponse
               |QLowEnergyCharacteristic::WriteSigned)));
    irService->writeCharacteristic(nonWritableChar, QByteArray("ABCD"));
    QTRY_VERIFY_WITH_TIMEOUT(!irErrorSpy.isEmpty(), 5000);
    QVERIFY(irWrittenSpy.isEmpty());
    QVERIFY(irReadSpy.isEmpty());
    QCOMPARE(irErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::CharacteristicWriteError);
    irErrorSpy.clear();

    // ********************************************************
    // Write to non-writable desc
    // CharacteristicUserDescription is not writable

    QLowEnergyDescriptor nonWritableDesc = nonWritableChar.descriptor(
                QBluetoothUuid::CharacteristicUserDescription);
    QVERIFY(nonWritableDesc.isValid());
    irService->writeDescriptor(nonWritableDesc, QByteArray("ABCD"));
    QTRY_VERIFY_WITH_TIMEOUT(!irErrorSpy.isEmpty(), 5000);
    QVERIFY(irWrittenSpy.isEmpty());
    QVERIFY(irReadSpy.isEmpty());
    QCOMPARE(irErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::DescriptorWriteError);
    irErrorSpy.clear();


    // ********************************************************
    // Read non-readable char

    // discover OAD Service
    oadService->discoverDetails();
    QTRY_VERIFY_WITH_TIMEOUT(
        oadService->state() == QLowEnergyService::ServiceDiscovered, 30000);
    oadErrorSpy.clear();

    // Test reading
    QLowEnergyCharacteristic oadChar = oadService->characteristic(oadCharUuid);
    QVERIFY(oadChar.isValid());
    oadService->readCharacteristic(oadChar);
    QTRY_VERIFY_WITH_TIMEOUT(!oadErrorSpy.isEmpty(), 5000);
    QCOMPARE(oadErrorSpy.count(), 1);
    QVERIFY(oadCharReadSpy.isEmpty());
    QCOMPARE(oadErrorSpy[0].at(0).value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::CharacteristicReadError);
    oadErrorSpy.clear();

    delete irService;
    delete oadService;
    control.disconnectFromDevice();
}

/*
    Tests write without responses. We utilize the Over-The-Air image update
    service of the SensorTag.
 */
void tst_QLowEnergyController::tst_writeCharacteristicNoResponse()
{
#if !defined(Q_OS_MAC) && !defined(Q_OS_WINRT)
    QList<QBluetoothHostInfo> localAdapters = QBluetoothLocalDevice::allDevices();
    if (localAdapters.isEmpty())
        QSKIP("No local Bluetooth device found. Skipping test.");
#endif

    if (!remoteDeviceInfo.isValid())
        QSKIP("No remote BTLE device found. Skipping test.");
    QLowEnergyController control(remoteDeviceInfo);

    QCOMPARE(control.error(), QLowEnergyController::NoError);

    control.connectToDevice();
    {
        QTRY_IMPL(control.state() != QLowEnergyController::ConnectingState,
              30000);
    }

    if (control.state() == QLowEnergyController::ConnectingState
            || control.error() != QLowEnergyController::NoError) {
        // default BTLE backend forever hangs in ConnectingState
        QSKIP("Cannot connect to remote device");
    }

    QCOMPARE(control.state(), QLowEnergyController::ConnectedState);
    QSignalSpy discoveryFinishedSpy(&control, SIGNAL(discoveryFinished()));
    QSignalSpy stateSpy(&control, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
    control.discoverServices();
    QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 20000);
    QCOMPARE(stateSpy.count(), 2);
    QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveringState);
    QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
             QLowEnergyController::DiscoveredState);

    // The Over-The-Air update service uuid
    const QBluetoothUuid testService(QString("f000ffc0-0451-4000-b000-000000000000"));
    QList<QBluetoothUuid> uuids = control.services();
    QVERIFY(uuids.contains(testService));

    QLowEnergyService *service = control.createServiceObject(testService, this);
    QVERIFY(service);
    service->discoverDetails();
    QTRY_VERIFY_WITH_TIMEOUT(
        service->state() == QLowEnergyService::ServiceDiscovered, 30000);

    // 1. Get "Image Identity" and "Image Block" characteristic
    const QLowEnergyCharacteristic imageIdentityChar = service->characteristic(
                QBluetoothUuid(QString("f000ffc1-0451-4000-b000-000000000000")));
    const QLowEnergyCharacteristic imageBlockChar = service->characteristic(
                QBluetoothUuid(QString("f000ffc2-0451-4000-b000-000000000000")));
    QVERIFY(imageIdentityChar.isValid());
    QVERIFY(imageIdentityChar.properties() & QLowEnergyCharacteristic::Write);
    QVERIFY(imageIdentityChar.properties() & QLowEnergyCharacteristic::WriteNoResponse);
    QVERIFY(!(imageIdentityChar.properties() & QLowEnergyCharacteristic::Read)); //not readable
    QVERIFY(imageBlockChar.isValid());

    // 2. Get "Image Identity" notification descriptor
    const QLowEnergyDescriptor notification = imageIdentityChar.descriptor(
                QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration));

    if (!notification.isValid() || !imageIdentityChar.isValid()) {
        delete service;
        control.disconnectFromDevice();
        QSKIP("Cannot find OAD char/notification");
    }

    // 3. Enable notifications
    QSignalSpy descWrittenSpy(service,
                        SIGNAL(descriptorWritten(QLowEnergyDescriptor,QByteArray)));
    QSignalSpy charChangedSpy(service,
                        SIGNAL(characteristicChanged(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy charWrittenSpy(service,
                        SIGNAL(characteristicWritten(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy charReadSpy(service,
                        SIGNAL(characteristicRead(QLowEnergyCharacteristic,QByteArray)));
    QSignalSpy errorSpy(service,
                        SIGNAL(error(QLowEnergyService::ServiceError)));


    // by default the device enables the notification bit already
    // no need to enable it. If notifications fail to arrive the
    // platform must check default enabled notifications.
    if (notification.value() != QByteArray::fromHex("0100")) {
        service->writeDescriptor(notification, QByteArray::fromHex("0100"));
        QTRY_VERIFY_WITH_TIMEOUT(!descWrittenSpy.isEmpty(), 3000);
        QCOMPARE(notification.value(), QByteArray::fromHex("0100"));
        QList<QVariant> firstSignalData = descWrittenSpy.first();
        QLowEnergyDescriptor signalDesc = firstSignalData[0].value<QLowEnergyDescriptor>();
        QByteArray signalValue = firstSignalData[1].toByteArray();
        QCOMPARE(signalValue, QByteArray::fromHex("0100"));
        QVERIFY(notification == signalDesc);
        descWrittenSpy.clear();
    }

    QList<QVariant> entry;

    // Test direct read of non-readable characteristic
    QVERIFY(errorSpy.isEmpty());
    QVERIFY(charReadSpy.isEmpty());
    service->readCharacteristic(imageIdentityChar);
    QTRY_VERIFY_WITH_TIMEOUT(!errorSpy.isEmpty(), 10000);
    QCOMPARE(errorSpy.count(), 1); // should throw CharacteristicReadError
    QVERIFY(charReadSpy.isEmpty());
    entry = errorSpy[0];
    QCOMPARE(entry[0].value<QLowEnergyService::ServiceError>(),
             QLowEnergyService::CharacteristicReadError);

    // 4. Trigger image identity announcement (using traditional write)
    bool foundOneImage = false;

    // Image A
    // Write triggers a notification and write confirmation
    service->writeCharacteristic(imageIdentityChar, QByteArray::fromHex("0"));
    QTest::qWait(1000);
    QTRY_VERIFY_WITH_TIMEOUT(charChangedSpy.count() == 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(charWrittenSpy.count() == 1, 5000);

    // This is very SensorTag specific logic.
    // If the image block is empty the current firmware
    // does not even send a notification for imageIdentityChar
    // but for imageBlockChar

    entry = charChangedSpy[0];
    QLowEnergyCharacteristic first = entry[0].value<QLowEnergyCharacteristic>();
    QByteArray val1 = entry[1].toByteArray();
    if (val1.size() == 8) {
        QCOMPARE(imageIdentityChar, first);
        foundOneImage = true;
    } else {
        // we received a notification for imageBlockChar without explicitly
        // enabling them. This is caused by the device's default settings.
        QCOMPARE(imageBlockChar, first);
        qWarning() << "Invalid image A ident info";
    }

    entry = charWrittenSpy[0];
    QLowEnergyCharacteristic second = entry[0].value<QLowEnergyCharacteristic>();
    QByteArray val2 = entry[1].toByteArray();
    QCOMPARE(imageIdentityChar, second);
    QVERIFY(val2 == QByteArray::fromHex("0") || val2 == val1);

    // notifications on non-readable characteristics do not update cache
    QVERIFY(imageIdentityChar.value().isEmpty());
    QVERIFY(imageBlockChar.value().isEmpty());

    charChangedSpy.clear();
    charWrittenSpy.clear();

    // Image B
    service->writeCharacteristic(imageIdentityChar, QByteArray::fromHex("1"));
    QTest::qWait(1000);
    QTRY_VERIFY_WITH_TIMEOUT(charChangedSpy.count() == 1, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(charWrittenSpy.count() == 1, 5000);

    entry = charChangedSpy[0];
    first = entry[0].value<QLowEnergyCharacteristic>();
    val1 = entry[1].toByteArray();
    if (val1.size() == 8) {
        QCOMPARE(imageIdentityChar, first);
        foundOneImage = true;
    } else {
        // we received a notification for imageBlockChar without explicitly
        // enabling them. This is caused by the device's default settings.
        QCOMPARE(imageBlockChar, first);
        qWarning() << "Invalid image B ident info";
    }

    entry = charWrittenSpy[0];
    second = entry[0].value<QLowEnergyCharacteristic>();
    val2 = entry[1].toByteArray();
    QCOMPARE(imageIdentityChar, second);

    // notifications on non-readable characteristics do not update cache
    QVERIFY(imageIdentityChar.value().isEmpty());
    QVERIFY(imageBlockChar.value().isEmpty());

    /* Bluez resends the last confirmed write value, other platforms
     * send the value received by the change notification value.
     */
    qDebug() << "Image B(1):" << val1.toHex() << val2.toHex();
    QVERIFY(val2 == QByteArray::fromHex("1") || val2 == val1);

    QVERIFY2(foundOneImage, "The SensorTag doesn't have a valid image? (1)");

    // 5. Trigger image identity announcement (without response)
    charChangedSpy.clear();
    charWrittenSpy.clear();
    foundOneImage = false;

    // Image A
    service->writeCharacteristic(imageIdentityChar,
                                 QByteArray::fromHex("0"),
                                 QLowEnergyService::WriteWithoutResponse);

    // we only expect one signal (the notification but not the write confirmation)
    // Wait at least a second for a potential second signals
    QTest::qWait(1000);
    QTRY_VERIFY_WITH_TIMEOUT(charChangedSpy.count() == 1, 10000);

    entry = charChangedSpy[0];
    first = entry[0].value<QLowEnergyCharacteristic>();
    val1 = entry[1].toByteArray();

#ifdef Q_OS_ANDROID
    QEXPECT_FAIL("", "Android sends write confirmation when using WriteWithoutResponse",
                 Continue);
#endif
    QVERIFY(charWrittenSpy.isEmpty());
    if (val1.size() == 8) {
        QCOMPARE(first, imageIdentityChar);
        foundOneImage = true;
    } else {
        // we received a notification for imageBlockChar without explicitly
        // enabling them. This is caused by the device's default settings.
        QCOMPARE(imageBlockChar, first);
        qWarning() << "Image A not set?";
    }

    // notifications on non-readable characteristics do not update cache
    QVERIFY(imageIdentityChar.value().isEmpty());
    QVERIFY(imageBlockChar.value().isEmpty());

    charChangedSpy.clear();

    // Image B
    service->writeCharacteristic(imageIdentityChar,
                                 QByteArray::fromHex("1"),
                                 QLowEnergyService::WriteWithoutResponse);

    // we only expect one signal (the notification but not the write confirmation)
    // Wait at least a second for a potential second signals
    QTest::qWait(1000);
    QTRY_VERIFY_WITH_TIMEOUT(charChangedSpy.count() == 1, 10000);

    entry = charChangedSpy[0];
    first = entry[0].value<QLowEnergyCharacteristic>();
    val1 = entry[1].toByteArray();

#ifdef Q_OS_ANDROID
    QEXPECT_FAIL("", "Android sends write confirmation when using WriteWithoutResponse",
                 Continue);
#endif
    QVERIFY(charWrittenSpy.isEmpty());
    if (val1.size() == 8) {
        QCOMPARE(first, imageIdentityChar);
        foundOneImage = true;
    } else {
        // we received a notification for imageBlockChar without explicitly
        // enabling them. This is caused by the device's default settings.
        QCOMPARE(imageBlockChar, first);
        qWarning() << "Image B not set?";
    }

    // notifications on non-readable characteristics do not update cache
    QVERIFY(imageIdentityChar.value().isEmpty());
    QVERIFY(imageBlockChar.value().isEmpty());


    QVERIFY2(foundOneImage, "The SensorTag doesn't have a valid image? (2)");

    delete service;
    control.disconnectFromDevice();
}

QTEST_MAIN(tst_QLowEnergyController)

#include "tst_qlowenergycontroller.moc"
