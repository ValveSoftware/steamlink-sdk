/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>

#include <QDebug>

#include <qbluetoothaddress.h>
#include <qbluetoothdeviceinfo.h>
#include <qbluetoothlocaldevice.h>
#include <qbluetoothuuid.h>

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QBluetoothDeviceInfo::ServiceClasses)
Q_DECLARE_METATYPE(QBluetoothDeviceInfo::MajorDeviceClass)
Q_DECLARE_METATYPE(QBluetoothDeviceInfo::CoreConfiguration)

class tst_QBluetoothDeviceInfo : public QObject
{
    Q_OBJECT

public:
    tst_QBluetoothDeviceInfo();
    ~tst_QBluetoothDeviceInfo();

private slots:
    void initTestCase();

    void tst_construction_data();
    void tst_construction();

    void tst_assignment_data();
    void tst_assignment();

    void tst_serviceUuids();

    void tst_cached();
};

tst_QBluetoothDeviceInfo::tst_QBluetoothDeviceInfo()
{
}

tst_QBluetoothDeviceInfo::~tst_QBluetoothDeviceInfo()
{
}

void tst_QBluetoothDeviceInfo::initTestCase()
{
    qRegisterMetaType<QBluetoothDeviceInfo::ServiceClasses>("QBluetoothDeviceInfo::ServiceClasses");
    qRegisterMetaType<QBluetoothDeviceInfo::MajorDeviceClass>("QBluetoothDeviceInfo::MajorDeviceClass");
    // start Bluetooth if not started
    QBluetoothLocalDevice *device = new QBluetoothLocalDevice();
    device->powerOn();
    delete device;
}

