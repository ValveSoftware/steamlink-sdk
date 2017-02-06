/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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


#include <QtTest/QTest>
#include <Qt3DInput/private/utils_p.h>
#include <Qt3DInput/private/axis_p.h>
#include <Qt3DInput/qanalogaxisinput.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "qbackendnodetester.h"
#include "testdeviceproxy.h"

namespace {

class FakeBackendDevice : public Qt3DInput::QAbstractPhysicalDeviceBackendNode
{
    // QAbstractPhysicalDeviceBackendNode interface
public:
    FakeBackendDevice()
        : Qt3DInput::QAbstractPhysicalDeviceBackendNode(Qt3DCore::QBackendNode::ReadOnly)
    {}
    float axisValue(int) const { return 0.0f; }
    bool isButtonPressed(int) const { return false; }
};

class FakeInputDeviceIntegration : public Qt3DInput::QInputDeviceIntegration
{
public:
    explicit FakeInputDeviceIntegration(Qt3DInput::QAbstractPhysicalDevice *device)
        : Qt3DInput::QInputDeviceIntegration()
        , m_device(device)
    {}

    QVector<Qt3DCore::QAspectJobPtr> jobsToExecute(qint64) Q_DECL_OVERRIDE { return QVector<Qt3DCore::QAspectJobPtr>(); }
    Qt3DInput::QAbstractPhysicalDevice *createPhysicalDevice(const QString &) Q_DECL_OVERRIDE { return nullptr; }
    QVector<Qt3DCore::QNodeId> physicalDevices() const Q_DECL_OVERRIDE { return QVector<Qt3DCore::QNodeId>(); }
    QStringList deviceNames() const Q_DECL_OVERRIDE { return QStringList(); }

    Qt3DInput::QAbstractPhysicalDeviceBackendNode *physicalDevice(Qt3DCore::QNodeId deviceId) const Q_DECL_OVERRIDE
    {
        if (m_device->id() == deviceId)
            return new FakeBackendDevice();
        return nullptr;
    }

private:
    void onInitialize() Q_DECL_OVERRIDE {}
    Qt3DInput::QAbstractPhysicalDevice *m_device;
};


} // anonymous

