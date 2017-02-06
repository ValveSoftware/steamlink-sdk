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
#include <QVariant>
#include <QList>
#include <QLoggingCategory>

#include <qbluetoothaddress.h>
#include <qbluetoothdevicediscoveryagent.h>
#include <qbluetoothlocaldevice.h>

QT_USE_NAMESPACE

/*
 * Some parts of this test require a remote and discoverable Bluetooth
 * device. Setting the BT_TEST_DEVICE environment variable will
 * set the test up to fail if it cannot find a remote device.
 * BT_TEST_DEVICE should contain the address of the device the
 * test expects to find. Ensure that the device is running
 * in discovery mode.
 **/

// Maximum time to for bluetooth device scan
const int MaxScanTime = 5 * 60 * 1000;  // 5 minutes in ms

//Bluez needs at least 10s for a device discovery to be cancelled
const int MaxWaitForCancelTime = 15 * 1000;  // 15 seconds in ms

class tst_QBluetoothDeviceDiscoveryAgent : public QObject
{
    Q_OBJECT

public:
    tst_QBluetoothDeviceDiscoveryAgent();
    ~tst_QBluetoothDeviceDiscoveryAgent();

public slots:
    void deviceDiscoveryDebug(const QBluetoothDeviceInfo &info);
    void finished();

private slots:
    void initTestCase();

    void tst_properties();

    void tst_invalidBtAddress();

    void tst_startStopDeviceDiscoveries();

    void tst_deviceDiscovery_data();
    void tst_deviceDiscovery();

    void tst_discoveryTimeout();

    void tst_discoveryMethods();
private:
    int noOfLocalDevices;
    bool isBluez5Runtime = false;
};

tst_QBluetoothDeviceDiscoveryAgent::tst_QBluetoothDeviceDiscoveryAgent()
{
    QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
    qRegisterMetaType<QBluetoothDeviceDiscoveryAgent::Error>();
}

tst_QBluetoothDeviceDiscoveryAgent::~tst_QBluetoothDeviceDiscoveryAgent()
{
}

#ifdef QT_BLUEZ_BLUETOOTH
// This section was adopted from tst_qloggingcategory.cpp
QString logMessage;

QByteArray qMyMessageFormatString(QtMsgType type, const QMessageLogContext &context,
                                              const QString &str)
{
    QByteArray message;
    message.append(context.category);
    switch (type) {
    case QtDebugMsg:   message.append(".debug"); break;
    case QtInfoMsg:    message.append(".info"); break;
    case QtWarningMsg: message.append(".warning"); break;
    case QtCriticalMsg:message.append(".critical"); break;
    case QtFatalMsg:   message.append(".fatal"); break;
    }
    message.append(": ");
    message.append(qPrintable(str));

    return message.simplified();
}

static void myCustomMessageHandler(QtMsgType type,
                                   const QMessageLogContext &context,
                                   const QString &msg)
{
    logMessage = qMyMessageFormatString(type, context, msg);
}
#endif



void tst_QBluetoothDeviceDiscoveryAgent::initTestCase()
{
    qRegisterMetaType<QBluetoothDeviceInfo>();
    qRegisterMetaType<QBluetoothDeviceDiscoveryAgent::InquiryType>();

#ifdef QT_BLUEZ_BLUETOOTH
    // To distinguish Bluez 4 and 5 we peek into the debug output
    // of first Bluetooth ctor. It executes a runtime test and prints the result
    // as logging output. This avoids more complex runtime detection logic within this unit test.
    QtMessageHandler oldMessageHandler;
    oldMessageHandler = qInstallMessageHandler(myCustomMessageHandler);

    noOfLocalDevices = QBluetoothLocalDevice::allDevices().count();
    qInstallMessageHandler(oldMessageHandler);
    isBluez5Runtime = logMessage.contains(QStringLiteral("Bluez 5"));
    if (isBluez5Runtime)
        qDebug() << "BlueZ 5 runtime detected.";
#else
    noOfLocalDevices = QBluetoothLocalDevice::allDevices().count();
#endif

    if (!noOfLocalDevices)
        return;

    // turn on BT in case it is not on
    QBluetoothLocalDevice *device = new QBluetoothLocalDevice();
    if (device->hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        QSignalSpy hostModeSpy(device, SIGNAL(hostModeStateChanged(QBluetoothLocalDevice::HostMode)));
        QVERIFY(hostModeSpy.isEmpty());
        device->powerOn();
        int connectTime = 5000;  // ms
        while (hostModeSpy.count() < 1 && connectTime > 0) {
            QTest::qWait(500);
            connectTime -= 500;
        }
        QVERIFY(hostModeSpy.count() > 0);
    }
    QBluetoothLocalDevice::HostMode hostMode= device->hostMode();
    QVERIFY(hostMode == QBluetoothLocalDevice::HostConnectable
         || hostMode == QBluetoothLocalDevice::HostDiscoverable
         || hostMode == QBluetoothLocalDevice::HostDiscoverableLimitedInquiry);
    delete device;
}

