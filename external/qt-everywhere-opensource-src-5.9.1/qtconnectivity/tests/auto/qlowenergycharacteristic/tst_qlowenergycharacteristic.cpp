/***************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited all rights reserved
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

#include <QBluetoothDeviceDiscoveryAgent>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyController>
#include <QBluetoothLocalDevice>

QT_USE_NAMESPACE

// This define must be set if the platform provides access to GATT handles
// otherwise it must not be defined. As of now the two supported platforms
// (Android and Bluez/Linux) provide access or some notion of it.
#define HANDLES_PROVIDED_BY_PLATFORM

#ifdef HANDLES_PROVIDED_BY_PLATFORM
#define HANDLE_VERIFY(stmt) QVERIFY(stmt)
#else
#define HANDLE_VERIFY(stmt)
#endif

class tst_QLowEnergyCharacteristic : public QObject
{
    Q_OBJECT

public:
    tst_QLowEnergyCharacteristic();
    ~tst_QLowEnergyCharacteristic();

protected slots:
    void deviceDiscovered(const QBluetoothDeviceInfo &info);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void tst_constructionDefault();
    void tst_assignCompare();

private:
    QList<QBluetoothDeviceInfo> remoteLeDevices;
    QLowEnergyController *globalControl;
    QLowEnergyService *globalService;
};

tst_QLowEnergyCharacteristic::tst_QLowEnergyCharacteristic() :
    globalControl(0), globalService(0)
{
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
    qRegisterMetaType<QBluetoothDeviceDiscoveryAgent::Error>();
}

tst_QLowEnergyCharacteristic::~tst_QLowEnergyCharacteristic()
{
}

void tst_QLowEnergyCharacteristic::initTestCase()
{
    if (QBluetoothLocalDevice::allDevices().isEmpty()) {
        qWarning("No remote device discovered.");
        return;
    }

    // start Bluetooth if not started
    QBluetoothLocalDevice device;
    device.powerOn();

    // find an arbitrary low energy device in vincinity
    // find an arbitrary service with characteristic
    QBluetoothDeviceDiscoveryAgent *devAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(devAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
            this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));

    QSignalSpy errorSpy(devAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)));
    QVERIFY(errorSpy.isValid());
    QVERIFY(errorSpy.isEmpty());

    QSignalSpy spy(devAgent, SIGNAL(finished()));
    QVERIFY(spy.isValid());
    QVERIFY(spy.isEmpty());

    devAgent->start();
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 50000);

    // find first service with descriptor
    QLowEnergyController *controller = 0;
    foreach (const QBluetoothDeviceInfo &remoteDevice, remoteLeDevices) {
        controller = new QLowEnergyController(remoteDevice, this);
        qDebug() << "Connecting to" << remoteDevice.name()
                 << remoteDevice.address() << remoteDevice.deviceUuid();
        controller->connectToDevice();
        QTRY_IMPL(controller->state() != QLowEnergyController::ConnectingState,
                  20000);
        if (controller->state() != QLowEnergyController::ConnectedState) {
            // any error and we skip
            delete controller;
            qDebug() << "Skipping device";
            continue;
        }

        QSignalSpy discoveryFinishedSpy(controller, SIGNAL(discoveryFinished()));
        QSignalSpy stateSpy(controller, SIGNAL(stateChanged(QLowEnergyController::ControllerState)));
        controller->discoverServices();
        QTRY_VERIFY_WITH_TIMEOUT(discoveryFinishedSpy.count() == 1, 10000);
        QCOMPARE(stateSpy.count(), 2);
        QCOMPARE(stateSpy.at(0).at(0).value<QLowEnergyController::ControllerState>(),
                 QLowEnergyController::DiscoveringState);
        QCOMPARE(stateSpy.at(1).at(0).value<QLowEnergyController::ControllerState>(),
                 QLowEnergyController::DiscoveredState);

        foreach (const QBluetoothUuid &leServiceUuid, controller->services()) {
            QLowEnergyService *leService = controller->createServiceObject(leServiceUuid, this);
            if (!leService)
                continue;

            leService->discoverDetails();
            QTRY_VERIFY_WITH_TIMEOUT(
                        leService->state() == QLowEnergyService::ServiceDiscovered, 10000);

            QList<QLowEnergyCharacteristic> chars = leService->characteristics();
            foreach (const QLowEnergyCharacteristic &ch, chars) {
                if (!ch.descriptors().isEmpty()) {
                    globalService = leService;
                    globalControl = controller;
                    qWarning() << "Found service with descriptor" << remoteDevice.address()
                               << globalService->serviceName() << globalService->serviceUuid();
                    break;
                }
            }

            if (globalControl)
                break;
            else
                delete leService;
        }

        if (globalControl)
            break;

        delete controller;
    }

    if (!globalControl) {
        qWarning() << "Test limited due to missing remote QLowEnergyDescriptor."
                   << "Please ensure the Bluetooth Low Energy device is advertising its services.";
    }
}

void tst_QLowEnergyCharacteristic::cleanupTestCase()
{
    if (globalControl)
        globalControl->disconnectFromDevice();
}

void tst_QLowEnergyCharacteristic::deviceDiscovered(const QBluetoothDeviceInfo &info)
{
    if (info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)
        remoteLeDevices.append(info);
}

void tst_QLowEnergyCharacteristic::tst_constructionDefault()
{
    QLowEnergyCharacteristic characteristic;
    QVERIFY(!characteristic.isValid());
    QCOMPARE(characteristic.value(), QByteArray());
    QVERIFY(characteristic.uuid().isNull());
    QVERIFY(characteristic.handle() == 0);
    QCOMPARE(characteristic.name(), QString());
    QCOMPARE(characteristic.descriptors().count(), 0);
    QCOMPARE(characteristic.descriptor(QBluetoothUuid()),
             QLowEnergyDescriptor());
    QCOMPARE(characteristic.descriptor(QBluetoothUuid(QBluetoothUuid::ClientCharacteristicConfiguration)),
             QLowEnergyDescriptor());
    QCOMPARE(characteristic.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration),
             QLowEnergyDescriptor());
    QCOMPARE(characteristic.properties(), QLowEnergyCharacteristic::Unknown);

    QLowEnergyCharacteristic copyConstructed(characteristic);
    QVERIFY(!copyConstructed.isValid());
    QCOMPARE(copyConstructed.value(), QByteArray());
    QVERIFY(copyConstructed.uuid().isNull());
    QVERIFY(copyConstructed.handle() == 0);
    QCOMPARE(copyConstructed.name(), QString());
    QCOMPARE(copyConstructed.descriptors().count(), 0);
    QCOMPARE(copyConstructed.properties(), QLowEnergyCharacteristic::Unknown);

    QVERIFY(copyConstructed == characteristic);
    QVERIFY(characteristic == copyConstructed);
    QVERIFY(!(copyConstructed != characteristic));
    QVERIFY(!(characteristic != copyConstructed));

    QLowEnergyCharacteristic assigned;

    QVERIFY(assigned == characteristic);
    QVERIFY(characteristic == assigned);
    QVERIFY(!(assigned != characteristic));
    QVERIFY(!(characteristic != assigned));

    assigned = characteristic;
    QVERIFY(!assigned.isValid());
    QCOMPARE(assigned.value(), QByteArray());
    QVERIFY(assigned.uuid().isNull());
    QVERIFY(assigned.handle() == 0);
    QCOMPARE(assigned.name(), QString());
    QCOMPARE(assigned.descriptors().count(), 0);
    QCOMPARE(assigned.properties(), QLowEnergyCharacteristic::Unknown);

    QVERIFY(assigned == characteristic);
    QVERIFY(characteristic == assigned);
    QVERIFY(!(assigned != characteristic));
    QVERIFY(!(characteristic != assigned));
}

void tst_QLowEnergyCharacteristic::tst_assignCompare()
{
    if (!globalService)
        QSKIP("No characteristic found.");

    QLowEnergyCharacteristic target;
    QVERIFY(!target.isValid());
    QCOMPARE(target.value(), QByteArray());
    QVERIFY(target.uuid().isNull());
    QVERIFY(target.handle() == 0);
    QCOMPARE(target.name(), QString());
    QCOMPARE(target.descriptors().count(), 0);
    QCOMPARE(target.properties(), QLowEnergyCharacteristic::Unknown);

    int indexWithDescriptor = -1;
    const QList<QLowEnergyCharacteristic> chars = globalService->characteristics();
    QVERIFY(!chars.isEmpty());
    for (int i = 0; i < chars.count(); i++) {
        const QLowEnergyCharacteristic specific =
                globalService->characteristic(chars[i].uuid());
        QVERIFY(specific.isValid());
        QCOMPARE(specific, chars[i]);
        if (chars[i].descriptors().count() > 0) {
            indexWithDescriptor = i;
            break;
        }
    }

    if (chars.isEmpty())
        QSKIP("No suitable characteristic found despite prior indication.");

    bool noDescriptors = (indexWithDescriptor == -1);
    if (noDescriptors)
        indexWithDescriptor = 0; // just choose one

    // test assignment operator
    target = chars[indexWithDescriptor];
    QVERIFY(target.isValid());
    QVERIFY(!target.name().isEmpty());
    HANDLE_VERIFY(target.handle() > 0);
    QVERIFY(!target.uuid().isNull());
    QVERIFY(target.properties() != QLowEnergyCharacteristic::Unknown);
    if (target.properties() & QLowEnergyCharacteristic::Read)
        QVERIFY(!target.value().isEmpty());
    if (!noDescriptors)
        QVERIFY(target.descriptors().count() > 0);

    QVERIFY(target == chars[indexWithDescriptor]);
    QVERIFY(chars[indexWithDescriptor] == target);
    QVERIFY(!(target != chars[indexWithDescriptor]));
    QVERIFY(!(chars[indexWithDescriptor] != target));

    QCOMPARE(target.isValid(), chars[indexWithDescriptor].isValid());
    QCOMPARE(target.name(), chars[indexWithDescriptor].name());
    QCOMPARE(target.handle(), chars[indexWithDescriptor].handle());
    QCOMPARE(target.uuid(), chars[indexWithDescriptor].uuid());
    QCOMPARE(target.value(), chars[indexWithDescriptor].value());
    QCOMPARE(target.properties(), chars[indexWithDescriptor].properties());
    QCOMPARE(target.descriptors().count(),
             chars[indexWithDescriptor].descriptors().count());
    for (int i = 0; i < target.descriptors().count(); i++) {
        const QLowEnergyDescriptor ref = chars[indexWithDescriptor].descriptors()[i];
        QCOMPARE(target.descriptors()[i].name(), ref.name());
        QCOMPARE(target.descriptors()[i].isValid(), ref.isValid());
        QCOMPARE(target.descriptors()[i].type(), ref.type());
        QCOMPARE(target.descriptors()[i].handle(), ref.handle());
        QCOMPARE(target.descriptors()[i].uuid(), ref.uuid());
        QCOMPARE(target.descriptors()[i].value(), ref.value());

        const QLowEnergyDescriptor ref2 = chars[indexWithDescriptor].descriptor(ref.uuid());
        QCOMPARE(ref, ref2);
    }

    // test copy constructor
    QLowEnergyCharacteristic copyConstructed(target);
    QCOMPARE(copyConstructed.isValid(), chars[indexWithDescriptor].isValid());
    QCOMPARE(copyConstructed.name(), chars[indexWithDescriptor].name());
    QCOMPARE(copyConstructed.handle(), chars[indexWithDescriptor].handle());
    QCOMPARE(copyConstructed.uuid(), chars[indexWithDescriptor].uuid());
    QCOMPARE(copyConstructed.value(), chars[indexWithDescriptor].value());
    QCOMPARE(copyConstructed.properties(), chars[indexWithDescriptor].properties());
    QCOMPARE(copyConstructed.descriptors().count(),
             chars[indexWithDescriptor].descriptors().count());

    QVERIFY(copyConstructed == target);
    QVERIFY(target == copyConstructed);
    QVERIFY(!(copyConstructed != target));
    QVERIFY(!(target != copyConstructed));

    // test invalidation
    QLowEnergyCharacteristic invalid;
    target = invalid;
    QVERIFY(!target.isValid());
    QCOMPARE(target.value(), QByteArray());
    QVERIFY(target.uuid().isNull());
    QVERIFY(target.handle() == 0);
    QCOMPARE(target.name(), QString());
    QCOMPARE(target.descriptors().count(), 0);
    QCOMPARE(target.properties(), QLowEnergyCharacteristic::Unknown);

    QVERIFY(invalid == target);
    QVERIFY(target == invalid);
    QVERIFY(!(invalid != target));
    QVERIFY(!(target != invalid));

    QVERIFY(!(chars[indexWithDescriptor] == target));
    QVERIFY(!(target == chars[indexWithDescriptor]));
    QVERIFY(chars[indexWithDescriptor] != target);
    QVERIFY(target != chars[indexWithDescriptor]);

    if (chars.count() >= 2) {
        // at least two characteristics
        QVERIFY(!(chars[0] == chars[1]));
        QVERIFY(!(chars[1] == chars[0]));
        QVERIFY(chars[0] != chars[1]);
        QVERIFY(chars[1] != chars[0]);
    }
}

QTEST_MAIN(tst_QLowEnergyCharacteristic)

#include "tst_qlowenergycharacteristic.moc"