void tst_QBluetoothDeviceInfo::tst_construction_data()
{
    QTest::addColumn<QBluetoothAddress>("address");
    QTest::addColumn<QString>("name");
    QTest::addColumn<quint32>("classOfDevice");
    QTest::addColumn<QBluetoothDeviceInfo::ServiceClasses>("serviceClasses");
    QTest::addColumn<QBluetoothDeviceInfo::MajorDeviceClass>("majorDeviceClass");
    QTest::addColumn<quint8>("minorDeviceClass");
    QTest::addColumn<QBluetoothDeviceInfo::CoreConfiguration>("coreConfiguration");

    // bits 12-8 Major
    // bits 7-2 Minor
    // bits 1-0 0

    QTest::newRow("0x000000 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000000)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::MiscellaneousDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedMiscellaneous)
        << QBluetoothDeviceInfo::BaseRateCoreConfiguration;
    QTest::newRow("0x000100 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000100)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::ComputerDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedComputer)
        << QBluetoothDeviceInfo::BaseRateCoreConfiguration;
    QTest::newRow("0x000104 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000104)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::ComputerDevice
        << quint8(QBluetoothDeviceInfo::DesktopComputer)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000118 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000118)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::ComputerDevice
        << quint8(QBluetoothDeviceInfo::WearableComputer)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000200 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device" << quint32(0x000200)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::PhoneDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedPhone)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000204 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000204)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::PhoneDevice
        << quint8(QBluetoothDeviceInfo::CellularPhone)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000214 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device" << quint32(0x000214)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::PhoneDevice
        << quint8(QBluetoothDeviceInfo::CommonIsdnAccessPhone)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000300 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000300)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::LANAccessDevice
        << quint8(QBluetoothDeviceInfo::NetworkFullService)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000320 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000320)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::LANAccessDevice
        << quint8(QBluetoothDeviceInfo::NetworkLoadFactorOne)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x0003E0 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x0003E0)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::LANAccessDevice
        << quint8(QBluetoothDeviceInfo::NetworkNoService)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000400 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000400)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::AudioVideoDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedAudioVideoDevice)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000448 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000448)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::AudioVideoDevice
        << quint8(QBluetoothDeviceInfo::GamingDevice)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000500 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000500)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::PeripheralDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedPeripheral)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x0005D8 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x0005D8)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::PeripheralDevice
        << quint8(QBluetoothDeviceInfo::KeyboardWithPointingDevicePeripheral | QBluetoothDeviceInfo::CardReaderPeripheral)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000600 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000600)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::ImagingDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedImagingDevice)
        << QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;
    QTest::newRow("0x000680 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000680)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::ImagingDevice
        << quint8(QBluetoothDeviceInfo::ImagePrinter)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    QTest::newRow("0x000700 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000700)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::WearableDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedWearableDevice)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    QTest::newRow("0x000714 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000714)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::WearableDevice
        << quint8(QBluetoothDeviceInfo::WearableGlasses)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    QTest::newRow("0x000800 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000800)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::ToyDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedToy)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    QTest::newRow("0x000814 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x000814)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::ToyDevice
        << quint8(QBluetoothDeviceInfo::ToyGame)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    QTest::newRow("0x001f00 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x001f00)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::NoService)
        << QBluetoothDeviceInfo::UncategorizedDevice
        << quint8(0)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    QTest::newRow("0x002000 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x002000)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::PositioningService)
        << QBluetoothDeviceInfo::MiscellaneousDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedMiscellaneous)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    QTest::newRow("0x100000 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0x100000)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::InformationService)
        << QBluetoothDeviceInfo::MiscellaneousDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedMiscellaneous)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    QTest::newRow("0xFFE000 COD") << QBluetoothAddress("000000000000") << "My Bluetooth Device"
        << quint32(0xFFE000)
        << QBluetoothDeviceInfo::ServiceClasses(QBluetoothDeviceInfo::AllServices)
        << QBluetoothDeviceInfo::MiscellaneousDevice
        << quint8(QBluetoothDeviceInfo::UncategorizedMiscellaneous)
        << QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
}

void tst_QBluetoothDeviceInfo::tst_construction()
{
    {
        QBluetoothDeviceInfo deviceInfo;

        QVERIFY(!deviceInfo.isValid());
        QVERIFY(deviceInfo.coreConfigurations()
                    == QBluetoothDeviceInfo::UnknownCoreConfiguration);
    }

    {
        QFETCH(QBluetoothAddress, address);
        QFETCH(QString, name);
        QFETCH(quint32, classOfDevice);
        QFETCH(QBluetoothDeviceInfo::ServiceClasses, serviceClasses);
        QFETCH(QBluetoothDeviceInfo::MajorDeviceClass, majorDeviceClass);
        QFETCH(quint8, minorDeviceClass);
        QFETCH(QBluetoothDeviceInfo::CoreConfiguration, coreConfiguration);

        QBluetoothDeviceInfo deviceInfo(address, name, classOfDevice);

        QVERIFY(deviceInfo.isValid());

        QCOMPARE(deviceInfo.address(), address);
        QCOMPARE(deviceInfo.name(), name);
        QCOMPARE(deviceInfo.serviceClasses(), serviceClasses);
        QCOMPARE(deviceInfo.majorDeviceClass(), majorDeviceClass);
        QCOMPARE(deviceInfo.minorDeviceClass(), minorDeviceClass);
        QCOMPARE(deviceInfo.coreConfigurations(), QBluetoothDeviceInfo::UnknownCoreConfiguration);

        deviceInfo.setCoreConfigurations(coreConfiguration);
        QCOMPARE(deviceInfo.coreConfigurations(), coreConfiguration);

        QBluetoothDeviceInfo copyInfo(deviceInfo);
        QVERIFY(copyInfo.isValid());

        QCOMPARE(copyInfo.address(), address);
        QCOMPARE(copyInfo.name(), name);
        QCOMPARE(copyInfo.serviceClasses(), serviceClasses);
        QCOMPARE(copyInfo.majorDeviceClass(), majorDeviceClass);
        QCOMPARE(copyInfo.minorDeviceClass(), minorDeviceClass);
        QCOMPARE(copyInfo.coreConfigurations(), coreConfiguration);
    }
}

void tst_QBluetoothDeviceInfo::tst_assignment_data()
{
    tst_construction_data();
}

void tst_QBluetoothDeviceInfo::tst_assignment()
{
    QFETCH(QBluetoothAddress, address);
    QFETCH(QString, name);
    QFETCH(quint32, classOfDevice);
    QFETCH(QBluetoothDeviceInfo::ServiceClasses, serviceClasses);
    QFETCH(QBluetoothDeviceInfo::MajorDeviceClass, majorDeviceClass);
    QFETCH(quint8, minorDeviceClass);
    QFETCH(QBluetoothDeviceInfo::CoreConfiguration, coreConfiguration);

    QBluetoothDeviceInfo deviceInfo(address, name, classOfDevice);
    deviceInfo.setCoreConfigurations(coreConfiguration);

    QVERIFY(deviceInfo.isValid());

    {
        QBluetoothDeviceInfo copyInfo = deviceInfo;

        QVERIFY(copyInfo.isValid());

        QCOMPARE(copyInfo.address(), address);
        QCOMPARE(copyInfo.name(), name);
        QCOMPARE(copyInfo.serviceClasses(), serviceClasses);
        QCOMPARE(copyInfo.majorDeviceClass(), majorDeviceClass);
        QCOMPARE(copyInfo.minorDeviceClass(), minorDeviceClass);
        QCOMPARE(copyInfo.coreConfigurations(), coreConfiguration);
    }

    {
        QBluetoothDeviceInfo copyInfo;

        QVERIFY(!copyInfo.isValid());

        copyInfo = deviceInfo;

        QVERIFY(copyInfo.isValid());

        QCOMPARE(copyInfo.address(), address);
        QCOMPARE(copyInfo.name(), name);
        QCOMPARE(copyInfo.serviceClasses(), serviceClasses);
        QCOMPARE(copyInfo.majorDeviceClass(), majorDeviceClass);
        QCOMPARE(copyInfo.minorDeviceClass(), minorDeviceClass);
        QCOMPARE(copyInfo.coreConfigurations(), coreConfiguration);
    }

    {
        QBluetoothDeviceInfo copyInfo1;
        QBluetoothDeviceInfo copyInfo2;

        QVERIFY(!copyInfo1.isValid());
        QVERIFY(!copyInfo2.isValid());

        copyInfo1 = copyInfo2 = deviceInfo;

        QVERIFY(copyInfo1.isValid());
        QVERIFY(copyInfo2.isValid());
        QVERIFY(QBluetoothDeviceInfo() != copyInfo1);

        QCOMPARE(copyInfo1.address(), address);
        QCOMPARE(copyInfo2.address(), address);
        QCOMPARE(copyInfo1.name(), name);
        QCOMPARE(copyInfo2.name(), name);
        QCOMPARE(copyInfo1.serviceClasses(), serviceClasses);
        QCOMPARE(copyInfo2.serviceClasses(), serviceClasses);
        QCOMPARE(copyInfo1.majorDeviceClass(), majorDeviceClass);
        QCOMPARE(copyInfo2.majorDeviceClass(), majorDeviceClass);
        QCOMPARE(copyInfo1.minorDeviceClass(), minorDeviceClass);
        QCOMPARE(copyInfo2.minorDeviceClass(), minorDeviceClass);
        QCOMPARE(copyInfo1.coreConfigurations(), coreConfiguration);
        QCOMPARE(copyInfo2.coreConfigurations(), coreConfiguration);
    }

    {
        QBluetoothDeviceInfo testDeviceInfo;
        QVERIFY(testDeviceInfo == QBluetoothDeviceInfo());
    }
}

void tst_QBluetoothDeviceInfo::tst_serviceUuids()
{
    QBluetoothDeviceInfo deviceInfo;
    QBluetoothDeviceInfo copyInfo = deviceInfo;

    QList<QBluetoothUuid> servicesList;
    servicesList.append(QBluetoothUuid::L2cap);
    servicesList.append(QBluetoothUuid::Rfcomm);
    QVERIFY(servicesList.count() > 0);

    deviceInfo.setServiceUuids(servicesList, QBluetoothDeviceInfo::DataComplete);
    QVERIFY(deviceInfo.serviceUuids().count() > 0);
    QVERIFY(deviceInfo != copyInfo);

    QVERIFY(deviceInfo.serviceUuidsCompleteness() == QBluetoothDeviceInfo::DataComplete);
}

void tst_QBluetoothDeviceInfo::tst_cached()
{
    QBluetoothDeviceInfo deviceInfo(QBluetoothAddress("AABBCCDDEEFF"),
        QString("My Bluetooth Device"), quint32(0x002000));
    QBluetoothDeviceInfo copyInfo = deviceInfo;

    QVERIFY(!deviceInfo.isCached());
    deviceInfo.setCached(true);
    QVERIFY(deviceInfo.isCached());
    QVERIFY(deviceInfo != copyInfo);

    deviceInfo.setCached(false);
    QVERIFY(!(deviceInfo.isCached()));
}

QTEST_MAIN(tst_QBluetoothDeviceInfo)

#include "tst_qbluetoothdeviceinfo.moc"
