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
#include <QLowEnergyDescriptor>
#include <QLowEnergyController>
#include <QBluetoothLocalDevice>

QT_USE_NAMESPACE

class tst_QLowEnergyDescriptor : public QObject
{
    Q_OBJECT

public:
    tst_QLowEnergyDescriptor();
    ~tst_QLowEnergyDescriptor();

protected slots:
    void deviceDiscovered(const QBluetoothDeviceInfo &info);

private slots:
    void initTestCase();
    void cleanupTestCase();
    void tst_constructionDefault();
    void tst_assignCompare();

private:
    QList<QBluetoothDeviceInfo> remoteLeDeviceInfos;
    QLowEnergyController *globalControl;
    QLowEnergyService *globalService;
};

tst_QLowEnergyDescriptor::tst_QLowEnergyDescriptor() :
    globalControl(0), globalService(0)
{
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
}

tst_QLowEnergyDescriptor::~tst_QLowEnergyDescriptor()
{
}

void tst_QLowEnergyDescriptor::initTestCase()
{
    if (QBluetoothLocalDevice::allDevices().isEmpty()) {
        qWarning("No remote device discovered.");

        return;
    }

    // start Bluetooth if not started
    QBluetoothLocalDevice device;
    device.powerOn();

    // find an arbitrary low energy device in vincinity
    // find an arbitrary service with descriptor

    QBluetoothDeviceDiscoveryAgent *devAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(devAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
            this, SLOT(deviceDiscovered(QBluetoothDeviceInfo)));

    QSignalSpy errorSpy(devAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)));
    QVERIFY(errorSpy.isValid());
    QVERIFY(errorSpy.isEmpty());

    QSignalSpy spy(devAgent, SIGNAL(finished()));
    // there should be no changes yet
    QVERIFY(spy.isValid());
    QVERIFY(spy.isEmpty());

    devAgent->start();
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() > 0, 50000);

    // find first service with descriptor
    QLowEnergyController *controller = 0;
    foreach (const QBluetoothDeviceInfo& remoteDeviceInfo, remoteLeDeviceInfos) {
        controller = new QLowEnergyController(remoteDeviceInfo, this);
        qDebug() << "Connecting to" << remoteDeviceInfo.address();
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
                    qWarning() << "Found service with descriptor" << remoteDeviceInfo.address()
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

void tst_QLowEnergyDescriptor::cleanupTestCase()
{
    if (globalControl)
        globalControl->disconnectFromDevice();
}

void tst_QLowEnergyDescriptor::deviceDiscovered(const QBluetoothDeviceInfo &info)
{
    if (info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration)
        remoteLeDeviceInfos.append(info);
}

void tst_QLowEnergyDescriptor::tst_constructionDefault()
{
    QLowEnergyDescriptor descriptor;
    QVERIFY(!descriptor.isValid());
    QCOMPARE(descriptor.value(), QByteArray());
    QVERIFY(descriptor.uuid().isNull());
    QVERIFY(descriptor.handle() == 0);
    QCOMPARE(descriptor.name(), QString());
    QCOMPARE(descriptor.type(), QBluetoothUuid::UnknownDescriptorType);

    QLowEnergyDescriptor copyConstructed(descriptor);
    QVERIFY(!copyConstructed.isValid());
    QCOMPARE(copyConstructed.value(), QByteArray());
    QVERIFY(copyConstructed.uuid().isNull());
    QVERIFY(copyConstructed.handle() == 0);
    QCOMPARE(copyConstructed.name(), QString());
    QCOMPARE(copyConstructed.type(), QBluetoothUuid::UnknownDescriptorType);

    QVERIFY(copyConstructed == descriptor);
    QVERIFY(descriptor == copyConstructed);
    QVERIFY(!(copyConstructed != descriptor));
    QVERIFY(!(descriptor != copyConstructed));

    QLowEnergyDescriptor assigned;

    QVERIFY(assigned == descriptor);
    QVERIFY(descriptor == assigned);
    QVERIFY(!(assigned != descriptor));
    QVERIFY(!(descriptor != assigned));

    assigned = descriptor;
    QVERIFY(!assigned.isValid());
    QCOMPARE(assigned.value(), QByteArray());
    QVERIFY(assigned.uuid().isNull());
    QVERIFY(assigned.handle() == 0);
    QCOMPARE(assigned.name(), QString());
    QCOMPARE(assigned.type(), QBluetoothUuid::UnknownDescriptorType);

    QVERIFY(assigned == descriptor);
    QVERIFY(descriptor == assigned);
    QVERIFY(!(assigned != descriptor));
    QVERIFY(!(descriptor != assigned));
}


void tst_QLowEnergyDescriptor::tst_assignCompare()
{
    //find the descriptor
    if (!globalService)
        QSKIP("No descriptor found.");

    QLowEnergyDescriptor target;
    QVERIFY(!target.isValid());
    QCOMPARE(target.type(), QBluetoothUuid::UnknownDescriptorType);
    QCOMPARE(target.name(), QString());
    QCOMPARE(target.handle(), QLowEnergyHandle(0));
    QCOMPARE(target.uuid(), QBluetoothUuid());
    QCOMPARE(target.value(), QByteArray());

    QList<QLowEnergyDescriptor> targets;
    const QList<QLowEnergyCharacteristic> chars = globalService->characteristics();
    foreach (const QLowEnergyCharacteristic &ch, chars) {
        if (!ch.descriptors().isEmpty()) {
           targets = ch.descriptors();
           break;
        }
    }

    if (targets.isEmpty())
        QSKIP("No descriptor found despite prior indication.");

    // test assignment operator
    target = targets.first();
    QVERIFY(target.isValid());
    QVERIFY(target.type() != QBluetoothUuid::UnknownDescriptorType);
    QVERIFY(!target.name().isEmpty());
    QVERIFY(target.handle() > 0);
    QVERIFY(!target.uuid().isNull());
    QVERIFY(!target.value().isEmpty());

    QVERIFY(target == targets.first());
    QVERIFY(targets.first() == target);
    QVERIFY(!(target != targets.first()));
    QVERIFY(!(targets.first() != target));

    QCOMPARE(target.isValid(), targets.first().isValid());
    QCOMPARE(target.type(), targets.first().type());
    QCOMPARE(target.name(), targets.first().name());
    QCOMPARE(target.handle(), targets.first().handle());
    QCOMPARE(target.uuid(), targets.first().uuid());
    QCOMPARE(target.value(), targets.first().value());

    // test copy constructor
    QLowEnergyDescriptor copyConstructed(target);
    QCOMPARE(copyConstructed.isValid(), targets.first().isValid());
    QCOMPARE(copyConstructed.type(), targets.first().type());
    QCOMPARE(copyConstructed.name(), targets.first().name());
    QCOMPARE(copyConstructed.handle(), targets.first().handle());
    QCOMPARE(copyConstructed.uuid(), targets.first().uuid());
    QCOMPARE(copyConstructed.value(), targets.first().value());

    QVERIFY(copyConstructed == target);
    QVERIFY(target == copyConstructed);
    QVERIFY(!(copyConstructed != target));
    QVERIFY(!(target != copyConstructed));

    // test invalidation
    QLowEnergyDescriptor invalid;
    target = invalid;
    QVERIFY(!target.isValid());
    QCOMPARE(target.value(), QByteArray());
    QVERIFY(target.uuid().isNull());
    QVERIFY(target.handle() == 0);
    QCOMPARE(target.name(), QString());
    QCOMPARE(target.type(), QBluetoothUuid::UnknownDescriptorType);

    QVERIFY(invalid == target);
    QVERIFY(target == invalid);
    QVERIFY(!(invalid != target));
    QVERIFY(!(target != invalid));

    QVERIFY(!(targets.first() == target));
    QVERIFY(!(target == targets.first()));
    QVERIFY(targets.first() != target);
    QVERIFY(target != targets.first());

    if (targets.count() >= 2) {
        QLowEnergyDescriptor second = targets[1];
        // at least two descriptors
        QVERIFY(!(targets.first() == second));
        QVERIFY(!(second == targets.first()));
        QVERIFY(targets.first() != second);
        QVERIFY(second != targets.first());
    }
}

QTEST_MAIN(tst_QLowEnergyDescriptor)

#include "tst_qlowenergydescriptor.moc"