void tst_QBluetoothDeviceDiscoveryAgent::tst_properties()
{
    QBluetoothDeviceDiscoveryAgent discoveryAgent;

    QCOMPARE(discoveryAgent.inquiryType(), QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry);
    discoveryAgent.setInquiryType(QBluetoothDeviceDiscoveryAgent::LimitedInquiry);
    QCOMPARE(discoveryAgent.inquiryType(), QBluetoothDeviceDiscoveryAgent::LimitedInquiry);
    discoveryAgent.setInquiryType(QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry);
    QCOMPARE(discoveryAgent.inquiryType(), QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry);
}

void tst_QBluetoothDeviceDiscoveryAgent::tst_invalidBtAddress()
{
    QBluetoothDeviceDiscoveryAgent *discoveryAgent = new QBluetoothDeviceDiscoveryAgent(QBluetoothAddress("11:11:11:11:11:11"));

    QCOMPARE(discoveryAgent->error(), QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError);
    discoveryAgent->start();
    QCOMPARE(discoveryAgent->isActive(), false);
    delete discoveryAgent;

    discoveryAgent = new QBluetoothDeviceDiscoveryAgent(QBluetoothAddress());
    QCOMPARE(discoveryAgent->error(), QBluetoothDeviceDiscoveryAgent::NoError);
    if (QBluetoothLocalDevice::allDevices().count() > 0) {
        discoveryAgent->start();
        QCOMPARE(discoveryAgent->isActive(), true);
    }
    delete discoveryAgent;
}

void tst_QBluetoothDeviceDiscoveryAgent::deviceDiscoveryDebug(const QBluetoothDeviceInfo &info)
{
    qDebug() << "Discovered device:" << info.address().toString() << info.name();
}