class tst_Utils : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkPhysicalDeviceForValidAxisInput()
    {
        // GIVEN
        Qt3DInput::QAnalogAxisInput analogAxisInput;
        Qt3DInput::Input::InputHandler handler;
        TestPhysicalDevice testPhysicalDevice;
        FakeInputDeviceIntegration fakeIntegration(&testPhysicalDevice);

        analogAxisInput.setSourceDevice(&testPhysicalDevice);

        // WHEN -> Create backend AnalogAxisInput
        Qt3DInput::Input::AnalogAxisInput *backendAxisInput = handler.analogAxisInputManager()->getOrCreateResource(analogAxisInput.id());
        simulateInitialization(&analogAxisInput, backendAxisInput);

        // THEN
        QCOMPARE(backendAxisInput->axis(), analogAxisInput.axis());
        QCOMPARE(backendAxisInput->sourceDevice(), testPhysicalDevice.id());

        // Create backend integration
        handler.addInputDeviceIntegration(&fakeIntegration);

        // WHEN
        Qt3DInput::QAbstractPhysicalDeviceBackendNode *backendDevice = Qt3DInput::Input::Utils::physicalDeviceForInput(backendAxisInput, &handler);

        // THEN -> FakeIntegration returns something non null if it receives
        // the same id as the device used to create it
        QVERIFY(backendDevice != nullptr);
        delete backendDevice;
    }

    void checkProxyPhysicalDeviceForValidAxisInput()
    {
        // GIVEN
        Qt3DInput::QAnalogAxisInput analogAxisInput;
        Qt3DInput::Input::InputHandler handler;
        TestProxy testProxyPhysicalDevice;
        TestPhysicalDevice testPhysicalDevice;
        FakeInputDeviceIntegration fakeIntegration(&testPhysicalDevice);

        analogAxisInput.setSourceDevice(&testProxyPhysicalDevice);

        // WHEN -> Create backend AnalogAxisInput
        Qt3DInput::Input::AnalogAxisInput *backendAxisInput = handler.analogAxisInputManager()->getOrCreateResource(analogAxisInput.id());
        simulateInitialization(&analogAxisInput, backendAxisInput);

        // THEN
        QCOMPARE(backendAxisInput->axis(), analogAxisInput.axis());
        QCOMPARE(backendAxisInput->sourceDevice(), testProxyPhysicalDevice.id());

        // WHEN -> Create backend PhysicalProxiDevice
        Qt3DInput::Input::PhysicalDeviceProxy *backendProxyDevice = handler.physicalDeviceProxyManager()->getOrCreateResource(testProxyPhysicalDevice.id());
        backendProxyDevice->setManager(handler.physicalDeviceProxyManager());
        simulateInitialization(&testProxyPhysicalDevice, backendProxyDevice);
        backendProxyDevice->setDevice(&testPhysicalDevice);

        // THEN
        QCOMPARE(backendProxyDevice->physicalDeviceId(), testPhysicalDevice.id());

        // Create backend integration
        handler.addInputDeviceIntegration(&fakeIntegration);

        // WHEN
        Qt3DInput::QAbstractPhysicalDeviceBackendNode *backendDevice = Qt3DInput::Input::Utils::physicalDeviceForInput(backendAxisInput, &handler);

        // THEN -> FakeIntegration returns something non null if it receives
        // the same id as the device used to create it
        QVERIFY(backendDevice != nullptr);
        delete backendDevice;
    }

    void checkNoPhysicalDeviceForInvalidAxisInput()
    {
        // GIVEN
        Qt3DInput::QAnalogAxisInput analogAxisInput;
        Qt3DInput::Input::InputHandler handler;
        TestPhysicalDevice testPhysicalDevice;
        TestPhysicalDevice testPhysicalDevice2;
        FakeInputDeviceIntegration fakeIntegration(&testPhysicalDevice);

        analogAxisInput.setSourceDevice(&testPhysicalDevice2);

        // WHEN -> Create backend AnalogAxisInput
        Qt3DInput::Input::AnalogAxisInput *backendAxisInput = handler.analogAxisInputManager()->getOrCreateResource(analogAxisInput.id());
        simulateInitialization(&analogAxisInput, backendAxisInput);

        // THEN
        QCOMPARE(backendAxisInput->axis(), analogAxisInput.axis());
        QCOMPARE(backendAxisInput->sourceDevice(), testPhysicalDevice2.id());

        // Create backend integration
        handler.addInputDeviceIntegration(&fakeIntegration);

        // WHEN
        Qt3DInput::QAbstractPhysicalDeviceBackendNode *backendDevice = Qt3DInput::Input::Utils::physicalDeviceForInput(backendAxisInput, &handler);

        // THEN ->  FakeIntegration returns something non null if it receives
        // the same id as the device used to create it (testPhysicalDevice != testPhysicalDevice2)
        QVERIFY(backendDevice == nullptr);
    }

    void checkNoPysicalDeviceForInvalidProxyPhysicalDevice()
    {
        // GIVEN
        Qt3DInput::QAnalogAxisInput analogAxisInput;
        Qt3DInput::Input::InputHandler handler;
        TestProxy testProxyPhysicalDevice;
        TestPhysicalDevice testPhysicalDevice;
        TestPhysicalDevice testPhysicalDevice2;
        FakeInputDeviceIntegration fakeIntegration(&testPhysicalDevice);

        analogAxisInput.setSourceDevice(&testProxyPhysicalDevice);

        // WHEN -> Create backend AnalogAxisInput
        Qt3DInput::Input::AnalogAxisInput *backendAxisInput = handler.analogAxisInputManager()->getOrCreateResource(analogAxisInput.id());
        simulateInitialization(&analogAxisInput, backendAxisInput);

        // THEN
        QCOMPARE(backendAxisInput->axis(), analogAxisInput.axis());
        QCOMPARE(backendAxisInput->sourceDevice(), testProxyPhysicalDevice.id());

        // WHEN -> Create backend PhysicalProxiDevice
        Qt3DInput::Input::PhysicalDeviceProxy *backendProxyDevice = handler.physicalDeviceProxyManager()->getOrCreateResource(testProxyPhysicalDevice.id());
        backendProxyDevice->setManager(handler.physicalDeviceProxyManager());
        simulateInitialization(&testProxyPhysicalDevice, backendProxyDevice);
        backendProxyDevice->setDevice(&testPhysicalDevice2);

        // THEN
        QCOMPARE(backendProxyDevice->physicalDeviceId(), testPhysicalDevice2.id());

        // Create backend integration
        handler.addInputDeviceIntegration(&fakeIntegration);

        // WHEN
        Qt3DInput::QAbstractPhysicalDeviceBackendNode *backendDevice = Qt3DInput::Input::Utils::physicalDeviceForInput(backendAxisInput, &handler);

        // THEN ->  FakeIntegration returns something non null if it receives
        // the same id as the device used to create it (testPhysicalDevice != testPhysicalDevice2)
        QVERIFY(backendDevice == nullptr);
    }
};

QTEST_MAIN(tst_Utils)

#include "tst_utils.moc"