void tst_QBluetoothDeviceDiscoveryAgent::tst_startStopDeviceDiscoveries()
{
    QBluetoothDeviceDiscoveryAgent::InquiryType inquiryType = QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry;
    QBluetoothDeviceDiscoveryAgent discoveryAgent;

    QVERIFY(discoveryAgent.error() == discoveryAgent.NoError);
    QVERIFY(discoveryAgent.errorString().isEmpty());
    QVERIFY(!discoveryAgent.isActive());
    QVERIFY(discoveryAgent.discoveredDevices().isEmpty());

    QSignalSpy finishedSpy(&discoveryAgent, SIGNAL(finished()));
    QSignalSpy cancelSpy(&discoveryAgent, SIGNAL(canceled()));
    QSignalSpy errorSpy(&discoveryAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)));

    // Starting case 1: start-stop, expecting cancel signal
    discoveryAgent.setInquiryType(inquiryType);
    // we should have no errors at this point.
    QVERIFY(errorSpy.isEmpty());

    discoveryAgent.start();

    if (errorSpy.isEmpty()) {
        QVERIFY(discoveryAgent.isActive());
        QCOMPARE(discoveryAgent.errorString(), QString());
        QCOMPARE(discoveryAgent.error(), QBluetoothDeviceDiscoveryAgent::NoError);
    } else {
        QCOMPARE(noOfLocalDevices, 0);
        QVERIFY(!discoveryAgent.isActive());
        QVERIFY(!discoveryAgent.errorString().isEmpty());
        QVERIFY(discoveryAgent.error() != QBluetoothDeviceDiscoveryAgent::NoError);
        QSKIP("No local Bluetooth device available. Skipping remaining part of test.");
    }
    // cancel current request.
    discoveryAgent.stop();

    // Wait for up to MaxWaitForCancelTime for the cancel to finish
    int waitTime = MaxWaitForCancelTime;
    while (cancelSpy.count() == 0 && waitTime > 0) {
        QTest::qWait(100);
        waitTime-=100;
    }

    // we should not be active anymore
    QVERIFY(!discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(cancelSpy.count(), 1);
    cancelSpy.clear();
    // Starting case 2: start-start-stop, expecting cancel signal
    discoveryAgent.start();
    // we should be active now
    QVERIFY(discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    // start again. should this be error?
    discoveryAgent.start();
    QVERIFY(discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    // stop
    discoveryAgent.stop();

    // Wait for up to MaxWaitForCancelTime for the cancel to finish
    waitTime = MaxWaitForCancelTime;
    while (cancelSpy.count() == 0 && waitTime > 0) {
        QTest::qWait(100);
        waitTime-=100;
    }

    // we should not be active anymore
    QVERIFY(!discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    QVERIFY(cancelSpy.count() == 1);
    cancelSpy.clear();

    //  Starting case 3: stop
    discoveryAgent.stop();
    QVERIFY(!discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());

    // Don't expect finished signal and no error
    QVERIFY(finishedSpy.count() == 0);
    QVERIFY(discoveryAgent.error() == discoveryAgent.NoError);
    QVERIFY(discoveryAgent.errorString().isEmpty());

    /*
        Starting case 4: start-stop-start-stop:
        We are testing that two subsequent stop() calls reduce total number
        of cancel() signals to 1 if the true cancellation requires
        asynchronous function calls (signal consolidation); otherwise we
        expect 2x cancel() signal.

        Examples are:
            - Bluez4 (event loop needs to run for cancel)
            - Bluez5 (no event loop required)
    */

    bool immediateSignal = false;
    discoveryAgent.start();
    QVERIFY(discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    // cancel current request.
    discoveryAgent.stop();
    //should only have triggered cancel() if stop didn't involve the event loop
    if (cancelSpy.count() == 1) immediateSignal = true;

    // start a new one
    discoveryAgent.start();
    // we should be active now
    QVERIFY(discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    // stop
    discoveryAgent.stop();
    if (immediateSignal)
        QVERIFY(cancelSpy.count() == 2);

    // Wait for up to MaxWaitForCancelTime for the cancel to finish
    waitTime = MaxWaitForCancelTime;
    while (cancelSpy.count() == 0 && waitTime > 0) {
        QTest::qWait(100);
        waitTime-=100;
    }
    // we should not be active anymore
    QVERIFY(!discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    // should only have 1 cancel

    if (immediateSignal)
        QVERIFY(cancelSpy.count() == 2);
    else
        QVERIFY(cancelSpy.count() == 1);
    cancelSpy.clear();

    // Starting case 5: start-stop-start: expecting finished signal & no cancel
    discoveryAgent.start();
    QVERIFY(discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    // cancel current request.
    discoveryAgent.stop();
    // start a new one
    discoveryAgent.start();
    // we should be active now
    QVERIFY(discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());

    // Wait for up to MaxScanTime for the cancel to finish
    waitTime = MaxScanTime;
    while (finishedSpy.count() == 0 && waitTime > 0) {
        QTest::qWait(1000);
        waitTime-=1000;
    }

    // we should not be active anymore
    QVERIFY(!discoveryAgent.isActive());
    QVERIFY(errorSpy.isEmpty());
    // should only have 1 cancel
    QVERIFY(finishedSpy.count() == 1);

    // On OS X, stop is synchronous (signal will be emitted immediately).
    if (!immediateSignal)
        QVERIFY(cancelSpy.isEmpty());
}

void tst_QBluetoothDeviceDiscoveryAgent::finished()
{
    qDebug() << "Finished called";
}

void tst_QBluetoothDeviceDiscoveryAgent::tst_deviceDiscovery_data()
{
    QTest::addColumn<QBluetoothDeviceDiscoveryAgent::InquiryType>("inquiryType");

    QTest::newRow("general unlimited inquiry") << QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry;
    QTest::newRow("limited inquiry") << QBluetoothDeviceDiscoveryAgent::LimitedInquiry;
}

void tst_QBluetoothDeviceDiscoveryAgent::tst_deviceDiscovery()
{
    {
        QFETCH(QBluetoothDeviceDiscoveryAgent::InquiryType, inquiryType);

        //Run test in case of multiple Bluetooth adapters
        QBluetoothLocalDevice localDevice;
        //We will use default adapter if there is no other adapter
        QBluetoothAddress address = localDevice.address();
        int numberOfAdapters = (localDevice.allDevices()).size();
        QList<QBluetoothAddress> addresses;
        if (numberOfAdapters > 1) {

            for (int i=0; i < numberOfAdapters; i++) {
                addresses.append(((QBluetoothHostInfo)localDevice.allDevices().at(i)).address());
            }
            address = (QBluetoothAddress)addresses.at(0);
        }

        QBluetoothDeviceDiscoveryAgent discoveryAgent(address);
        QVERIFY(discoveryAgent.error() == discoveryAgent.NoError);
        QVERIFY(discoveryAgent.errorString().isEmpty());
        QVERIFY(!discoveryAgent.isActive());

        QVERIFY(discoveryAgent.discoveredDevices().isEmpty());

        QSignalSpy finishedSpy(&discoveryAgent, SIGNAL(finished()));
        QSignalSpy errorSpy(&discoveryAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)));
        QSignalSpy discoveredSpy(&discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)));
//        connect(&discoveryAgent, SIGNAL(finished()), this, SLOT(finished()));
//        connect(&discoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
//                this, SLOT(deviceDiscoveryDebug(QBluetoothDeviceInfo)));

        discoveryAgent.setInquiryType(inquiryType);
        discoveryAgent.start();
        if (!errorSpy.isEmpty()) {
            QCOMPARE(noOfLocalDevices, 0);
            QVERIFY(!discoveryAgent.isActive());
            QSKIP("No local Bluetooth device available. Skipping remaining part of test.");
        }

        QVERIFY(discoveryAgent.isActive());

        // Wait for up to MaxScanTime for the scan to finish
        int scanTime = MaxScanTime;
        while (finishedSpy.count() == 0 && scanTime > 0) {
            QTest::qWait(15000);
            scanTime -= 15000;
        }

        // verify that we are finished
        QVERIFY(!discoveryAgent.isActive());
        // stop
        discoveryAgent.stop();
        QVERIFY(!discoveryAgent.isActive());
        qDebug() << "Scan time left:" << scanTime;
        // Expect finished signal with no error
        QVERIFY(finishedSpy.count() == 1);
        QVERIFY(errorSpy.isEmpty());
        QVERIFY(discoveryAgent.error() == discoveryAgent.NoError);
        QVERIFY(discoveryAgent.errorString().isEmpty());

        // verify that the list is as big as the signals received.
        QVERIFY(discoveredSpy.count() == discoveryAgent.discoveredDevices().length());
        // verify that there really was some devices in the array

        const QString remote = qgetenv("BT_TEST_DEVICE");
        QBluetoothAddress remoteDevice;
        if (!remote.isEmpty()) {
            remoteDevice = QBluetoothAddress(remote);
            QVERIFY2(!remote.isNull(), "Expecting valid Bluetooth address to be passed via BT_TEST_DEVICE");
        } else {
            qWarning() << "Not using a remote device for testing. Set BT_TEST_DEVICE env to run extended tests involving a remote device";
        }

        if (!remoteDevice.isNull())
            QVERIFY(discoveredSpy.count() > 0);
        int counter = 0;
        // All returned QBluetoothDeviceInfo should be valid.
        while (!discoveredSpy.isEmpty()) {
            const QBluetoothDeviceInfo info =
                qvariant_cast<QBluetoothDeviceInfo>(discoveredSpy.takeFirst().at(0));
            QVERIFY(info.isValid());
            qDebug() << "Discovered device:" << info.address().toString() << info.name();

            if (numberOfAdapters > 1) {
                for (int i= 1; i < numberOfAdapters; i++) {
                    if (info.address().toString() == addresses[i].toString())
                        counter++;
                }
            }
        }
#if defined(Q_OS_IOS) || defined(Q_OS_WINRT)
        //On iOS/WinRT, we do not have access to the local device/adapter, numberOfAdapters is 0,
        //so we skip this test at all.
        QSKIP("iOS/WinRT: no local Bluetooth device available. Skipping remaining part of test.");
#endif

        //For multiple Bluetooth adapter do the check only for GeneralUnlimitedInquiry.
        if (!(inquiryType == QBluetoothDeviceDiscoveryAgent::LimitedInquiry))
            QVERIFY((numberOfAdapters-1) == counter);
    }
}


void tst_QBluetoothDeviceDiscoveryAgent::tst_discoveryTimeout()
{
    QBluetoothDeviceDiscoveryAgent agent;

    // check default values
#if defined(Q_OS_OSX) || defined(Q_OS_IOS) || defined(Q_OS_ANDROID) || defined(Q_OS_WINRT)
    QCOMPARE(agent.lowEnergyDiscoveryTimeout(), 25000);
    agent.setLowEnergyDiscoveryTimeout(-1); // negative ignored
    QCOMPARE(agent.lowEnergyDiscoveryTimeout(), 25000);
    agent.setLowEnergyDiscoveryTimeout(20000);
    QCOMPARE(agent.lowEnergyDiscoveryTimeout(), 20000);
#elif defined(QT_BLUEZ_BLUETOOTH)
    if (isBluez5Runtime) {
        QCOMPARE(agent.lowEnergyDiscoveryTimeout(), 20000);
        agent.setLowEnergyDiscoveryTimeout(-1); // negative ignored
        QCOMPARE(agent.lowEnergyDiscoveryTimeout(), 20000);
        agent.setLowEnergyDiscoveryTimeout(25000);
        QCOMPARE(agent.lowEnergyDiscoveryTimeout(), 25000);
    } else {
        QCOMPARE(agent.lowEnergyDiscoveryTimeout(), -1);
        agent.setLowEnergyDiscoveryTimeout(20000); // feature not supported -> ignored
        QCOMPARE(agent.lowEnergyDiscoveryTimeout(), -1);
    }
#else
    QCOMPARE(agent.lowEnergyDiscoveryTimeout(), -1);
    agent.setLowEnergyDiscoveryTimeout(20000); // feature not supported -> ignored
    QCOMPARE(agent.lowEnergyDiscoveryTimeout(), -1);
#endif
}

void tst_QBluetoothDeviceDiscoveryAgent::tst_discoveryMethods()
{
    const QBluetoothLocalDevice localDevice;
    if (localDevice.allDevices().size() != 1) {
        // On iOS it returns 0 but we still have working BT.
#ifndef Q_OS_IOS
        QSKIP("This test expects exactly one local device working");
#endif
    }

    const QBluetoothDeviceDiscoveryAgent::DiscoveryMethods
        supportedMethods = QBluetoothDeviceDiscoveryAgent::supportedDiscoveryMethods();

    QVERIFY(supportedMethods != QBluetoothDeviceDiscoveryAgent::NoMethod);

    QBluetoothDeviceDiscoveryAgent::DiscoveryMethod
        unsupportedMethods = QBluetoothDeviceDiscoveryAgent::NoMethod;
    QBluetoothDeviceInfo::CoreConfiguration
        expectedConfiguration = QBluetoothDeviceInfo::BaseRateAndLowEnergyCoreConfiguration;

    if (supportedMethods == QBluetoothDeviceDiscoveryAgent::ClassicMethod) {
        unsupportedMethods = QBluetoothDeviceDiscoveryAgent::LowEnergyMethod;
        expectedConfiguration = QBluetoothDeviceInfo::BaseRateCoreConfiguration;
    } else if (supportedMethods == QBluetoothDeviceDiscoveryAgent::LowEnergyMethod) {
        unsupportedMethods = QBluetoothDeviceDiscoveryAgent::ClassicMethod;
        expectedConfiguration = QBluetoothDeviceInfo::LowEnergyCoreConfiguration;
    }

    QBluetoothDeviceDiscoveryAgent agent;
    QSignalSpy finishedSpy(&agent, SIGNAL(finished()));
    QSignalSpy errorSpy(&agent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)));
    QSignalSpy discoveredSpy(&agent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)));

    // NoMethod - should just immediately return:
    agent.start(QBluetoothDeviceDiscoveryAgent::NoMethod);
    QCOMPARE(agent.error(), QBluetoothDeviceDiscoveryAgent::NoError);
    QVERIFY(!agent.isActive());
    QCOMPARE(finishedSpy.size(), 0);
    QCOMPARE(errorSpy.size(), 0);
    QCOMPARE(discoveredSpy.size(), 0);

    if (unsupportedMethods != QBluetoothDeviceDiscoveryAgent::NoMethod) {
        agent.start(unsupportedMethods);
        QCOMPARE(agent.error(), QBluetoothDeviceDiscoveryAgent::UnsupportedDiscoveryMethod);
        QVERIFY(!agent.isActive());
        QVERIFY(finishedSpy.isEmpty());
        QCOMPARE(errorSpy.size(), 1);
        errorSpy.clear();
        QVERIFY(discoveredSpy.isEmpty());
    }

    // Start discovery, probably both Classic and LE methods:
    agent.start(supportedMethods);
    QVERIFY(agent.isActive());
    QVERIFY(errorSpy.isEmpty());


#define RUN_DISCOVERY(maxTimeout, step, condition) \
    for (int scanTime = maxTimeout; (condition) && scanTime > 0; scanTime -= step) \
        QTest::qWait(step);

    // Wait for up to MaxScanTime for the scan to finish
    const int timeStep = 15000;
    RUN_DISCOVERY(MaxScanTime, timeStep, finishedSpy.isEmpty())

    QVERIFY(!agent.isActive());
    QVERIFY(errorSpy.size() <= 1);

    if (errorSpy.size()) {
        // For example, old iOS device could report it supports LE method,
        // but it actually does not.
        QVERIFY(supportedMethods == QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
        QCOMPARE(agent.error(), QBluetoothDeviceDiscoveryAgent::UnsupportedDiscoveryMethod);
    } else {
        QVERIFY(finishedSpy.count() == 1);
        QVERIFY(agent.error() == QBluetoothDeviceDiscoveryAgent::NoError);
        QVERIFY(agent.errorString().isEmpty());

        while (!discoveredSpy.isEmpty()) {
            const QBluetoothDeviceInfo info =
                qvariant_cast<QBluetoothDeviceInfo>(discoveredSpy.takeFirst().at(0));
            QVERIFY(info.isValid());
            QVERIFY(info.coreConfigurations() & expectedConfiguration);
        }
    }

    if (unsupportedMethods != QBluetoothDeviceDiscoveryAgent::NoMethod)
        return;

    // Both methods were reported as supported. We already tested them
    // above, now let's test first Classic then LE.
    finishedSpy.clear();
    errorSpy.clear();
    discoveredSpy.clear();

    agent.start(QBluetoothDeviceDiscoveryAgent::ClassicMethod);
    QVERIFY(agent.isActive());
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(agent.error(), QBluetoothDeviceDiscoveryAgent::NoError);

    RUN_DISCOVERY(MaxScanTime, timeStep, finishedSpy.isEmpty())

    QVERIFY(!agent.isActive());
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(finishedSpy.size(), 1);

    finishedSpy.clear();
    discoveredSpy.clear();

    agent.start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    QVERIFY(agent.isActive());
    QVERIFY(errorSpy.isEmpty());

    RUN_DISCOVERY(MaxScanTime, timeStep, finishedSpy.isEmpty() && errorSpy.isEmpty())

    QVERIFY(!agent.isActive());
    QVERIFY(errorSpy.size() <= 1);

    if (errorSpy.size()) {
        QCOMPARE(agent.error(), QBluetoothDeviceDiscoveryAgent::UnsupportedDiscoveryMethod);
        qDebug() << "QBluetoothDeviceDiscoveryAgent::supportedDiscoveryMethods is inaccurate"
                    " on your platform/with your device, LowEnergyMethod is not supported";
    } else {
        QCOMPARE(agent.error(), QBluetoothDeviceDiscoveryAgent::NoError);
        QCOMPARE(finishedSpy.size(), 1);
    }
}

QTEST_MAIN(tst_QBluetoothDeviceDiscoveryAgent)

#include "tst_qbluetoothdevicediscoveryagent.moc"
